#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;


class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop *baseLoop,const std::string &nameArg);
    ~EventLoopThreadPool();

    //设置底层线程的数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads;}

    //开启事件循环线程
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //若是多线程，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_;}

    const std::string name() const { return name_;}

private:
    EventLoop *baseLoop_; //EventLoop loop;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_; 
    //创建事件的线程
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    //事件线程里面EventLoop的指针
    std::vector<EventLoop*> loops_;

};
