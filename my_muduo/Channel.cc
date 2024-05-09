#include "Channel.h"
#include "EventLoop.h"
#include "logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent =EPOLLIN | EPOLLPRI; 
const int Channel::kWriteEvent = EPOLLOUT; 


Channel::Channel(EventLoop *loop,int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{

}

Channel::~Channel()
{

}

//channel的tie方法 在一个TcpConnection新连接创建的时候 调用
//TcpConnection->channel
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/*
当改变channel所表示的events事件后，update负责在poller里面更改fd相应的事件 epoll_ctl
EventLoop => ChannelList Poller
*/
void Channel::update()
{
    //通过channel所属的EventLoop，调用Poller相应的方法，注册fd的events事件
    loop_->updateChannel(this);
}

//在channel所属的EventLoop中 ，把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(TimeStamp receiveTime)
{
    
    if(tied_)
    {
        std::shared_ptr<void> guard;
        guard = tie_.lock(); //提升
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}


void Channel::handleEventWithGuard(TimeStamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n",revents_);

    // 连接断开，并且fd上没有可读数据（默认水平触发）
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }

}