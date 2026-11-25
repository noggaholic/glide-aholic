#include "glide/bootstrap_runner.h"
#include <algorithm>

namespace glide {
namespace {
constexpr size_t kMaximumInput = 2 * 1024 * 1024;
constexpr size_t kMaximumBatch = 60000;
const char* Failure(BootstrapResult result) {
    switch (result) {
    case BootstrapResult::TimedOut:
        return "confirmation timed out";
    case BootstrapResult::Closed:
        return "client closed during loading";
    case BootstrapResult::Malformed:
        return "malformed client message";
    case BootstrapResult::Unknown:
        return "unknown client message length";
    case BootstrapResult::InvalidRecipe:
        return "invalid loading recipe";
    case BootstrapResult::ResourceFailure:
        return "loading buffer limit or allocation failure";
    case BootstrapResult::ControlFailure:
        return "loading control could not be saved";
    default:
        return "loading failed";
    }
}
} // namespace
uint32_t BootstrapRunner::Remaining(uint64_t now) const {
    if (!waiting_ || terminal_ || index_ >= steps_.size())
        return 0;
    const uint64_t elapsed = now - waitStarted_;
    return elapsed >= steps_[index_].timeoutMs ? 0 : steps_[index_].timeoutMs - static_cast<uint32_t>(elapsed);
}
BootstrapResult BootstrapRunner::Advance(uint64_t now, Buffer& input, Buffer& output) {
    if (terminal_)
        return result_;
    if (!validated_) {
        if (steps_.empty())
            return Finish(BootstrapResult::InvalidRecipe);
        for (const auto& step : steps_) {
            if (!step.name || !step.name[0])
                return Finish(BootstrapResult::InvalidRecipe);
            if (step.kind == BootstrapStep::Kind::Send) {
                if (!step.payload.Size() || step.payload.Size() > kMaximumBatch)
                    return Finish(BootstrapResult::InvalidRecipe);
            } else if (step.kind != BootstrapStep::Kind::WaitFor || step.payload.Size() || !step.timeoutMs)
                return Finish(BootstrapResult::InvalidRecipe);
        }
        validated_ = true;
    }
    if (input.Size() > kMaximumInput)
        return Finish(BootstrapResult::ResourceFailure);
    size_t consumed = 0, processed = 0;
    while (index_ < steps_.size()) {
        const auto& step = steps_[index_];
        phase_ = step.name;
        if (step.kind == BootstrapStep::Kind::Send) {
            Buffer batch;
            do {
                const auto& payload = steps_[index_].payload;
                if (payload.Size() > kMaximumBatch - batch.Size())
                    break;
                if (!batch.Append(payload.View()))
                    return Finish(BootstrapResult::ResourceFailure);
                ++index_;
            } while (index_ < steps_.size() && steps_[index_].kind == BootstrapStep::Kind::Send);
            output = std::move(batch);
            return BootstrapResult::Send;
        }
        if (!waiting_) {
            waitStarted_ = now;
            waiting_ = true;
        }
        if (!Remaining(now))
            return Finish(BootstrapResult::TimedOut);
        GameMessage message;
        const auto status = wire_.DecodeGame(
            ByteView(input.Data() ? input.Data() + consumed : nullptr, input.Size() - consumed), message);
        if (status == DecodeStatus::Incomplete) {
            input.Consume(consumed);
            return BootstrapResult::Waiting;
        }
        if (status == DecodeStatus::Unknown)
            return Finish(BootstrapResult::Unknown);
        if (status != DecodeStatus::Complete || !message.consumed || message.consumed > input.Size() - consumed)
            return Finish(BootstrapResult::Malformed);
        if (message.action == GameAction::Close)
            return Finish(BootstrapResult::Closed);
        if (!wire_.AllowsDuringLoad(step.confirmation, message))
            return Finish(BootstrapResult::Malformed);
        const bool matched = wire_.BarrierMatches(step.confirmation, message);
        const bool deferred = !matched && wire_.DefersDuringLoad(step.confirmation, message);
        if (deferred && (message.consumed > kMaximumInput - deferred_.Size() ||
                         !deferred_.Append(ByteView(input.Data() + consumed, message.consumed))))
            return Finish(BootstrapResult::ResourceFailure);
        // Only controls accepted in this loading phase can change persistent state.
        if (!matched && !deferred && control_ && !control_(message))
            return Finish(BootstrapResult::ControlFailure);
        consumed += message.consumed;
        if (matched) {
            input.Consume(consumed);
            consumed = 0;
            waiting_ = false;
            ++index_;
        }
        if (++processed == 256) {
            input.Consume(consumed);
            return BootstrapResult::Progress;
        }
        // Other known messages are consumed during loading, as in the existing server.
        // Once matched, execute the next send step before inspecting any trailing confirmation.
    }
    if (deferred_.Size()) {
        Buffer pending;
        if (input.Size() > kMaximumInput - deferred_.Size() || !pending.Append(deferred_.View()) ||
            !pending.Append(input.View()))
            return Finish(BootstrapResult::ResourceFailure);
        input = std::move(pending);
    }
    return Finish(BootstrapResult::Complete);
}
bool RunBootstrap(Socket& socket, MsgConnStream& connection, const WireMap& wire,
                  const std::vector<BootstrapStep>& steps, bool framed, Buffer& input, Log& log,
                  BootstrapControl control, bool trace) {
    BootstrapRunner runner(steps, wire, std::move(control));
    Buffer output;
    std::string previousPhase;
    while (!socket.Stopped()) {
        const auto status = runner.Advance(GetTickCount64(), input, output);
        if (trace && previousPhase != runner.Phase()) {
            previousPhase = runner.Phase();
            log.Write("game", "Loading phase: " + previousPhase);
        }
        if (status == BootstrapResult::Progress)
            continue;
        if (status == BootstrapResult::Send) {
            if (!connection.Send(output.View(), framed))
                return false;
            continue;
        }
        if (status == BootstrapResult::Complete) {
            log.Write("game", "World entry messages completed");
            return true;
        }
        if (status != BootstrapResult::Waiting) {
            if (trace && input.Size()) {
                static const char digits[] = "0123456789abcdef";
                std::string prefix;
                const size_t size = (std::min)(input.Size(), size_t(16));
                for (size_t i = 0; i < size; ++i) {
                    prefix.push_back(digits[input.Data()[i] >> 4]);
                    prefix.push_back(digits[input.Data()[i] & 15]);
                }
                log.Write("game", "Loading rejected input prefix=" + prefix);
            }
            log.Write("game", std::string(runner.Phase()) + ": " + Failure(status));
            return false;
        }
        const uint32_t remaining = runner.Remaining(GetTickCount64());
        if (!remaining)
            continue;
        if (connection.HasBufferedInput() || socket.WaitReadable((std::min)(remaining, 100u))) {
            uint8_t bytes[8192];
            size_t received = 0;
            if (!connection.Read(bytes, sizeof(bytes), received) || received > kMaximumInput - input.Size() ||
                !input.Append(ByteView(bytes, received)))
                return false;
            if (trace) {
                static const char digits[] = "0123456789abcdef";
                std::string prefix;
                const size_t size = (std::min)(received, size_t(16));
                for (size_t i = 0; i < size; ++i) {
                    prefix.push_back(digits[bytes[i] >> 4]);
                    prefix.push_back(digits[bytes[i] & 15]);
                }
                log.Write("game", "Loading input bytes=" + std::to_string(received) + " prefix=" + prefix);
            }
        }
    }
    return false;
}
} // namespace glide
