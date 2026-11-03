#include "NetworkThread.hpp"

#include <iostream>
#include <system_error>

namespace ntype::server {

NetworkThread::NetworkThread(const std::uint16_t port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {
    receive();
    thread_ = std::thread([this] { io_.run(); });
}

NetworkThread::~NetworkThread() {
    io_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void NetworkThread::send(const asio::ip::udp::endpoint& to, std::vector<std::uint8_t> data) {
    // The socket is only used from the network thread, so the send is posted there.
    asio::post(io_, [this, to, data = std::move(data)] {
        std::error_code error;
        socket_.send_to(asio::buffer(data), to, 0, error);
        if (error) {
            std::cerr << "Send error: " << error.message() << '\n';
        }
    });
}

void NetworkThread::receive() {
    socket_.async_receive_from(asio::buffer(buffer_), sender_,
                               [this](const std::error_code& error, const std::size_t size) {
        if (error == asio::error::operation_aborted) {
            return;
        }
        // Other errors (e.g. an ICMP "port unreachable" on Windows) only concern
        // one client, so the server keeps listening.
        if (!error) {
            inbound_.push({sender_, {buffer_.data(), buffer_.data() + size}});
        }
        receive();
    });
}

} // namespace ntype::server
