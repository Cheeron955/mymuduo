#include "EPollPoller.h"
#include "logger.h"
#include "Channel.h"

#include <errno.h>
#include <strings.h>
#include <unistd.h>


const int kNew = -1;       //表示一个channel还没有被添加进epoll里面 channel中index_初始化为-1
const int kAdded = 1;      //表示一个channel已经添加进epoll里面
const int kDeleted = 2;    //表示一个channel已经从epoll里面删除


EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)  //调用基类的成员函数，来初始化基类继承来的成员
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n" ,errno);
    }
    
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

//通过epoll_wait将发生事件的channel通过activeChannels告知给EventLoop
TimeStamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    //实际上因为运行起来poll很多，使用LOG_DEBUG更合理，但是学习阶段使用LOG_INFO即可
    LOG_INFO("func=%s => fd total count:%lu \n",__FUNCTION__, channels_.size());

    //events_是vector类型，
    //events_.begin()返回首元素的地址，
    //*events_.begin()为首元素的值
    //&*events_.begin()存放首元素的地址
    //这就得到了vector底层首元素的起始地址
    int numEvents= ::epoll_wait(epollfd_, &*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno; //记录最开始poll里面的错误值
    TimeStamp now(TimeStamp::now());

    if(numEvents>0)
    {
        LOG_INFO("%d eventS happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size() * 2); 
            //说明当前发生的事件可能多于vector能存放的 ，需要扩容，等待下一轮处理
        }
    }    
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n",__FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR) //不是外部中断引起的
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() errno!");
        }
    }
    return now;
}

//channel update remove => EventLoop updateChannel removeChannel =>Poller updateChannel removeChannel
/**
 *             EventLoop => poller.poll
 *   ChannelList          Poller
 *                     ChannelMap <fd,channel*>   epollfd
*/
void EPollPoller::updateChannel(Channel* channel) 
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d  events=%d index=%d\n",__FUNCTION__, channel->fd(),channel->events(), index);
    if(index == kNew || index ==kDeleted)
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);

    }
    else //channel 已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
           update(EPOLL_CTL_DEL,channel);
           channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//从poller中删除channel
void EPollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d  \n",__FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);

}


void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i=0; i<numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        //EventLoop就拿到了他的poller给他返回的所有发生事件的channel列表了
        activeChannels->push_back(channel);
    }
}

//更新channel通道epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    bzero(&event,sizeof event);

    int fd = channel->fd();

    event.events = channel->events(); //fd感兴趣的事情的组合
    event.data.fd = fd;
    event.data.ptr = channel;
    
    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0)  //成功时返回零。发生错误时，返回-1并设置了errno。 
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }

}