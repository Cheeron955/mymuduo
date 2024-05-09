#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool resuseport);
    ~Acceptor();

    void setNewConnetionCallback(const NewConnectionCallback &cb)
    {
        newConnetionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void headleRead();
     
    EventLoop *loop_; //Acceptor用的就是用户定义的那个baseloop_，也就是mainloop
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnetionCallback_;
    bool listenning_;


};
