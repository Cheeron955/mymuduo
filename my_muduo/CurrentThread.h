#pragma once


#include <unistd.h>
#include <sys/syscall.h>

/*
获取当前线程的线程id
*/

namespace CurrentThread
{
    extern __thread int t_cachedTid;

    //因为tid的访问为系统调用，总是从用户空间切到内核空间是浪费效率
    //所以liycacheTid()在第一次访问以后就把当前线程的tid给存储起来
    void cacheTid(); 


    inline int tid()
    {
        if(__builtin_expect(t_cachedTid == 0,0)) //
        {
            cacheTid();
        }
        return t_cachedTid;
    }

}