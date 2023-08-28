/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 
#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    // 构造函数，最大检测事件的数量
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    // 对检测事件的epoll中加入新的连接对象(客户端)
    bool AddFd(int fd, uint32_t events);

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    // 调用内核检测
    int Wait(int timeoutMs = -1);
    // 获取事件的fd
    int GetEventFd(size_t i) const;
    // 获取事件
    uint32_t GetEvents(size_t i) const;
        
private:
    // epoll_create()创建epoll对象，返回值就是epollfd，用于操作epoll对象
    int epollFd_;
    // 检测到的事件的集合
    std::vector<struct epoll_event> events_;    
};

#endif //EPOLLER_H