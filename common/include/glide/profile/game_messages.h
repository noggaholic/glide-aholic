#pragma once
#include "glide/buffer.h"
#include "glide/catalog.h"
#include "glide/guid.h"
#include "glide/chat_text.h"

namespace glide {
enum class PositionResult { Absent, Present, Invalid };
enum class FacingResult { Absent, Present, Invalid };
enum class RelayResult { Ignored, Encoded, Invalid };
enum class FlightEvent { None, Deploy, DetailedMotion, Position, Stop, Invalid };
struct FlightUpdate {
    FlightEvent event = FlightEvent::None;
    uint16_t counter = 0; // Wrapping movement counter shared by detailed and position updates.
};
struct Facing {
    float x = 0, y = 0, z = 0;
};
struct ChatMessage {
    std::string text;
    uint8_t channel = 0, flags = 0;
    uint32_t target = 0;
    bool targeted = false; // A recipient request, distinct from a channel even if target is zero.
};
class GameMessages {
  public:
    virtual ~GameMessages() {}
    // Subject's relation toward this recipient's controlled character.
    virtual bool PeerAttitude(uint16_t, bool, Buffer&) const {
        return false;
    }
    virtual bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer& out) const = 0;
    virtual bool Signal(Buffer& out) const = 0;
    virtual bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const = 0;
    virtual PositionResult DecodePosition(ByteView message, CatalogPosition& out) const = 0;
    virtual FacingResult DecodeFacing(ByteView message, Facing& out) const = 0;
    virtual bool LocalTeleport(ByteView motion, const CatalogPosition& position, uint32_t sequence,
                               Buffer& out) const = 0;
    virtual bool TeleportNeedsMotion() const {
        return true;
    }
    virtual bool PeerTeleport(ByteView motion, const CatalogPosition& position, uint16_t body, uint32_t sequence,
                              Buffer& out) const = 0;
    virtual RelayResult PeerMotion(ByteView motion, uint16_t body, uint32_t sequence, uint32_t actionTick,
                                   Buffer& out) const = 0;
    // Some versions require an authoritative reply to the sender as well as peers.
    virtual RelayResult OwnMotion(ByteView, uint32_t, uint32_t, Buffer&) const {
        return RelayResult::Ignored;
    }
    // Merge a newer position report with the last detailed flight movement.
    // Ignored means no usable template or a report older than that template.
    virtual RelayResult PeerGlide(ByteView motion, ByteView position, uint16_t body, uint32_t sequence,
                                  Buffer& out) const = 0;
    virtual FlightUpdate DecodeFlight(ByteView motion) const = 0;
    virtual bool NeedsActionTickBeforeMotion(ByteView) const {
        return false;
    }
    // peerBody == 0 addresses the player's own character; other values address a peer.
    // skin is the catalog's internal skin definition, not its public command id.
    virtual bool Glider(uint32_t design, uint16_t peerBody, Buffer& out) const = 0;
    virtual bool Mount(uint32_t skin, uint32_t definition, uint16_t peerBody, Buffer& out) const = 0;
    // Start riding now; Mount above announces an already mounted character.
    virtual bool MountStart(uint32_t skin, uint32_t definition, uint16_t peerBody, Buffer& out) const = 0;
    virtual bool Unmount(uint16_t peerBody, Buffer& out) const = 0;
    // Decode exactly one complete wire message; outputs remain unchanged on failure.
    virtual bool DecodeWaypoint(ByteView, uint32_t&) const {
        return false;
    }
    virtual bool DecodeChat(ByteView message, ChatMessage& out) const = 0;
    // Whether this build's message may be broadcast to players on the same map.
    // Private, party and unknown channels must not become public by default.
    virtual bool IsMapChat(const ChatMessage& message) const = 0;
    virtual ChatTextLimits TextLimits() const = 0;
    // Build-specific service channel/display; caller decides the recipients.
    virtual bool ServiceChat(const std::string& text, const Guid& context, Buffer& out) const = 0;
    virtual bool Chat(const ChatMessage& message, const std::string& sender, const Guid& senderGuid,
                      Buffer& out) const = 0;
};
} // namespace glide
