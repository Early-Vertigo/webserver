/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft Apache 2.0
 */

#include "epoller.h"

// 创建epoll对象，events_是检测事件的集合
Epoller::Epoller(int maxEvent):epollFd_(epoll_create(512)), events_(maxEvent){
    assert(epollFd_ >= 0 && events_.size() > 0);
}
// 析构函数，关闭epoll对象
Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::AddFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    // 创建epoll_event初始化为0
    epoll_event ev = {0};
    // 对结构体输入数据
    ev.data.fd = fd;
    ev.events = events;
    // epoll_ctl操作用于添加
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::ModFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::DelFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

// 封装epoll_wait
int Epoller::Wait(int timeoutMs) {
    // epoll_wait中epollFd_就是创建的epoll对象
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}
// 获取第i个元素的事件的描述符
int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].data.fd;
}
// 获取第i个元素的事件
uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);
    return events_[i].events;
}