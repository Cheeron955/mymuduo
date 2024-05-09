#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"

#include <functional>
#include <memory>
/*
理清楚 EventLoop Channel，Poller之间的关系 他们在Reactor上面对应的Demultiplex 
Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN  EPOLLOUT事件
还绑定了poller返回的具体事件
*/

class EventLoop;


class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()> ;
    using ReadEventCallback = std::function<void(TimeStamp)>;

    Channel(EventLoop *loop,int fd);//只用类型对应的指针，大小是固定的四个字节，不影响编译，所以直接可以前置声明EventLoop就可以
    ~Channel();

    //fd得到poller通知以后，调用相应的方法来处理事件
    void handleEvent(TimeStamp receiveTime);//receiveTime变量，必须包含头文件

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_=std::move(cb);}
    void setWriteCallback(EventCallback cb) { writeCallback_=std::move(cb);}
    void setCloseCallback(EventCallback cb) { closeCallback_=std::move(cb);}
    void setErrorCallback(EventCallback cb) { errorCallback_=std::move(cb);}

    //防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    //
    int fd() const {return fd_;}
    int events() const {return events_;}
    int set_revents(int revt) { revents_=revt; }

    //设置fd相应的状态 update()相当于调用epoll_ctl
    void enableReading() { events_ |= kReadEvent; update();} //相当于把读事件给events相应的位置位了
    void disableReading() { events_ &= ~kReadEvent; update();}
    void enableWriting() { events_ |= kWriteEvent; update();}
    void disableWriting() { events_ &= ~kWriteEvent; update();}
    void disableAll() { events_ = kNoneEvent; update();}

    //返回fd当前的事件状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isReading() const {return events_ & kReadEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}

    int index() {return index_;}
    void set_index(int idx) { index_ = idx;}

    // one loop per thread
    EventLoop* onwerLoop() {return loop_;}
    void remove();
private:

    void update();
    void handleEventWithGuard(TimeStamp receiveTime);

    //成员变量
    static const int kNoneEvent; //fd的状态 没有感兴趣的
    static const int kReadEvent; //读
    static const int kWriteEvent; //写

    EventLoop *loop_;      //事件循环
    const int fd_;         //fd：poller监听的对象 epoll_ctl
    int events_;           //注册fd感兴趣的事件
    int revents_;          //poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;


    //因为channel可以获得fd最终发生的具体事件revent，所以他负责回调
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};


