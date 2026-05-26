#pragma once
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

class Buffer {
private:
    //string作为存储，可以动态扩容等
    std::string buffer_;
public:
    // 获取当前数据
    size_t size() {
        return buffer_.size();
    }
    //查看数据内容
    const char* data() {
        return buffer_.c_str();
    }
    //读取数据，自动存入
    ssize_t read_fd(int fd, int* saved_errno) {
        char temp[4096];//临时数组，最多读取4kb
        //调用recv
        ssize_t bytes_read = recv(fd, temp, sizeof(temp), 0);
        if(bytes_read > 0) {
            //读到的数据直接放置到string尾部
            //string会自动扩容
            buffer_.append(temp, bytes_read);
        }
        else if (bytes_read < 0) {
            *saved_errno = errno;
        }
        return bytes_read;
    }
    //处理完数据后删除
    void retrive(size_t len) {
        if (len <= buffer_.size()){
            buffer_.erase(0,len);//从0开始，删掉len个字符
        }
        else {
            buffer_.clear();//如果清空>现有数量，就全部清空
        }
    }
    std::string retrive_all() {
        std::string result = buffer_;//复制给result
        buffer_.clear();//清空自己
        return result;
    }
};