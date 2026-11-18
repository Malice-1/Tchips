#pragma once
#include <expected>
#include <session.pb.h>
#include <span>
#include <string>
#include <tchips/contracts/movement.hpp>
#include <tchips/contracts/version.hpp>

namespace tchips::wire {
namespace pb = ::tchips::protocol::v2;
inline constexpr std::size_t max_client_bytes = 4096;
inline constexpr std::size_t max_server_bytes = 32 * 1024;
[[nodiscard]] std::expected<pb::Envelope, std::string> decode(std::span<const std::byte> bytes,
                                                              bool from_client);
[[nodiscard]] pb::Envelope envelope(contracts::SessionId session = 0);
void write(pb::EntityState &destination, const contracts::EntityState &state);
[[nodiscard]] contracts::EntityState read(const pb::EntityState &state);
[[nodiscard]] contracts::InputCommand read(const pb::InputCommand &command);
void write(pb::InputCommand &destination, const contracts::InputCommand &command);
[[nodiscard]] bool valid_input(const contracts::InputCommand &command) noexcept;
} // namespace tchips::wire
