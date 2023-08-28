/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        // 读数据，并存储在readBuff_中，readBuff在httpconn对象中
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            break; // 缓冲区内容为0
        }
    } while (isET); // 判断是否是ET模式，是ET模式则需要一次性读完直到缓冲区内容为0
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        // iov_,iovCnt都在httpresponse.h中处理
        len = writev(fd_, iov_, iovCnt_);
        if(len <= 0) {
            *saveErrno = errno;
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

// HttpConn是连接，对应请求和相应
bool HttpConn::process() {
    // Http request初始化，封装为request对象
    request_.Init();
    // 检查是否有数据可读
    if(readBuff_.ReadableBytes() <= 0) {
        // 判断readBuff可读的字节数是否小于等于0，是的话不需要解析
        return false;
    }
    // 调用parse解析readBuff_(重点！)
    else if(request_.parse(readBuff_)) {
        LOG_DEBUG("%s", request_.path().c_str());
        // 解析成功，初始化响应
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);// response_响应，即生成的响应对象
        // 因为是解析成功，所以状态码为200
    } else {
        // 解析失败，状态码为400
        response_.Init(srcDir, request_.path(), false, 400);
    }
    // 放在writeBuff_中，响应的缓冲区
    response_.MakeResponse(writeBuff_);// 创造响应，数据保存在writeBuff_(因为响应是在请求被读取存储在readBuff_后解析之后发送的，存储在writeBuff_)
    // read请求的时候分散读，write响应的时候也是分散写
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;// 内存块的大小为2
    }
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
