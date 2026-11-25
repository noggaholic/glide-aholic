#pragma once
#include "glide/profile/wire_map.h"
#include "glide/msgconn_stream.h"
#include "glide/log.h"
#include <functional>

namespace glide {
enum class BootstrapResult {
    Send,
    Waiting,
    Progress,
    Complete,
    TimedOut,
    Malformed,
    Unknown,
    InvalidRecipe,
    ResourceFailure,
    Closed,
    ControlFailure
};
using BootstrapControl = std::function<bool(const GameMessage&)>;
class BootstrapRunner {
  public:
    // The recipe and wire profile must outlive the runner and remain unchanged.
    BootstrapRunner(const std::vector<BootstrapStep>& steps, const WireMap& wire, BootstrapControl control = {})
        : steps_(steps), wire_(wire), control_(std::move(control)) {}
    // Send output must be written successfully before calling Advance again. now is monotonic milliseconds.
    // Non-Send results leave output unchanged. Input following a matched confirmation is retained.
    BootstrapResult Advance(uint64_t now, Buffer& input, Buffer& output);
    uint32_t Remaining(uint64_t now) const;
    const char* Phase() const {
        return phase_;
    }

  private:
    BootstrapResult Finish(BootstrapResult result) {
        terminal_ = true;
        result_ = result;
        return result;
    }
    const std::vector<BootstrapStep>& steps_;
    const WireMap& wire_;
    BootstrapControl control_;
    Buffer deferred_;
    size_t index_ = 0;
    uint64_t waitStarted_ = 0;
    bool validated_ = false, waiting_ = false, terminal_ = false;
    BootstrapResult result_ = BootstrapResult::Waiting;
    const char* phase_ = "initialization";
};
// Uses an already-authorized, encrypted MsgConn connection. Leaves trailing GAME messages in input.
bool RunBootstrap(Socket& socket, MsgConnStream& connection, const WireMap& wire,
                  const std::vector<BootstrapStep>& steps, bool framed, Buffer& input, Log& log,
                  BootstrapControl control = {}, bool trace = false);
} // namespace glide
