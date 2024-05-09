#include "EventLoopThread.h"
#include "EventLoop.h"



EventLoopThread::EventLoopThread(const ThreadInitCallback &cb ,
        const std::string &name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc,this),name)
        , mutex_()
        , cond_()
        , callback_(cb)
        {

        }

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); //启动底层新线程
    /**
     * 会调用Thread::start()，然后执行func_(); func_(std::move(func))；
     * 而func就是&EventLoopThread::threadFunc,this 传入的，所以会启动一个新线程
    */
    EventLoop *loop =nullptr; //当startLoop时，才开始创建新线程,线程里面有一个loop
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_==nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;       
    }
    return loop;
}

//下面这个方法 是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop;//创建一个独立的Eventloop，和上面的线程是一一对应的 one loop per thread

    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); //EventLoop loop => Poller.poll
    std::unique_lock<std::mutex> lock(mutex_);
    loop_=nullptr;
}

