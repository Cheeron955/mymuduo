#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TimeStamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer 通过Acceptor 有一个新用户连接，通过accept函数拿到connfd
 * 打包TcpConnection 设置回调 => channel =>poller => channel的回调
 * 
*/
class TcpConnection : noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    
    TcpConnection(EventLoop *loop,
            const std::string &name,
            int sockfd,
            const InetAddress &localAddr,
            const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_;}
    const std::string& name() const { return name_;}
    const InetAddress& localAddress() const { return localAddr_;}
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected;}
    //bool disconnected() const { return state_ == kDisconnected;}

    //发送数据
    void send(const std::string &buf);

    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {       
        highWaterMarkCallback_ = cb;
        highWaterMark_= highWaterMark;
    }

    void setCloseCallback(const CloseCallback& cb)
    {
        closeCallback_ = cb;
    }

    //建立连接
    void connectEstablished();

    //销毁连接
    void connectDestroyed();


private:
    //已经断开连接，正在连接，已经连接，正在断开连接
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);

    void shutdownInLoop();

    EventLoop *loop_; //绝对不是baseloop，因为TcpConnetion都是在subloop中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_;
    
    Buffer inputBuffer_; //接受数据的缓冲区
    Buffer outputBuffer_; //发送数据的缓冲区





};
