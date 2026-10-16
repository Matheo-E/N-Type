#include <atomic>
#include <chrono>
#include <thread>

#include <ntype/engine/net/NetworkThread.hpp>
#include <ntype/engine/util/Log.hpp>

#include "UdpSocket.hpp"

namespace ntype::engine::net {

namespace {

// How long the thread waits for incoming data before checking the outbound
// queue again. Bounds the extra send latency.
constexpr auto kPollInterval = std::chrono::milliseconds(1);

} // namespace

struct NetworkThread::Impl {
    Impl(const std::uint16_t port, ThreadSafeQueue<InboundMessage>& inbound_queue,
         ThreadSafeQueue<OutboundMessage>& outbound_queue)
        : socket(io, port), inbound(inbound_queue), outbound(outbound_queue) {}

    void receive_next() {
        socket.async_receive([this](const std::error_code error,
                                    const std::span<const std::byte> bytes, const Endpoint& from) {
            if (error == asio::error::operation_aborted) {
                return;
            }
            // Errors such as an ICMP "port unreachable" from a vanished peer
            // are reported on UDP sockets too; they must not stop the loop.
            if (!error) {
                on_datagram(bytes, from);
            }
            receive_next();
        });
    }

    void on_datagram(const std::span<const std::byte> bytes, const Endpoint& from) {
        packets_in.fetch_add(1, std::memory_order_relaxed);
        bytes_in.fetch_add(bytes.size(), std::memory_order_relaxed);
        auto packet = Packet::decode(bytes);
        if (!packet) {
            dropped_malformed.fetch_add(1, std::memory_order_relaxed);
            log::debug("dropped datagram from {}: {}", from.to_string(), to_string(packet.error()));
            return;
        }
        inbound.push({from, std::move(*packet), time::SteadyClock::now()});
    }

    void flush_outbound() {
        for (const auto& message : outbound.drain()) {
            const auto bytes = message.packet.encode();
            if (const auto error = socket.send_to(bytes, message.to)) {
                send_errors.fetch_add(1, std::memory_order_relaxed);
                log::debug("send to {} failed: {}", message.to.to_string(), error.message());
                continue;
            }
            packets_out.fetch_add(1, std::memory_order_relaxed);
            bytes_out.fetch_add(bytes.size(), std::memory_order_relaxed);
        }
    }

    void run(const std::stop_token& stop) {
        receive_next();
        while (!stop.stop_requested()) {
            flush_outbound();
            io.run_for(kPollInterval);
            if (io.stopped()) {
                io.restart();
            }
        }
        flush_outbound();
        socket.close();
        io.poll();
    }

    asio::io_context io;
    UdpSocket socket;
    ThreadSafeQueue<InboundMessage>& inbound;
    ThreadSafeQueue<OutboundMessage>& outbound;
    std::jthread worker;

    std::atomic<std::uint64_t> packets_in{0};
    std::atomic<std::uint64_t> packets_out{0};
    std::atomic<std::uint64_t> bytes_in{0};
    std::atomic<std::uint64_t> bytes_out{0};
    std::atomic<std::uint64_t> dropped_malformed{0};
    std::atomic<std::uint64_t> send_errors{0};
};

NetworkThread::NetworkThread(const std::uint16_t port, ThreadSafeQueue<InboundMessage>& inbound,
                             ThreadSafeQueue<OutboundMessage>& outbound)
    : impl_(std::make_unique<Impl>(port, inbound, outbound)) {}

NetworkThread::~NetworkThread() {
    stop();
}

void NetworkThread::start() {
    if (impl_->worker.joinable()) {
        return;
    }
    impl_->worker = std::jthread([this](const std::stop_token& stop) {
        try {
            impl_->run(stop);
        } catch (const std::exception& error) {
            log::error("network thread stopped: {}", error.what());
        }
    });
}

void NetworkThread::stop() {
    if (impl_->worker.joinable()) {
        impl_->worker.request_stop();
        impl_->worker.join();
    }
}

std::uint16_t NetworkThread::local_port() const noexcept {
    return impl_->socket.local_port();
}

NetStats NetworkThread::stats() const noexcept {
    return {
        impl_->packets_in.load(std::memory_order_relaxed),
        impl_->packets_out.load(std::memory_order_relaxed),
        impl_->bytes_in.load(std::memory_order_relaxed),
        impl_->bytes_out.load(std::memory_order_relaxed),
        impl_->dropped_malformed.load(std::memory_order_relaxed),
        impl_->send_errors.load(std::memory_order_relaxed),
    };
}

} // namespace ntype::engine::net
