/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "buffer.h"

// 构造函数，给buffer_(vector)预先设好的大小，当前读写位置都为0
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 对vector<char> Buffer获取到当前可读的大小，由写位-读位得到
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}
// 对vector<char> Buffer获取到当前可写的大小
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

size_t Buffer::PrependableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    // 更新已写位置
    writePos_ += len;
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}
// Append(buff, len - writable); buff临时数组，len-writable是临时数组中的数据个数
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len); // 检查空间，接下来采用两种策略：1.利用临时buff读取；2.利用buffer的已读内容空间重新利用读取
    // copy到起始写的位置
    std::copy(str, str + len, BeginWrite());
    // 更新已写位置
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}
// EnsureWriteable(len);
void Buffer::EnsureWriteable(size_t len) {
    // 判断可写的字节数是否小于len，是的话需要创建空间
    if(WritableBytes() < len) {
        MakeSpace_(len);// 需要对vector<char> Buffer进行扩容
    }
    assert(WritableBytes() >= len);
}
// 
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];//临时的buff，保证能够把所有的数据都读出来
    struct iovec iov[2];//结构体数组[2]，第一个元素[0]是结构体的iov_base是vector，第二个iov_len是临时的buff[65535]
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    // writePos_初始化为0，根据单词读完的大小，再加上BeginPtr()就可以得到当前vector-BUffer的索引
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    // readv分散读，第三个参数是表示有几个要读的容器
    const ssize_t len = readv(fd, iov, 2); // len是读到的字节数
    if(len < 0) {
        // 关闭连接
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        // writable表示可写的字节数，为buffer_size()-writePos_;
        // else if判断当前要读的内容是否小于等于可写的空间，可以则写入，并刷新writePos_的位置
        writePos_ += len;
    }
    else {
        // else说明要读的内容大于可写的空间，则buffer将无法完全容纳，需要将临时buff[65535]的内容纳入
        writePos_ = buffer_.size(); // 刷新writePos_，刷新到Buffer队尾
        Append(buff, len - writable); // 添加(属于buffer扩充的核心部分)
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}
// MakeSpace_(len);对Buffer进行扩容
void Buffer::MakeSpace_(size_t len) {
    // PrependableBytes()-前面可以用的空间，readPos_前面的空间都是已读过的，可以用作扩充
    if(WritableBytes() + PrependableBytes() < len) {
        // 按照buffer当前的情况，可写大小+已读过的(可支持扩充)的大小<len
        buffer_.resize(writePos_ + len + 1);
        // 对容器进行stl操作，扩容
    } 
    else {
        // else表示可写大小+已读过的(可支持扩充)的大小足够支撑读取剩余的数据
        size_t readable = ReadableBytes(); // 获取可读的大小
        // BeginPtr_()+(writePos_-readPos_)，由于不扩充vector，而是利用已有的空间，则新的可用空间等于=已写到的位置-已读到的位置
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;// 由于利用vector前面的空间，重置readPos_
        writePos_ = readPos_ + readable; // 已写到的位置刷新
        assert(readable == ReadableBytes());
    }
}