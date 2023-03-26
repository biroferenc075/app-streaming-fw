#pragma once

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lockfree/queue.hpp>
using boost::asio::ip::tcp;
using boost::lockfree::queue;

namespace sc {
    class tcp_client : public boost::enable_shared_from_this<tcp_client> {
    public:
        tcp_client(tcp::socket& socket);

        void readThread();

        void writeThread();
    private:
        tcp::socket& socket_;

        void handle_write(const boost::system::error_code& /*error*/,
            size_t /*bytes_transferred*/);

        bool shouldStop = false;

    };
}


