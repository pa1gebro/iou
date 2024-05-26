#pragma once

#include <ev++.h>
#include <netinet/in.h>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string_view>

namespace iou {

class LibevEchoServer {
   public:
    LibevEchoServer(ev::loop_ref);
    ~LibevEchoServer();

    void Run(std::string_view addr, uint16_t port);

   private:
    class Client {
       public:
        Client(LibevEchoServer*, ev::loop_ref, int sock);
        void Start();
        void Stop();

       private:
        void on_client_io_callback(ev::io&, int events);

       private:
        LibevEchoServer* server_;
        int sock_;
        ev::loop_ref network_loop_;
        ev::io client_io_;
        std::array<char, 1024> buffer_;
    };

   private:
    void on_server_io_callback(ev::io&, int events);
    void on_server_signal(ev::sig&, int events);
    void on_shutdown();
    void on_client_closed(int);

   private:
    ev::loop_ref network_loop_;
    int server_socket_;
    ev::io server_io_;
    ev::sig signal_;
    std::map<int, std::unique_ptr<Client>> clients_;
};

class LibevEchoClient {
   public:
    LibevEchoClient(ev::loop_ref);
    void Run(std::string_view address, uint16_t port);

   private:
    void on_stdin_io_callback(ev::io&, int events);
    void on_network_io_callback(ev::io&, int events);

   private:
    void on_close();

   private:
    ev::loop_ref network_loop_;
    int client_socket_;
    ev::io stdin_io_;
    ev::io network_io_;
    std::array<char, 1024> stdin_buffer_;
    std::array<char, 1024> buffer_;
    std::string message_;
    enum { Closed, Connecting, Executing } state_;
    std::string addr_;
    uint16_t port_;
    size_t sent_length_;
    size_t received_length_;
};

}  // namespace iou
