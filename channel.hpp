#pragma once
#include <functional>
#include <sys/epoll.h>
#include <fmt/format.h>

class Channel {
private:
    int fd_;    //channel负责看管的文件描述符
    uint32_t events_;    //用户关心的事件
    uint32_t revents_;   //epoll实际返回的活跃事件

    //存储
    std::function<void()> readCallback_;
    std::function<void()> writeCallback_;

public:
    //构造函数，诞生时绑定一个fd
    Channel(int fd) : fd_(fd), events_(0), revents_(0) {}
    ~Channel() = default;

    //属性相关
    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    //外层epoll_wait唤醒后，调用函数，告知channel发生事件
    void set_revents(uint32_t revt) { revents_ = revt; }
    //设置回调函数
    //下面的函数，将具体处理代码放到Channel内部
    void setReadCallback(std::function<void()> cb) { readCallback_ = cb; }
    void setWriteCallback(std::function<void()> cb) { writeCallback_ = cb;}
    //开启读事件监听（EPOLLIN）
    void enableReading() {
        events_ |= EPOLLIN;
    }
    void handleEvent() {
        if (revents_ & EPOLLIN) {
            if (readCallback_) {
                readCallback_(); // 执行回调函数
            }
        }
        
        // 如果以后有写事件（EPOLLOUT），就在这里扩展
        if (revents_ & EPOLLOUT) {
            if (writeCallback_) {
                writeCallback_();
            }
        }
    }
};