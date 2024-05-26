#include <arpa/inet.h>
#include <ev++.h>
#include <gflags/gflags.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <string_view>
#include "iou/libev.h"

namespace iou {

LibevEchoClient::LibevEchoClient(ev::loop_ref loop) : network_loop_(loop), state_(Closed) {
    stdin_io_.set(loop);
    stdin_io_.set(STDIN_FILENO, EV_READ);
    stdin_io_.set<LibevEchoClient, &LibevEchoClient::on_stdin_io_callback>(this);

    network_io_.set(loop);
    network_io_.set<LibevEchoClient, &LibevEchoClient::on_network_io_callback>(this);
}

void LibevEchoClient::Run(std::string_view addr, uint16_t port) {
    addr_ = addr;
    port_ = port;

    client_socket_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (client_socket_ < 0) {
        std::cout << "create client failed: " << strerror(errno) << std::endl;
        return;
    }

    network_io_.start(client_socket_, EV_READ | EV_WRITE);
    stdin_io_.start();
}

void LibevEchoClient::on_stdin_io_callback(ev::io&, int events) {
    auto size = read(STDIN_FILENO, stdin_buffer_.data(), stdin_buffer_.size());
    auto sent_size = 0;
    for (; sent_size < size;) {
        auto s = write(client_socket_, stdin_buffer_.data() + sent_size, size - sent_size);
        sent_size += s;
    }
}

void LibevEchoClient::on_network_io_callback(ev::io&, int events) {
    switch (state_) {
        case Closed: {
            struct sockaddr_in sock_addr;
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = inet_addr(addr_.data());
            sock_addr.sin_port = htons(port_);

            auto ret = connect(client_socket_, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
            if (ret == 0) {
                state_ = Connecting;
            } else if (errno == EAGAIN || errno == EINPROGRESS) {
                state_ = Connecting;
                return;
            } else {
                std::cout << "client connect failed on connect: " << strerror(errno) << std::endl;
                on_close();
            }
            return;
        }
        case Connecting: {
            int err = 0;
            socklen_t len = sizeof(err);
            if (::getsockopt(client_socket_, SOL_SOCKET, SO_ERROR, (void*)&err, &len) < 0) {
                std::cout << "client connect failed on getsockopt: " << strerror(errno)
                          << std::endl;
                on_close();
                return;
            }
            if (err != 0) {
                std::cout << "client connect failed: " << std::endl;
                on_close();
                return;
            }
            state_ = Executing;
            network_io_.stop();
            network_io_.start(client_socket_, EV_READ);
            std::cout << "connected !" << std::endl;
            return;
        }
        case Executing: {
            auto size = read(client_socket_, buffer_.data(), buffer_.size());
            if (size == 0) {
                on_close();
            } else if (size < 0) {
                switch (errno) {
                    case EAGAIN:
                        return;
                    default:
                        std::cout << "client connection error: " << strerror(errno) << std::endl;
                        on_close();
                        return;
                }
            } else {
                std::cout << std::string_view(buffer_.data(), size);
            }
            return;
        }
    }
}

void LibevEchoClient::on_close() {
    stdin_io_.stop();
    network_io_.stop();
    close(client_socket_);
}

}  // namespace iou

DEFINE_string(address, "127.0.0.1", "server address");
DEFINE_int32(port, 8080, "");

using namespace iou;

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    ev::dynamic_loop loop;

    LibevEchoClient client(loop);
    client.Run(FLAGS_address, FLAGS_port);

    loop.run();
    return 0;
}

