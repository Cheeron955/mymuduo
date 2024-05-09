#include "TcpConnection.h"
#include "logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop *CheckLoopNotNull(EventLoop* loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null! \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
            const std::string &nameArg,
            int sockfd,
            const InetAddress &localAddr,
            const InetAddress &peerAddr)
        : loop_(CheckLoopNotNull(loop))
        , name_(nameArg)
        , state_(kConnecting)
        , reading_(true)
        , socket_(new Socket(sockfd))
        , channel_(new Channel(loop,sockfd))
        , localAddr_(localAddr)
        , peerAddr_(peerAddr)
        , highWaterMark_(64*1024*1024) //64M
        {
            //下面给channel设置相应的回调函数
            //poller给channel通知感兴趣的事件发生了
            //channel会回调相应的操作函数
            //将TcpConnection自己的成员函数注册给当前accept返回的connfd对应的Channel对象上
            channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
            channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
            channel_->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
            channel_->setErrorCallback(std::bind(&TcpConnection::handleError,this));

            LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
            socket_->setKeepAlive(true);

        }


TcpConnection::~TcpConnection()
{
    //TcpConnection没有自己开辟什么资源，所以析构不用做什么
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
                    name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::send(const std::string &buf) //直接引用buffer
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            //string.c_str是Borland封装的String类中的一个函数，它返回当前字符串的首字符地址。
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop
                                , this
                                , buf.c_str()
                                , buf.size()
            ));
        }
    }
}
/**
 * 发送数据，应用写得快，内核发送数据慢，
 * 需要把待发送的数据写入缓冲区
 * 且设置了水位回调，防止发送太快
*/
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len; //未发送的数据
    bool faultError = false; //记录是否产生错误

    //之前调用过connection的shutdown 不能在发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected,give up writing!");
        return ;
    }

    //channel 第一次开始写数据，且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(),data,len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                //一次性数据全部发送完成，就不要再给channel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK) //用于非阻塞模式，不需要重新读或者写
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) //SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }

    //说明当前这一次write ，并没有把数据全发送出去，剩余的数据
    // 需要保存到缓冲区当中，给channel注册epollout事件
    //poller发现tcp发送缓冲区有空间，会通知相应的socket-channel
    //调用相应的writeCallback（）回调方法
    // 也就是调用TcpConnection::handleWrite，把发送缓冲区中数据全部发送出去
    if(!faultError && remaining > 0) 
    {
        //目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen + remaining >= highWaterMark_ 
            && oldlen < highWaterMark_
            && highWaterMark_)
            {
                loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_,shared_from_this(),oldlen + remaining)
                );
            }
            outputBuffer_.append((char*)data + nwrote,remaining);
            if(!channel_->isWriting())
            {
                channel_->enableWriting(); //注册channel写事件，否则poller不会向channel通知epollout
            }
    }
}

void TcpConnection::handleRead(TimeStamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n > 0)
    {
        //已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        //shared_from_this()获取了当前TcpConnection对象的智能指针
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n==0) //客户端断开
    {
        handleClose();
    } 
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::hanleRead");
        handleError();
    }

}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n); //处理了n个
            if(outputBuffer_.readableBytes() == 0) //发送完成
            {
                channel_->disableWriting(); //不可写了
                if(writeCompleteCallback_)
                {
                    //唤醒loop对应的thread线程，执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();// 在当前loop中删除TcpConnection
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",channel_->fd());
    }
}

//Poller => Channel::closeCallback => TcpConnection::handlerClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调
    closeCallback_(connPtr); //关闭连接的回调 TcpServer => TcpServer::removeConnection
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0)
    {
        err = errno;        
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);

}


 //关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting()) //说明当前outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdowmWrite(); // 关闭写端

    }
}

//建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); //向poller注册channel的epollin事件

    //新连接建立 执行回调
    connectionCallback_(shared_from_this());
}

//销毁连接
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); //把channel所有感兴趣的事件，从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller中删除掉
}