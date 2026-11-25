#pragma once
#include "glide/buffer.h"
#include "agent_protocol.h"
namespace glide {
namespace hot49284 {
using MotionMessage = Glide49284AgentMessage;
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out, size_t& consumed);
// Relays one complete validated request, replacing only agent and sequence.
// Position-only/delay/report messages have no corresponding relay packet.
bool RelayMotion(ByteView exactRequest, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace hot49284
} // namespace glide
