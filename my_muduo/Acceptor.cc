#include "Acceptor.h"
#include "logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__,__FUNCTION__,__LINE__,errno);  
    }
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool resuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) //创建sock
    , acceptChannel_(loop, acceptSocket_.fd()) //封装成channel
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true); //更改TCP选项
    acceptSocket_.setReusePort(true); 
    acceptSocket_.bindAddress(listenAddr); //绑定套接字
    // Tcpserver::start() Acceptor.listen 有新用户的连接
    // 执行一个回调（connfd=> channel => subloop）
    // baseloop => acceptChannel_(listenfd) => 
    acceptChannel_.setReadCallback(std::bind(&Acceptor::headleRead,this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove(); 
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); //listen 
    acceptChannel_.enableReading(); //acceptChannel_=> Poller
}

//listenfd 有事件发生了，就是有新用户连接了
void Acceptor::headleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if (newConnetionCallback_)
        {
            newConnetionCallback_(connfd,peerAddr);//轮询找到SUBLOOP唤醒，分发当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__,__FUNCTION__,__LINE__,errno); 
        if (errno == ENFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__,__FUNCTION__,__LINE__); 
        } 
    }
}