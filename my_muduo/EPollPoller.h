#pragma once

#include "Poller.h"
#include "TimeStamp.h"

#include <vector>
#include <sys/epoll.h>


/*
epoll的使用
epoll_create    也就是EPollPoller
epoll_ctl  add/mol/del   也就是updateChannel removeChannel
epoll_wait   也就是poll
*/

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    /*
    如果派生类在虚函数声明时使用了override描述符，
    那么该函数必须重载其基类中的同名函数，否则代码将无法通过编译。
    */
    ~EPollPoller() override;


    //重写基类Poller的抽象方法
    TimeStamp poll(int timeoutMs, ChannelList* activeChannels) override;

    void updateChannel(Channel* channel) override;

    void removeChannel(Channel* channel) override;
private:
    //epoll_event初始的长度
    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

    //更新channel通道
    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};

