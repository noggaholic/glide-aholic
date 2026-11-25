#pragma once
#include "glide/profile/game_messages.h"
namespace glide {
namespace july14812 {
DecodeStatus DecodeChat(ByteView, ChatMessage&, size_t& consumed);
bool IsPublicChat(const ChatMessage&);
bool PublishChat(const ChatMessage&, const std::string& sender, const Guid&, Buffer&);
bool ServiceChat(const std::string&, Buffer&);
} // namespace july14812
} // namespace glide
