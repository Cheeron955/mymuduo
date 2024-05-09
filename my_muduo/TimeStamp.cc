#include "TimeStamp.h"
#include <time.h>
#include <stdio.h>

TimeStamp::TimeStamp():microSecondsSinceEpoch_(0) {}// 默认构造


TimeStamp::TimeStamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {}// 带参构造

TimeStamp TimeStamp::now()
{
    return TimeStamp(time(NULL));    //获取当前时间
}

std::string TimeStamp::toString() const
{
    char buf[128]={0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, 128, "%4d/%02d/%02d %02d:%02d:%02d",
    tm_time->tm_year+1900,
    tm_time->tm_mon+1,
    tm_time->tm_mday,
    tm_time->tm_hour,
    tm_time->tm_min,
    tm_time->tm_sec);
    return buf;
}

// #include <iostream>
// int main()
// {
//     std::cout<<TimeStamp::now().toString()<<std::endl;
//     return 0;
// }