#pragma once
#include <cstdint>

namespace tchips::contracts {
inline constexpr std::uint32_t protocol_version = 2;
[[nodiscard]] constexpr bool accepts_protocol(std::uint32_t remote) noexcept {
    return remote == protocol_version;
}
} // namespace tchips::contracts
