#include <cmath>
#include <google/protobuf/message.h>
#include <numbers>
#include <tchips/protocol/codec.hpp>

namespace {
bool no_unknown_fields(const google::protobuf::Message &message) {
    const auto *reflection = message.GetReflection();
    if (!reflection->GetUnknownFields(message).empty())
        return false;
    std::vector<const google::protobuf::FieldDescriptor *> fields;
    reflection->ListFields(message, &fields);
    for (const auto *field : fields)
        if (field->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
            if (field->is_repeated()) {
                for (int i = 0; i < reflection->FieldSize(message, field); ++i)
                    if (!no_unknown_fields(reflection->GetRepeatedMessage(message, field, i)))
                        return false;
            } else if (!no_unknown_fields(reflection->GetMessage(message, field)))
                return false;
        }
    return true;
}
} // namespace
namespace tchips::wire {
std::expected<pb::Envelope, std::string> decode(std::span<const std::byte> bytes, bool from_client) {
    if (bytes.empty() || bytes.size() > (from_client ? max_client_bytes : max_server_bytes))
        return std::unexpected("message size");
    pb::Envelope result;
    if (!result.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())) || !no_unknown_fields(result))
        return std::unexpected("malformed or unknown protobuf fields");
    if (!contracts::accepts_protocol(result.protocol_version()))
        return std::unexpected("protocol version");
    if (result.body_case() == pb::Envelope::BODY_NOT_SET)
        return std::unexpected("missing body");
    if (from_client) {
        if (!result.has_handshake() && !result.has_inputs() && !result.has_disconnect())
            return std::unexpected("client result forbidden");
        if (result.has_inputs()) {
            const auto &commands = result.inputs().commands();
            if (commands.empty() || commands.size() > static_cast<int>(contracts::input_redundancy))
                return std::unexpected("batch size");
            for (const auto &command : commands)
                if (!valid_input(read(command)))
                    return std::unexpected("invalid input");
        }
    } else {
        if (result.has_inputs() || result.has_handshake())
            return std::unexpected("unexpected server body");
        if (result.has_snapshot() &&
            (result.snapshot().entities_size() == 0 ||
             result.snapshot().entities_size() > static_cast<int>(contracts::max_visible_entities)))
            return std::unexpected("snapshot size");
    }
    if (result.has_disconnect() &&
        !pb::DisconnectReason_Code_IsValid(static_cast<int>(result.disconnect().code())))
        return std::unexpected("disconnect code");
    return result;
}
pb::Envelope envelope(contracts::SessionId session) {
    pb::Envelope e;
    e.set_protocol_version(contracts::protocol_version);
    e.set_session_id(session);
    return e;
}
void write(pb::EntityState &d, const contracts::EntityState &s) {
    d.set_entity_id(s.id);
    d.set_x(s.position.x);
    d.set_y(s.position.y);
    d.set_angle(s.angle);
}
contracts::EntityState read(const pb::EntityState &s) {
    return {s.entity_id(), {s.x(), s.y()}, s.angle()};
}
contracts::InputCommand read(const pb::InputCommand &c) {
    return {c.sequence(), {c.move_x(), c.move_y()}, c.angle()};
}
void write(pb::InputCommand &d, const contracts::InputCommand &c) {
    d.set_sequence(c.sequence);
    d.set_move_x(c.direction.x);
    d.set_move_y(c.direction.y);
    d.set_angle(c.angle);
}
bool valid_input(const contracts::InputCommand &c) noexcept {
    return c.sequence > 0 && std::isfinite(c.direction.x) && std::isfinite(c.direction.y) &&
           std::abs(c.direction.x) <= 1 && std::abs(c.direction.y) <= 1 && std::isfinite(c.angle) &&
           std::abs(c.angle) <= std::numbers::pi;
}
} // namespace tchips::wire
