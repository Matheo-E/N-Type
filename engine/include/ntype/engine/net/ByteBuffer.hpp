#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <type_traits>
#include <vector>

#include <ntype/engine/net/DecodeError.hpp>

namespace ntype::engine::net {

/// Byte buffer with explicit little-endian encoding of fixed-width values.
///
/// Values are written field by field (never by memcpy of a struct), so the
/// wire format is identical whatever the compiler, padding or CPU. Reads are
/// bounds-checked: reading past the end yields DecodeError::Truncated.
class ByteBuffer {
public:
    ByteBuffer() = default;
    explicit ByteBuffer(std::span<const std::byte> data) : data_(data.begin(), data.end()) {}

    template <std::integral T>
    void write(const T value) {
        using Unsigned = std::make_unsigned_t<T>;
        const auto bits = static_cast<Unsigned>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            data_.push_back(static_cast<std::byte>((bits >> (8U * i)) & 0xFFU));
        }
    }

    void write(const float value) { write(std::bit_cast<std::uint32_t>(value)); }

    void write_bytes(std::span<const std::byte> bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
    }

    template <std::integral T>
    [[nodiscard]] std::expected<T, DecodeError> read() {
        if (remaining() < sizeof(T)) {
            return std::unexpected(DecodeError::Truncated);
        }
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned bits = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bits |= static_cast<Unsigned>(static_cast<Unsigned>(data_[read_pos_ + i]) << (8U * i));
        }
        read_pos_ += sizeof(T);
        return static_cast<T>(bits);
    }

    template <std::same_as<float> T>
    [[nodiscard]] std::expected<T, DecodeError> read() {
        return read<std::uint32_t>().transform(
            [](const std::uint32_t bits) { return std::bit_cast<float>(bits); });
    }

    [[nodiscard]] std::expected<std::span<const std::byte>, DecodeError>
    read_bytes(const std::size_t count) {
        if (remaining() < count) {
            return std::unexpected(DecodeError::Truncated);
        }
        const std::span<const std::byte> result(data_.data() + read_pos_, count);
        read_pos_ += count;
        return result;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - read_pos_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return data_; }
    [[nodiscard]] std::vector<std::byte> release() && noexcept { return std::move(data_); }

private:
    std::vector<std::byte> data_;
    std::size_t read_pos_ = 0;
};

} // namespace ntype::engine::net
