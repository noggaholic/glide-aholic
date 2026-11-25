#pragma once
#include "glide/character.h"

namespace glide {
enum class CreateAction { Start, Preview, Submit, Close, Complete, Control, Acknowledge, StartedControl };
class CreateHandshake {
  public:
    bool Accept(CreateAction action) {
        if (action == CreateAction::Start) {
            if (started_)
                return false;
            started_ = true;
            return true;
        }
        if (action == CreateAction::Acknowledge) {
            if (!started_ || acknowledged_)
                return false;
            acknowledged_ = true;
            return true;
        }
        if (action == CreateAction::Submit)
            return started_ && acknowledged_;
        if (action == CreateAction::Preview || action == CreateAction::StartedControl)
            return started_;
        return true;
    }

  private:
    bool started_ = false, acknowledged_ = false;
};
struct CreateMessage {
    CreateAction action = CreateAction::Control;
    size_t consumed = 0;
    Character character;
};
class CreateFlow {
  public:
    virtual ~CreateFlow() {}
    virtual bool WorldRequestThroughAuth() const {
        return false;
    }
    virtual bool RequiresReadyAcknowledgment() const {
        return false;
    }
    virtual DecodeStatus Decode(ByteView input, CreateMessage& out) const = 0;
    virtual bool ValidateSubmit(const Character& character) const = 0;
    virtual const SpawnPoint& InitialLocation() const = 0;
    virtual bool Ready(Buffer& out) const = 0;
    virtual bool Signal(Buffer& out) const = 0;
    virtual bool Created(const Guid& account, const Character& character, Buffer& out) const = 0;
};
} // namespace glide
