#include <array>
#include <google/protobuf/unknown_field_set.h>
#include <gtest/gtest.h>
#include <tchips/protocol/codec.hpp>
#include <vector>

namespace {
std::expected<tchips::wire::pb::Envelope, std::string> decode(const tchips::wire::pb::Envelope &message,
                                                              bool client = true) {
    const auto bytes = message.SerializeAsString();
    return tchips::wire::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}), client);
}
} // namespace
TEST(WireProtocol, AcceptsVersionedHandshakeAndBoundedInputBatch) {
    auto message = tchips::wire::envelope();
    message.mutable_handshake()->set_map_fingerprint(123);
    EXPECT_TRUE(decode(message));
    message.clear_handshake();
    for (unsigned i = 1; i <= 16; ++i)
        tchips::wire::write(*message.mutable_inputs()->add_commands(), {i, {1, 1}, 0});
    EXPECT_TRUE(decode(message));
    tchips::wire::write(*message.mutable_inputs()->add_commands(), {17, {1, 0}, 0});
    EXPECT_FALSE(decode(message));
}
TEST(WireProtocol, RejectsClientResultsUnknownFieldsAndWrongVersions) {
    auto message = tchips::wire::envelope();
    message.mutable_snapshot()->set_tick(1);
    EXPECT_FALSE(decode(message));
    message.clear_snapshot();
    message.mutable_handshake();
    message.GetReflection()->MutableUnknownFields(&message)->AddVarint(99, 123);
    EXPECT_FALSE(decode(message));
    message.GetReflection()->MutableUnknownFields(&message)->Clear();
    message.set_protocol_version(1);
    EXPECT_FALSE(decode(message));
}
TEST(WireProtocol, RejectsMalformedOversizedAndEmptyData) {
    EXPECT_FALSE(tchips::wire::decode({}, true));
    std::vector<std::byte> oversized(tchips::wire::max_client_bytes + 1);
    EXPECT_FALSE(tchips::wire::decode(oversized, true));
    const std::array<std::byte, 2> invalid{std::byte{255}, std::byte{255}};
    EXPECT_FALSE(tchips::wire::decode(invalid, true));
}
TEST(WireProtocol, RejectsInputWithoutSequenceAndServerInputBodies) {
    auto message = tchips::wire::envelope(1);
    message.mutable_inputs()->add_commands()->set_move_x(1);
    EXPECT_FALSE(decode(message));
    message.mutable_inputs()->mutable_commands(0)->set_sequence(1);
    EXPECT_FALSE(decode(message, false));
}
