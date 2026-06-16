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
#include <memory>
#include "buffer.hpp"
#include "channel.hpp"

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
        std::string view(buf.data(), buf.size());
        //查询换行符
        size_t pos = view.find("\r\n\r\n");
        //1.未找到->半包数据
        if (pos == std::string::npos) {
            break;//不清空直接退出，保留数据在buf内
        }
        // 成功提取出一个完整的 HTTP 请求头
        std::string request_header = view.substr(0, pos);
        fmt::println("收到 HTTP 请求:\n{}", request_header);
        // 构造 HTTP 响应报文
        // 1. 准备要在浏览器里显示的 HTML 内容
        std::string body = "<html><head><title>Web Server</title></head>"
                           "<body style='font-family: Arial, sans-serif; text-align: center; margin-top: 50px;'>"
                           "<h1>Welcome to LKDenchin's Web Server!</h1>"
                           "<p>如果你能看到这个页面，说明解析成功了！</p>"
                           "</body></html>";
        
        // 2. 组装 HTTP 报文格式
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html; charset=utf-8\r\n"
                               "Content-Length: " + std::to_string(body.size()) + "\r\n"
                               "Connection: close\r\n"  // 让浏览器请求完就断开连接（短连接）
                               "\r\n"                   // 头部和主体之间的空行
                               + body;

        // 3. 发送给浏览器
        ssize_t bytes_sent = send(client_fd, response.data(), response.size(), 0);
        if (bytes_sent < 0) {
            fmt::println("套接字 {} 发送失败: {}", client_fd, strerror(errno));
            return false;
        }

        // 清理水桶中已处理的数据。\r\n\r\n 一共占据了 4 个字节，所以要删掉 pos + 4
        buf.retrive(pos + 4);

        // 对于简单的短连接 HTTP，发完响应就主动关闭连接
        return false; 
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
    Channel server_channel(sockfd);
    server_channel.enableReading(); // 开启监听读事件
    // 监听套接字加入epoll
    struct epoll_event ev;
    ev.events = server_channel.events();
    ev.data.ptr = &server_channel; // 将Channel对象的地址存储在事件数据中,不再存fd
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    //创建数组接受活跃事件
    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];
    std::map<int, Buffer> client_buffers; //声明map
    std::map<int, std::unique_ptr<Channel>> client_channels; //存储客户端的Channel对象
    std::vector<int> channels_to_delete; //存储需要删除的Channel对象的fd

    server_channel.setReadCallback([&](){
        int client_fd = accept_client(sockfd);
        if (client_fd != -1){
            set_nonblocking(client_fd);
            //新客户端单独buffer
            client_buffers[client_fd];
            //创建channel
            auto client_chan = std::make_unique<Channel>(client_fd);
            client_chan->enableReading();
            //channel可读回调
            client_chan->setReadCallback([client_fd, epfd, &client_buffers, &channels_to_delete](){
                //返回false,延迟销毁
                if (!handle_client_message(client_fd, client_buffers[client_fd])){
                    channels_to_delete.push_back(client_fd);//先不销毁
                }
            });

            struct epoll_event client_ev;
            client_ev.events = client_chan->events();
            client_ev.data.ptr = client_chan.get();//存channel裸指针 
            epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev);

            client_channels[client_fd] = std::move(client_chan);//由unique_ptr移入map
        }
    });
    //阻塞循环
    while (true) {
        //在此处阻塞，直到有新事件发生
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1){
            fmt::println("epoll_wait 错误：{}", strerror(errno));
            break;
        }
        //分发事件：直接取出 Channel 指针，调用 handleEvent()
        for (int i = 0; i < nfds; i++) {
            Channel* channel = static_cast<Channel*>(events[i].data.ptr);
            channel->set_revents(events[i].events);
            channel->handleEvent();         // Channel 自己调用绑定的回调
        }
        //安全清理已断开的连接（在所有事件处理完之后）
        for (int fd : channels_to_delete) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);   // 从 epoll 移除
            client_buffers.erase(fd);                       // 释放缓冲区
            client_channels.erase(fd);
            close(fd);                                      // 关闭套接字
            fmt::println("套接字 {} 的资源已安全清理销毁", fd);
        }
        channels_to_delete.clear();
    }
    // 4. 清理资源
    close(sockfd);
    close(epfd);
    return 0;
}