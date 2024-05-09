# pragma once

#include <iostream>
#include <string>
#include <stdint.h>

/*时间类*/
class TimeStamp
{
private:
    int64_t microSecondsSinceEpoch_;
public:
    TimeStamp(); //默认构造
    explicit TimeStamp(int64_t microSecondsSinceEpoch); //带参构造
    static TimeStamp now();
    std::string toString() const; 
};
