#include <my_muduo/TcpServer.h>
#include <my_muduo/logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,
            const InetAddress &addr,
            const std::string &name)
        :server_(loop,addr,name)
        ,loop_(loop)
        {
            //注册回调函数
            server_.setConnectionCallback(
                std::bind(&EchoServer::onConnection,this,std::placeholders::_1)
            );

            server_.setMessageCallback(
                std::bind(&EchoServer::onMessage,this,
                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );

            //设置合适的loop线程数量 loopThread
            server_.setThreadNum(1);
        }

    void start()
    {
        server_.start();
    }
private:
    //连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected())
        {
            LOG_INFO("Connection UP : %s",conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s",conn->peerAddress().toIpPort().c_str());
        }
    }

    //可读写事件回调
    void onMessage(const TcpConnectionPtr &conn,
                Buffer *buf,
                TimeStamp time)
        {
            std::string msg = buf->retrieveAllAsString();
            conn->send(msg);
            conn->shutdown(); //关闭写端EPOLLHUP=> closeCallback
        }
        
    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    //Acceptor non-blocking listenfd create bind
    EchoServer server(&loop,addr,"EchoServer-01");
    //listen loopthread listenfd => acceptChannel => mainLoop => subloop
    server.start();
    loop.loop(); //启动mainloop的底层pooler

    return 0;
}