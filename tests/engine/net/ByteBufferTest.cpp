#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include <ntype/engine/net/ByteBuffer.hpp>

using ntype::engine::net::ByteBuffer;
using ntype::engine::net::DecodeError;

TEST(ByteBuffer, RoundTripsFixedWidthValues) {
    ByteBuffer buffer;
    buffer.write(std::uint8_t{0xAB});
    buffer.write(std::int16_t{-1234});
    buffer.write(std::uint32_t{0xDEADBEEF});
    buffer.write(std::numeric_limits<std::int64_t>::min());
    buffer.write(3.5F);

    ByteBuffer reader(buffer.bytes());
    EXPECT_EQ(*reader.read<std::uint8_t>(), 0xAB);
    EXPECT_EQ(*reader.read<std::int16_t>(), -1234);
    EXPECT_EQ(*reader.read<std::uint32_t>(), 0xDEADBEEFU);
    EXPECT_EQ(*reader.read<std::int64_t>(), std::numeric_limits<std::int64_t>::min());
    EXPECT_FLOAT_EQ(*reader.read<float>(), 3.5F);
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ByteBuffer, EncodesLittleEndianWhateverTheHost) {
    ByteBuffer buffer;
    buffer.write(std::uint32_t{0x01020304});
    const auto bytes = buffer.bytes();
    ASSERT_EQ(bytes.size(), 4U);
    EXPECT_EQ(bytes[0], std::byte{0x04});
    EXPECT_EQ(bytes[3], std::byte{0x01});
}

TEST(ByteBuffer, ReadingPastTheEndIsAnErrorNotACrash) {
    ByteBuffer buffer;
    buffer.write(std::uint16_t{7});
    ByteBuffer reader(buffer.bytes());
    const auto value = reader.read<std::uint32_t>();
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error(), DecodeError::Truncated);
    EXPECT_FALSE(reader.read_bytes(3).has_value());
}
