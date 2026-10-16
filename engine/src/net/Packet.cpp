#include <ntype/engine/net/Packet.hpp>

namespace ntype::engine::net {

void PacketHeader::write(ByteBuffer& buffer) const {
    buffer.write(magic);
    buffer.write(version);
    buffer.write(type);
    buffer.write(flags);
    buffer.write(sequence);
    buffer.write(ack);
    buffer.write(ack_bits);
    buffer.write(payload_size);
}

std::expected<PacketHeader, DecodeError> PacketHeader::read(ByteBuffer& buffer) {
    if (buffer.remaining() < kSize) {
        return std::unexpected(DecodeError::Truncated);
    }
    // The size check above guarantees every read below succeeds.
    PacketHeader header;
    header.magic = *buffer.read<std::uint16_t>();
    if (header.magic != kMagic) {
        return std::unexpected(DecodeError::BadMagic);
    }
    header.version = *buffer.read<std::uint8_t>();
    if (header.version != kVersion) {
        return std::unexpected(DecodeError::BadVersion);
    }
    header.type = *buffer.read<std::uint8_t>();
    header.flags = *buffer.read<std::uint8_t>();
    header.sequence = *buffer.read<std::uint16_t>();
    header.ack = *buffer.read<std::uint16_t>();
    header.ack_bits = *buffer.read<std::uint32_t>();
    header.payload_size = *buffer.read<std::uint16_t>();
    if (header.payload_size > kMaxPayloadSize) {
        return std::unexpected(DecodeError::OversizedPayload);
    }
    return header;
}

std::vector<std::byte> Packet::encode() const {
    ByteBuffer buffer;
    PacketHeader stamped = header;
    stamped.payload_size = static_cast<std::uint16_t>(payload.size());
    stamped.write(buffer);
    buffer.write_bytes(payload);
    return std::move(buffer).release();
}

std::expected<Packet, DecodeError> Packet::decode(const std::span<const std::byte> bytes) {
    if (bytes.size() > PacketHeader::kMaxDatagramSize) {
        return std::unexpected(DecodeError::OversizedPayload);
    }
    ByteBuffer buffer(bytes);
    auto header = PacketHeader::read(buffer);
    if (!header) {
        return std::unexpected(header.error());
    }
    if (buffer.remaining() < header->payload_size) {
        return std::unexpected(DecodeError::Truncated);
    }
    if (buffer.remaining() > header->payload_size) {
        return std::unexpected(DecodeError::TrailingData);
    }
    const auto payload = *buffer.read_bytes(header->payload_size);
    return Packet{*header, {payload.begin(), payload.end()}};
}

} // namespace ntype::engine::net
