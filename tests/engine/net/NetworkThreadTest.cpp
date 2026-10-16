#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <ntype/engine/net/NetworkThread.hpp>

using namespace std::chrono_literals;
using namespace ntype::engine::net;

namespace {

template <typename Predicate>
bool wait_for(Predicate predicate) {
    for (int i = 0; i < 200; ++i) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

} // namespace

TEST(NetworkThread, DeliversValidPacketsOverLoopback) {
    ThreadSafeQueue<InboundMessage> server_in;
    ThreadSafeQueue<OutboundMessage> server_out;
    ThreadSafeQueue<InboundMessage> client_in;
    ThreadSafeQueue<OutboundMessage> client_out;
    NetworkThread server(0, server_in, server_out);
    NetworkThread client(0, client_in, client_out);
    server.start();
    client.start();

    const auto server_endpoint = Endpoint::resolve("127.0.0.1", server.local_port());
    ASSERT_TRUE(server_endpoint.has_value());
    Packet packet;
    packet.header.type = 7;
    packet.payload = {std::byte{1}, std::byte{2}};
    client_out.push({*server_endpoint, packet});

    std::vector<InboundMessage> received;
    ASSERT_TRUE(wait_for([&] {
        for (auto& message : server_in.drain()) {
            received.push_back(std::move(message));
        }
        return !received.empty();
    }));
    EXPECT_EQ(received[0].packet.header.type, 7);
    EXPECT_EQ(received[0].packet.payload, packet.payload);
    EXPECT_EQ(received[0].from.port, client.local_port());
    EXPECT_EQ(server.stats().packets_in, 1U);
}

TEST(NetworkThread, CountsAndDropsMalformedDatagrams) {
    ThreadSafeQueue<InboundMessage> server_in;
    ThreadSafeQueue<OutboundMessage> server_out;
    ThreadSafeQueue<InboundMessage> client_in;
    ThreadSafeQueue<OutboundMessage> client_out;
    NetworkThread server(0, server_in, server_out);
    NetworkThread client(0, client_in, client_out);
    server.start();
    client.start();

    // A packet whose encoded header has a wrong magic number.
    Packet bad;
    bad.header.magic = 0x1234;
    client_out.push({*Endpoint::resolve("127.0.0.1", server.local_port()), bad});

    ASSERT_TRUE(wait_for([&] { return server.stats().dropped_malformed == 1; }));
    EXPECT_EQ(server_in.size(), 0U);
}

TEST(Endpoint, FormatsAndCompares) {
    const auto endpoint = Endpoint::resolve("127.0.0.1", 4242);
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_EQ(endpoint->to_string(), "127.0.0.1:4242");
    EXPECT_EQ(*endpoint, (Endpoint{0x7F000001, 4242}));
}
