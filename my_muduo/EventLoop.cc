#include "EventLoop.h"
#include "logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <errno.h>

//防止一个线程创建多个EventLoop
//当创建了一个EventLoop对象时，*t_loopInThisThread就指向这个对象
//在一个线程里面在创建EventLoop时，指针不为空就不会创建了
//从而控制了一个线程里面只有一个EventLoop
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeupfd 用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    //eventfd 计数不为零表示有可读事件发生，read 之后计数会清零，write 则会递增计数器。  
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this)) //当前对象本身就是loop
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EvnetLoop %p exists in this thread %d \n",t_loopInThisThread, threadId_);    
    }
    else
    {
        t_loopInThisThread = this;
    }

    //设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    //minreactor通过给subreactor写东西，通知其苏醒
    wakeupChannel_->enableReading();

}


EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n",this);

    while(!quit_)
    {
        activeChannels_.clear();
        //监听两类fd 一种是client的fd  一种是wakeup
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel *channel : activeChannels_)
        {
            //poller监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainloop accept fd <= channel  subloop
         * mainloop事先注册一个回调cb，需要subloop执行  
         * wakeup subloop后执行下面的方法 执行之前mainloop注册的cb回调
         * 
        */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping,\n",this);
    looping_ = false;
}

//退出事件循环
//1. loop在自己的线程中调用quit
//2. 在其他线程中调用的quit（在一个subloop（woker）中，调用了mainloop（IO）的quit）
/*
                mainloop

    ****************************** 生产者-消费者的线程安全的队列（no）

    subloop1     subloop2     subloop3

*/
void EventLoop::quit()
{
    quit_ = true;

     // 如果是在其它线程中，调用的quit   在一个subloop(woker)中，调用了mainLoop(IO)的quit
    if(!isInLoopThread())
    {
        wakeup();
    }
}


//在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())//在当前的loop线程中，执行cb
    {
        cb();
    }
    else //在非当前loop执行cb，就需要唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }

}

//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应的需要执行上面回调操作的loop的线程了
    //callingPendingFunctors_=TRUE表示当前的loop正在执行回调 doPendingFunctors
    //执行完又阻塞poller_->poll(kPollTimeMs,&activeChannels_);
    //若有添加新的回调pendingFunctors_.emplace_back(cb);
    // 在唤醒一下后执行新的回调doPendingFunctors
    if(!isInLoopThread() || callingPendingFunctors_) 
    {
        wakeup();
    }
}

//发送给subreactor一个读信号，唤醒subreactor
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8",n);
    }
}

//用来唤醒loop所在地线程的 向wakeupfd_写一个数据,
//wakeupchannel就发生读事件，当前loop线程就被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

//EventLoop的方法=> poller的方法
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

//执行回调的
void EventLoop::doPendingFunctors()
{
    //使用一个局部的vector和pendingFunctors_的交换，
    //避免了因为要读取的时候，没有释放锁
    //当有新的事件往里写得时候写不进去(mainloop向subloop里面写回调)
    std::vector<Functor> functors;
    callingPendingFunctors_ = true; //需要执行回调

    //括号用于上锁 出了括号就解锁了
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor: functors)
    {
        functor();//执行当前loop需要执行的回调操作
    } 
    callingPendingFunctors_=false;
} 