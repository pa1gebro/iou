#include <arpa/inet.h>
#include <errno.h>
#include <ev++.h>
#include <gflags/gflags.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <memory>
#include <string_view>
#include "iou/libev.h"

namespace iou {

LibevEchoServer::LibevEchoServer(ev::loop_ref loop) : network_loop_(loop), server_socket_(-1) {
    server_io_.set(loop);
    server_io_.set<LibevEchoServer, &LibevEchoServer::on_server_io_callback>(this);
    signal_.set(loop);
    signal_.set(SIGINT);
    signal_.set<LibevEchoServer, &LibevEchoServer::on_server_signal>(this);
}

LibevEchoServer::~LibevEchoServer() {
    if (server_socket_ >= 0) {
        close(server_socket_);
    }
}

void LibevEchoServer::Run(std::string_view addr, uint16_t port) {
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(addr.data());
    sock_addr.sin_port = htons(port);

    server_socket_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (server_socket_ < 0) {
        std::cout << "create socket failed: " << strerror(errno) << std::endl;
        return;
    }

    auto ret = bind(server_socket_, (struct sockaddr*)&sock_addr, sizeof(sock_addr));

    if (ret < 0) {
        std::cout << "bind server addr failed: " << strerror(errno) << std::endl;
        return;
    }

    ret = listen(server_socket_, 1024);

    if (ret < 0) {
        std::cout << "listen the socket failed: " << strerror(errno) << std::endl;
        return;
    }

    std::cout << "server start listen on : " << addr << ":" << port << std::endl;

    server_io_.start(server_socket_, EV_READ);
}

void LibevEchoServer::on_server_io_callback(ev::io&, int events) {
    struct sockaddr_in client_addr;
    socklen_t sock_len;
    auto client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &sock_len);
    if (client_socket < 0) {
        switch (errno) {
            case EAGAIN:
                return;
            default:
                std::cout << "accept error: " << strerror(errno) << std::endl;
                server_io_.stop();
                return;
        }
    } else {
        std::cout << "accept client " << client_socket << std::endl;
        auto client = std::make_unique<LibevEchoServer::Client>(this, network_loop_, client_socket);
        client->Start();
        clients_.insert({client_socket, std::move(client)});
    }
}

LibevEchoServer::Client::Client(LibevEchoServer* server, ev::loop_ref loop, int sock)
    : server_(server), network_loop_(loop), sock_(sock) {
    client_io_.set(loop);
    client_io_.set<LibevEchoServer::Client, &LibevEchoServer::Client::on_client_io_callback>(this);
    client_io_.set(sock, EV_READ);
}

void LibevEchoServer::Client::Start() { client_io_.start(); }

void LibevEchoServer::Client::Stop() {
    client_io_.stop();
    close(sock_);
}

void LibevEchoServer::Client::on_client_io_callback(ev::io&, int events) {
    auto size = read(sock_, buffer_.data(), buffer_.size());
    if (size == 0) {
        server_->on_client_closed(sock_);
    } else if (size < 0) {
        switch (errno) {
            case EAGAIN:
                return;
            default:
                server_->on_client_closed(sock_);
                return;
        }
    } else {
        std::cout << std::string_view(buffer_.data(), size);
        auto wrote = write(sock_, buffer_.data(), size);
        return;
    }
}

void LibevEchoServer::on_server_signal(ev::sig&, int events) { on_shutdown(); }

void LibevEchoServer::on_shutdown() {
    std::cout << "server will quit " << std::endl;
    server_io_.stop();
    signal_.stop();
    auto it = clients_.begin();
    while (it != clients_.end()) {
        it->second->Stop();
        it = clients_.erase(it);
    }
}

void LibevEchoServer::on_client_closed(int sock) {
    auto it = clients_.find(sock);
    if (it != clients_.end()) {
        std::cout << "client closed " << sock << std::endl;
        it->second->Stop();
        clients_.erase(it);
    }
}

}  // namespace iou

using namespace iou;

DEFINE_string(address, "127.0.0.1", "server address");
DEFINE_int32(port, 8080, "");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    ev::dynamic_loop loop;

    LibevEchoServer server(loop);
    server.Run(FLAGS_address, FLAGS_port);

    loop.run();

    return 0;
}

