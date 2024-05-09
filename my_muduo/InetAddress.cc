#include "InetAddress.h"

#include <strings.h>
#include <string.h>
#include <stdio.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);//主机字节序到网络字节序
    addr_.sin_addr.s_addr = inet_addr(ip.c_str()); //字符串转成整型数据
}

std::string InetAddress::toIp() const
{
    //addr_ 读取ip地址,转换成点分十进制
    char buf[64] ={0};
    ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof buf);
    return buf;
}
    
std::string InetAddress::toIpPort() const
{
    //ip:port 
    char buf[64] ={0};
    ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof buf);
    size_t end=strlen(buf); //ip的有效长度
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf+end,":%u", port);
    return buf;
}
   
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);

}

// #include <iostream>
// int main()
// {

//     InetAddress addr(8080);
//     std::cout<<addr.toIpPort()<<std::endl;
//     return 0;
// }