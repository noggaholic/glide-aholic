#pragma once
#include "glide/profile/game_messages.h"
namespace glide {
namespace hot49284 {
DecodeStatus DecodeChat(ByteView, ChatMessage& out, size_t& consumed);
bool IsPublicChat(const ChatMessage&);
bool PlayerChat(const ChatMessage&, const std::string& sender, const Guid&, Buffer& out);
bool ServiceChat(const std::string& text, Buffer& out);
} // namespace hot49284
} // namespace glide
