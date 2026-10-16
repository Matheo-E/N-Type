#include <ntype/engine/util/Log.hpp>
#include <ntype/rtype/protocol/Codec.hpp>
#include <ntype/rtype/protocol/Transport.hpp>

namespace ntype::rtype::protocol {

engine::net::Packet make_packet(engine::net::Connection& connection, const Message& message,
                                const engine::time::TimePoint now) {
    auto encoded = encode(message);
    const auto type = static_cast<std::uint8_t>(encoded.type);
    return connection.make_packet(type, std::move(encoded.payload), delivery_of(encoded.type), now);
}

std::optional<Message> receive(engine::net::Connection& connection,
                               const engine::net::Packet& packet,
                               const engine::time::TimePoint now) {
    if (!connection.on_received(packet.header, now)) {
        return std::nullopt;
    }
    auto message = decode(packet.header.type, packet.payload);
    if (!message) {
        engine::log::debug("malformed message type {} from {}: {}", packet.header.type,
                           connection.remote().to_string(), to_string(message.error()));
        return std::nullopt;
    }
    return std::move(*message);
}

} // namespace ntype::rtype::protocol
