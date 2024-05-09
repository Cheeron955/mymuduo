#pragma once
 
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

//事件循环类  主要包含了两大模块 Channel Poller（epoll的抽象）
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    
    //退出事件循环
    void quit(); 

    TimeStamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行cb
    void runInLoop(Functor cb);

    //把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    //用来唤醒loop所在的线程的
    void wakeup();

    //EventLoop的方法=> poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel); 

    //证明EventLoop创建时的线程id与当前线程id是否相等 
    // 相等表示EventLoop就在所创建他的loop线程里面，可以执行回调
    // 不相等就需要queueInLoop，等待唤醒它自己的线程时，在执行回调
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
 
    void handleRead(); //唤醒用的 wake up
    void doPendingFunctors(); //执行回调的

    using ChannelList = std::vector<Channel*>; 

    std::atomic_bool looping_; //原子操作，底层通过CAS实现
    std::atomic_bool quit_; //标志退出loop循环
    
    const pid_t threadId_; //记录当前loop所在线程的id

    TimeStamp pollReturnTime_; //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;

    //当mainloop获取一个新用户的channel，通过轮询方法选择一个subloop，
    //通过该成员唤醒sunloop，处理channel----通过eventfd
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; //存储loop需要执行的所有回调操作
    std::mutex mutex_;  //互斥锁，用来保护上面vector容器的线程安全操作

};

