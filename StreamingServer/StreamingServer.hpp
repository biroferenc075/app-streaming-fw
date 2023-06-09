#pragma once

#include <ctime>
#include <iostream>
#include <string>
#include "consts.hpp"
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>

#include <stb_image.h>

using boost::asio::ip::tcp;

bool shouldStop = false;
std::string make_daytime_string()
{
    using namespace std;
    time_t now = time(0);
#pragma warning(disable : 4996)
    return ctime(&now);
}

class tcp_connection
    : public boost::enable_shared_from_this<tcp_connection>
{
public:
    typedef boost::shared_ptr<tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return socket_;
    }


    //const std::string fpath = "f0.png";
    static const int frameNum = 57;
    unsigned char* pixels[frameNum];
    int imgWidth, imgHeight, imgChannels, imageSize;
    unsigned char delim[4] = { 1u,2u,3u,4u };
    int delimSize = 4;
    
    void start(boost::asio::io_context& io_context, boost::lockfree::queue<BFE::Frame*>& queue)
    {
        try {
            message_ = make_daytime_string();
            /*for (int i = 0; i < frameNum; i++) {
                char buf[30];
                sprintf(buf, "frame_%02d_delay-0.1s.png", i);
                pixels[i] = stbi_load(buf, &imgWidth, &imgHeight, &imgChannels, STBI_rgb_alpha);
                if (!pixels[i]) {
                    std::cout << buf;
                    throw std::runtime_error("failed to load image!");
                }
            }*/
            //imageSize = imgWidth * imgHeight * 4;
            std::cout << imageSize << " imagesize \n";

            boost::thread t(boost::bind(&tcp_connection::readThread, this));
            t.detach();
            writeThread(io_context, queue);
        }
        catch (std::exception e) {
            std::cout << e.what();
        }

    }

    void readThread() {
        try {
            while (!shouldStop)
            {
                boost::array<char, 128> buf;
                boost::system::error_code error;

                size_t len = socket_.read_some(boost::asio::buffer(buf), error);

                if (error == boost::asio::error::eof) {
                    shouldStop = true;
                    break; // Connection closed cleanly by peer.
                }
                else if (error) {
                    shouldStop = true;
                    throw boost::system::system_error(error); // Some other error.
                }

                message_.assign(buf.data(), len);
            }
        }
        catch (std::exception e)
        {
            std::cout << e.what();
        }

    }

    void writeThread(boost::asio::io_context& io_context, boost::lockfree::queue<BFE::Frame*>& queue) {
        socket_.wait_read();
        int l = 0;
        boost::array<char, 8> buf;
        while (l = socket_.read_some(boost::asio::buffer(buf))) {
            if (l > 0 && buf[0] == 'r') {
                break;
            }
        }
        boost::asio::steady_timer t(io_context, boost::asio::chrono::nanoseconds(1'000'000'000 / FRAMERATE));//boost::asio::chrono::milliseconds(250)); //boost::asio::chrono::nanoseconds(1'000'000'000 / 30));

        std::cout << "write\n";
        int i = 0;
        while (!shouldStop)
        {
            BFE::Frame* f;
            if (queue.pop(f)) {
                imageSize = f->size;
                //sendMsg(pixels[i++ % frameNum]);
                sendMsg(f->data);
                sendDelim();
                std::cout << i++ << " tick" << imageSize << " imagesize \n";
                t.expires_from_now(boost::asio::chrono::nanoseconds(1'000'000'000 / FRAMERATE));
                t.wait();

                delete f;
            }
        }
    }

    void sendMsg(void* src) {
        int sent = 0;
        int total = 0;
        //boost::asio::async_write(socket_, boost::asio::buffer(src, imageSize), boost::bind(&tcp_connection::handle_write, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        while (total < imageSize) {
            sent = socket_.send(boost::asio::buffer((unsigned char*)src + total, imageSize - total));
            total += sent;
            //std::cout << imageSize - total << std::endl;
        }
    }

    void sendDelim() {
        int sent = 0;
        int total = 0;
        while (total < delimSize) {
            sent = socket_.send(boost::asio::buffer(delim + total, delimSize - total));
            total += sent;
        }
    }

private:
    tcp_connection(boost::asio::io_context& io_context)
        : socket_(io_context)
    {
    }

    void handle_write(const boost::system::error_code& error,
        size_t bytes)
    {
        std::cout << "message sent, bytes transferred: " << bytes << "\n";
        if (error) {
            std::cout << error.message();
        }
    }

    tcp::socket socket_;
    std::string message_;

};

class tcp_server
{
public:
    tcp_server(boost::asio::io_context& io_context, boost::lockfree::queue<BFE::Frame*>& q)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 13)), queue(q)
    {
        start_accept();
    }

private:
    boost::lockfree::queue<BFE::Frame*> &queue;
    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_);

        acceptor_.async_accept(new_connection->socket(),
            boost::bind(&tcp_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
    }

    void handle_accept(tcp_connection::pointer new_connection,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_connection->start(io_context_, queue);
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};
void runServer(boost::lockfree::queue<BFE::Frame*>* q) {
    try
    {
        boost::asio::io_context io_context;
        tcp_server server(io_context, *q);
        boost::thread t(boost::bind(&boost::asio::io_service::run, &io_context));
        char inp;
        do {
            std::cin >> inp;
        } while (inp != 'x');
        shouldStop = true;
        io_context.stop();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
/*int main()
{
    runServer();
    return 0;
}*/