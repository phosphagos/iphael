/**
  * @file TcpServer.cpp
  * @author jason
  * @date 2022/2/28
  */
#include <map>
#include <fmt/format.h>
#include "event/Signal.h"
#include "event/EventLoop.h"
#include "event/ThreadPool.h"
#include "net/InetAddress.h"
#include "net/TcpConnectionSet.h"
#include "net/TcpConnection.h"
#include "net/TcpListener.h"

using namespace iphael;

class EchoServer {
private:
    EventLoopConcept &listenerLoop;
    ThreadPool threadPool;
    TcpListener listener;
    TcpConnectionSet connections;

public:
    EchoServer(EventLoopConcept &loop, const InetAddress& address)
            : listenerLoop{loop},
              threadPool{loop},
              connections{},
              listener{loop, address} {

        if (listener != nullptr) {
            threadPool.SetThreadsCount(4);
            threadPool.Start();

            Coroutine::Spawn(loop, [this] {
                return listenerTask();
            });
        }
    }

    ~EchoServer() = default;

    bool operator==(std::nullptr_t) const { return listener == nullptr; }

private:
    Coroutine listenerTask() {
        while (true) {
            auto sock = co_await listener.Accept();
            if (sock == nullptr) {
                co_return;
            } else {
//                auto &nextLoop = threadPool.NextLoop();
                auto &nextLoop = listenerLoop;
                auto &conn = connections.Emplace(nextLoop, std::move(sock));

                Coroutine::Spawn(
                        nextLoop,
                        [this, &conn] { return connectionTask(conn); },
                        [this, &conn] { connections.Remove(conn); });
            }
        }
    }

    Coroutine connectionTask(TcpConnection &conn) {
        char buffer[4096];

        while (true) {
            ssize_t len = co_await conn.ReadSome(buffer, sizeof(buffer));
            if (len <= 0) { co_return ; }

            if (co_await conn.Write(buffer, len) < 0) { co_return; }
        }
    }
};


int main() {
    EventLoop loop;
    EchoServer server{loop, InetAddress{1234}};

    if (server == nullptr) { return EXIT_FAILURE; }

    std::thread([&loop] {
        getchar();
        loop.Shutdown();
    }).detach();

    return loop.Execute();
}
