#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <array>
#include <map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <type_traits>
#include <unistd.h>
#include <fmt/format.h> //Cpp23标准库，Cpp11引用fmt第三方库
#include <utility>
#include <vector>
#include <string.h>//strerror用到

int main(){
    setlocale(LC_ALL, "zh_CN.UTF-8");//将报错信息转换为中文
    // 定义指针：存储getaddrinfo返回的网络地址信息链表
    struct addrinfo *addrinfo;
    // 解析IP+端口，结果存入addrinfo指针
    int res = getaddrinfo("127.0.0.1","1145",NULL,&addrinfo);
    // 错误判断：函数返回值非0表示失败
    if (res != 0){
        //perror("getaddrinfo"); //C写法
        fmt::println("getaddrinfo: {} {}",gai_strerror(res),res);//cpp11+写法
        return 1;
    }
    
    // fmt::println("{}", addrinfo->ai_family); //打印地址族（IPv4/IPv6）
    // fmt::println("{}", addrinfo->ai_socktype); //打印套接字类型（TCP/UDP）
    // fmt::println("{}", addrinfo->ai_protocol); //打印协议类型 
    
    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1){
        //EWOULDBLOCK; 排错查表用的,下面的strerror代替了手动查表的步骤
        fmt::println("socket: {}", strerror(errno));
        /* C语言写法
        perror("errno");
        */
       freeaddrinfo(addrinfo);
        return 1;
    }


    // 设置套接字选项
    int opt = 1;
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))== -1){
        fmt::println("setsockopt: {}",strerror(errno));
        close (sockfd);
        return 1;
    }
    fmt::println(("套接字选项设置成功"));
    
    // 绑定IP与端口
    if (bind(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1){
        fmt::println("bind: {}", strerror(errno));
        close (sockfd);
        return 1;
    }
    fmt::println("绑定完成");

    freeaddrinfo(addrinfo);
    // listening...
    if (listen(sockfd, SOMAXCONN) == -1){
        fmt::println("listen: {}", strerror(errno));
        close (sockfd);
        return 1;
    }
    fmt::println("等待客户端连接");

    //定义结构体，存储客户端地址
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr); //存储地址实际长度
    //accept阻塞连接
    int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);//新建描述符，和对应客户端通信
    if (client_fd == -1){
        fmt::println("accept: {}", strerror(errno));
        close(sockfd);
        return 1;
    }

    //解析客户端IP
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    //打印客户端信息
    fmt::println("IP: {}", client_ip);
    fmt::println("Port: {}", client_port);
    fmt::println("Socket: {}", client_fd);

    close(client_fd);
    close(sockfd);

    return 0;
}