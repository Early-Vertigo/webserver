/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 

#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    // 初始化Buffer为1024
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;//可写       
    size_t ReadableBytes() const ;//可读
    size_t PrependableBytes() const;// 在buffer空间不够时，采用策略2(利用buffer先前已经读取过的空间重复利用)时，计算得到的可重复利用的大小

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();//char指针
    const char* BeginPtr_() const;//
    void MakeSpace_(size_t len);//创建新的空间

    std::vector<char> buffer_;//具体装数据的vector
    std::atomic<std::size_t> readPos_;// 读的位置(目前)
    std::atomic<std::size_t> writePos_; // 目前写的位置
};

#endif //BUFFER_H