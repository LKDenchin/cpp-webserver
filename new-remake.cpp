#include <algorithm>
#include <arpa/inet.h>
#include <cassert>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <array>
#include <map> //引入map字典
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netdb.h>
#include <type_traits>
#include <unistd.h>
#include <fmt/format.h> // Cpp23标准库，Cpp11引用fmt第三方库
#include <utility>
#include <vector>
#include <string.h> // strerror用到
#include "buffer.hpp"

// 初始化并启动服务器
int create_server_socket(const char* ip, const char* port) {
    struct addrinfo *addrinfo;
    // 解析IP+端口
    int res = getaddrinfo(ip, port, NULL, &addrinfo);
    if (res != 0) {
        fmt::println("getaddrinfo: {} {}", gai_strerror(res), res);
        return -1;
    }
    
    int sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1) {
        fmt::println("socket: {}", strerror(errno));
        freeaddrinfo(addrinfo);
        return -1;
    }

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
        fmt::println("setsockopt: {}", strerror(errno));
        close(sockfd);
        freeaddrinfo(addrinfo);
        return -1;
    }
    fmt::println("套接字选项设置成功");
    
    // 绑定IP与端口
    if (bind(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen) == -1) {
        fmt::println("bind: {}", strerror(errno));
        close(sockfd);
        freeaddrinfo(addrinfo);
        return -1;
    }
    fmt::println("绑定完成");

    freeaddrinfo(addrinfo);

    // listening...
    if (listen(sockfd, SOMAXCONN) == -1) {
        fmt::println("listen: {}", strerror(errno));
        close(sockfd);
        return -1;
    }
    fmt::println("等待客户端连接");

    return sockfd;
}

// 等待客户端连接并打印信息
int accept_client(int sockfd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        fmt::println("accept: {}", strerror(errno));
        return -1;
    }

    // 解析客户端IP
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    
    // 打印客户端信息
    fmt::println("IP: {}", client_ip);
    fmt::println("Port: {}", client_port);
    fmt::println("Socket: {}", client_fd);

    return client_fd;
}

// 设置非阻塞
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1){
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// 处理客户端消息收发
bool handle_client_message(int client_fd, Buffer& buf) {
    int saved_errno = 0;
    //读取套接字数据
    ssize_t bytes_received = buf.read_fd(client_fd, &saved_errno);
    if (bytes_received < 0) {
        if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
            return true; //无数据可读，不是报错，继续保持连接
        }
        fmt::println("套接字 {} 接受失败 {}", client_fd, strerror(saved_errno));
        return false;
    }
    else if (bytes_received == 0) {
        fmt::println("套接字 {} 已断开连接", client_fd);
        return false;
    }
    //切分数据
    while(true) {
        std::string view(buf.data(), buf,size());
        //查询换行符
        size_t pos = view.find('\n');
        //1.未找到->半包数据
        if (pos == std::string::npos) {
            break;//不清空直接退出，保留数据在buf内
        }
        //2.找到换行符
        std::string single_message = view.substr(0, pos);
        //打印
        fmt::println("切分消息: [socket: {} ] {}", client_fd, single_message);
            //回传数据
        std::string reply = single_message + "\n";
        ssize_t bytes_sent = send(client_fd, reply.data(), reply.size(), 0);
        if (bytes_sent < 0) {
            fmt::println("套接字 {} 发送失败: {}", client_fd, strerror(errno));
            return false;
        }
        //清理处理完的数据，记得\n也要占位，所以需要删掉pos+1的数据
        buf.retrive(pos + 1);
    }
    return true;
}

int main() {
    setlocale(LC_ALL, "zh_CN.UTF-8"); // 将报错信息转换为中文

    // 1. 创建服务器
    int sockfd = create_server_socket("127.0.0.1", "1145");
    if (sockfd == -1) return 1;

    // 将监听套接字设置为非阻塞
    set_nonblocking(sockfd);
    //创建epoll
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        fmt::println("epoll_create1 失败");
        return 1;
    }
    // 监听套接字加入epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    //创建数组接受活跃事件
    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];

    //阻塞循环
    while (true) {
        //在此处阻塞，直到有新事件发生
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1){
            fmt::println("epoll_wait 错误：{}", strerror(errno));
            break;
        }
        for (int i=0;i<nfds;i++){
            int current_fd = events[i].data.fd;
            // 1.server套接字响应，新客户端进入
            if (current_fd == sockfd){
                int client_fd = accept_client(sockfd);
                if (client_fd != -1) {
                    set_nonblocking(client_fd);

                    //客户端加入监听名单
                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN;
                    client_ev.data.fd = client_fd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev); //登记新客户端
                }
            }
            // 2.客户端套接字相应，新消息进入
            else {
                if (!handle_client_message(current_fd)) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, current_fd, nullptr);//移除
                    close(current_fd);//销毁套接字
                }
            }
        }
    }
    // 4. 清理资源
    close(sockfd);
    close(epfd);
    return 0;
}