#pragma once
#include "noncopyable.h"
#include "TimeStamp.h"

#include <vector>
#include <unordered_map>
/*
muduo库中 多路事件分发器的核心IO复用模块
*/

class Channel;
class EventLoop;

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口
    virtual TimeStamp poll(int timeoutMs, ChannelList *activeChannel) = 0;

    //
    virtual void updateChannel(Channel* channel) = 0;

    virtual void removeChannel(Channel* channel) = 0;

    //判断参数channel是否在当前Poller当中
    bool  hasChannel(Channel* channel) const;

    //EventLoop可以通过改接口获取默认的IO复用的具体实现
    /** 
   * 它的实现并不在 Poller.cc 文件中
   * 因为 Poller 是一个基类。
   * 如果在 Poller.cc 文件内实现则势必会在 Poller.cc包含 EPollPoller.h PollPoller.h等头文件。
   * 那么外面就会在基类引用派生类的头文件，这个抽象的设计就不好
   * 所以外面会单独创建一个 DefaultPoller.cc 的文件去实现
   */
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    //map的key表示 sockfd  value表示所属的channel通道类型
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;  //定义Poller所属的事件循环EventLoop
};