#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0); //静态成员变量 要在类外单独进行初始化


Thread::Thread(ThreadFunc func,const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}


Thread::~Thread()
{
    if (started_ && !joined_) //线程已经运行起来了，并且没有joined_
    {
        thread_->detach(); //thread类提供的分离线程的方法
    }
}

void Thread::start() //一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem,false,0);//初始值为0

    //开启线程 labmle表达式
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        //获取线程tid值
        tid_ = CurrentThread::tid();

        sem_post(&sem); //给信号量资源+1，说明tid_已经有了

        //开始一个新线程，专门执行该线程函数
        func_();

    }));

    //必须等待获取上面新线程的值线程tid_值
    sem_wait(&sem); //阻塞住
}

void Thread::join()
{
    joined_= true;
    thread_->join();
}


void Thread::setDefaultName() //初始化名字
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf,sizeof buf,"Thread%d",num);
        name_=buf;
    }
}