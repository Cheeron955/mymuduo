#pragma once // 指定编译器在编译源代码文件时只包含头文件一次
/*
noncopyable 被继承之后，派生类对象可以正常的构造和析构，但是
派生类对象无法进行拷贝构造和赋值操作
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete; //将拷贝构造delete(禁用)掉
    noncopyable& operator=(const noncopyable&) = delete; //赋值构造delete掉
protected:
    noncopyable()= default;
    ~noncopyable()= default;
};