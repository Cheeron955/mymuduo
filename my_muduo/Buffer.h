#pragma once

#include <vector>
#include <string.h>
#include <stddef.h>
#include <string>
#include <algorithm>

//网络库底层的缓冲期定义
class Buffer
{
public:

    static const size_t kCheapPrepend = 8;  //缓冲区头部
    static const size_t kInitialSize = 1024; //缓冲区读写初始大小

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(initialSize + kCheapPrepend)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
        {

        }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    size_t writerableBytes() const { return buffer_.size() - writerIndex_; }

    size_t prependableBytes() const { return readerIndex_; }

    const char* peek() const
    {
        return begin() + readerIndex_; //返回缓冲区中可读数据的起始地址
    }

    //onMessage string <= Buffer
    void retrieve(size_t len) //len表示已经读了的
    {
        if(len < readableBytes()) 
        {
            //已经读的小于可读的，只读了一部分len
            //还剩readerIndex_ += len 到 writerIndex_
            readerIndex_ += len; 
        }
        else //len == readableBytes()
        {
            retrieveAll();
        }
    }

    void retrieveAll() //都读完了
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());//应用可读取数据的长度
    }

    std::string  retrieveAsString(size_t len)
    {
        std::string result(peek(),len); //从起始位置读len长
        retrieve(len);//上面一句吧缓冲区可读的数据读出来，这里对缓冲区进行复位操作
        return result;
    }

    //buffer.size - writerIndex_ 可写的  len
    void ensureWriterableBytes(size_t len)
    {
        if (writerableBytes() < len)
        {
            makeSpace(len); //扩容
        }     
    }

    //把[data ,data+len]内存上的数据，添加到writeable缓冲区当中
    void append(const char* data, size_t len) //添加数据
    {
        ensureWriterableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite() {return begin() + writerIndex_; }
    const char* beginWrite() const {return begin() + writerIndex_; }

    //从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);

    //通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin()
    {
        //it.operator*() 首元素  it.operator*().operator&() 首元素的地址
        return &*buffer_.begin(); //vector底层数组元素的地址，也就是数组的起始地址
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

/**
 kCheapPrepend | reader | writer
 kCheapPrepend |      len         |
*/
    void makeSpace(size_t len)
    {
        if (prependableBytes() + writerableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes(); //保存一下没有读取的数据
            std::copy(begin()+readerIndex_
                , begin()+writerIndex_
                , begin()+ kCheapPrepend); //挪一挪
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_+readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};

