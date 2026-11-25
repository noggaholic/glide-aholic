#include "supervisor.h"
#include <algorithm>

namespace glide {
namespace {
class Handle {
  public:
    explicit Handle(HANDLE handle) : handle_(handle) {}
    ~Handle() {
        if (handle_ && handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }
    HANDLE Get() const {
        return handle_;
    }

  private:
    HANDLE handle_;
};
enum class Operation { Connect, Read, Write };
bool Io(HANDLE pipe, Operation operation, void* bytes, DWORD size, DWORD& count, HANDLE stop, DWORD timeout) {
    Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.Get())
        return false;
    OVERLAPPED pending = {};
    pending.hEvent = event.Get();
    count = 0;
    BOOL result = FALSE;
    if (operation == Operation::Connect)
        result = ConnectNamedPipe(pipe, &pending);
    else if (operation == Operation::Read)
        result = ReadFile(pipe, bytes, size, &count, &pending);
    else
        result = WriteFile(pipe, bytes, size, &count, &pending);
    if (result)
        return operation == Operation::Connect || GetOverlappedResult(pipe, &pending, &count, FALSE) != FALSE;
    const DWORD failure = GetLastError();
    if (operation == Operation::Connect && failure == ERROR_PIPE_CONNECTED)
        return true;
    if (failure != ERROR_IO_PENDING)
        return false;
    const HANDLE events[] = {event.Get(), stop};
    const DWORD waited = WaitForMultipleObjects(stop ? 2u : 1u, events, FALSE, timeout);
    if (waited != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &pending);
        GetOverlappedResult(pipe, &pending, &count,
                            TRUE); // Never release pending buffers before cancellation completes.
        return false;
    }
    return GetOverlappedResult(pipe, &pending, &count, FALSE) != FALSE;
}
DWORD Remaining(ULONGLONG deadline) {
    const ULONGLONG now = GetTickCount64();
    return now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
}
} // namespace
SupervisorPipe::~SupervisorPipe() {
    Stop();
}
std::wstring SupervisorPipe::PipeName(const std::wstring& identity) {
    return L"\\\\.\\pipe\\GlideAholic-" + identity;
}
bool SupervisorPipe::Start(const LaunchRequest& owner, Handler handler, std::string& error, bool* existing) {
    if (existing)
        *existing = false;
    error = "Cannot start supervisor channel";
    if (mutex_ || !handler || !owner.Identity(identity_, error))
        return false;
    // Object existence elects one owner; no cross-thread mutex acquisition/release is required.
    mutex_ = CreateMutexW(nullptr, FALSE, (L"Local\\GlideAholicSupervisor-" + identity_).c_str());
    const DWORD mutexError = GetLastError();
    if (!mutex_ || mutexError == ERROR_ALREADY_EXISTS) {
        if (existing && mutex_ && mutexError == ERROR_ALREADY_EXISTS)
            *existing = true;
        error = "A supervisor already owns this installation, build and server";
        Stop();
        return false;
    }
    pipe_ = CreateNamedPipeW(PipeName(identity_).c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                             65536, 65536, 0, nullptr);
    stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (pipe_ == INVALID_HANDLE_VALUE || !stop_) {
        error = "Cannot create local supervisor pipe";
        Stop();
        return false;
    }
    handler_ = std::move(handler);
    thread_ = CreateThread(nullptr, 0, Thread, this, 0, nullptr);
    if (!thread_) {
        error = "Cannot start supervisor worker";
        Stop();
        return false;
    }
    error.clear();
    return true;
}
void SupervisorPipe::Stop() {
    if (stop_)
        SetEvent(stop_);
    if (thread_) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
    if (stop_) {
        CloseHandle(stop_);
        stop_ = nullptr;
    }
    if (mutex_) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
    handler_ = Handler();
    identity_.clear();
}
DWORD WINAPI SupervisorPipe::Thread(void* self) {
    static_cast<SupervisorPipe*>(self)->Run();
    return 0;
}
void SupervisorPipe::Run() {
    while (WaitForSingleObject(stop_, 0) == WAIT_TIMEOUT) {
        DWORD count = 0;
        if (!Io(pipe_, Operation::Connect, nullptr, 0, count, stop_, INFINITE))
            break;
        Buffer bytes;
        if (bytes.Resize(65536) && Io(pipe_, Operation::Read, bytes.Data(), 65536, count, stop_, 5000)) {
            LaunchRequest request;
            LaunchReply reply;
            std::string error;
            std::wstring identity;
            if (!LaunchRequest::Decode(ByteView(bytes.Data(), count), request, error))
                reply.error = error;
            else if (!request.Identity(identity, error) || identity != identity_)
                reply.error = "Request belongs to a different supervisor";
            else if (WaitForSingleObject(stop_, 0) == WAIT_TIMEOUT)
                reply = handler_(request);
            else
                reply.error = "Supervisor is stopping";
            Buffer response;
            if (!reply.Encode(response)) {
                reply = LaunchReply();
                reply.error = "Invalid supervisor response";
                reply.Encode(response);
            }
            if (Io(pipe_, Operation::Write, response.Data(), static_cast<DWORD>(response.Size()), count, stop_, 5000) &&
                count == response.Size()) {
                uint8_t ack = 0;
                // Bounded acknowledgement replaces FlushFileBuffers, which can block on an abandoned reader.
                Io(pipe_, Operation::Read, &ack, 1, count, stop_, 5000);
            }
        }
        DisconnectNamedPipe(pipe_);
    }
}
bool SupervisorPipe::Forward(const LaunchRequest& request, LaunchReply& reply, DWORD timeoutMs, std::string& error) {
    error = "Supervisor did not complete the request; launch outcome may be unknown";
    if (!timeoutMs || timeoutMs == INFINITE)
        return false;
    std::wstring identity;
    Buffer bytes;
    if (!request.Identity(identity, error) || !request.Encode(bytes, error))
        return false;
    error = "Supervisor did not complete the request; launch outcome may be unknown";
    const std::wstring name = PipeName(identity);
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    HANDLE raw = INVALID_HANDLE_VALUE;
    do {
        raw = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (raw != INVALID_HANDLE_VALUE)
            break;
        const DWORD failure = GetLastError();
        if (failure != ERROR_PIPE_BUSY && failure != ERROR_FILE_NOT_FOUND)
            return false;
        if (!Remaining(deadline))
            return false;
        Sleep(std::min<DWORD>(10, Remaining(deadline)));
    } while (true);
    Handle pipe(raw);
    DWORD mode = PIPE_READMODE_MESSAGE, count = 0;
    if (!SetNamedPipeHandleState(raw, &mode, nullptr, nullptr) ||
        !Io(raw, Operation::Write, bytes.Data(), static_cast<DWORD>(bytes.Size()), count, nullptr,
            Remaining(deadline)) ||
        count != bytes.Size())
        return false;
    Buffer response;
    if (!response.Resize(4116) || !Io(raw, Operation::Read, response.Data(), 4116, count, nullptr, Remaining(deadline)))
        return false;
    LaunchReply received;
    if (!LaunchReply::Decode(ByteView(response.Data(), count), received))
        return false;
    uint8_t ack = 1;
    DWORD acknowledged = 0;
    Io(raw, Operation::Write, &ack, 1, acknowledged, nullptr, Remaining(deadline));
    reply = std::move(received);
    error.clear();
    return true;
}
} // namespace glide
