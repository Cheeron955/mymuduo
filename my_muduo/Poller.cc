#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)   //把loop所属的EventLoop记录下来
{
}

bool  Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd());
    //找到了并且相等 说明参数传入的channel就是poller中的channel
    return it !=channels_.end() && it->second == channel;
}
