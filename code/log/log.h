/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level, const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256; // 日志路径的长度
    static const int LOG_NAME_LEN = 256; // 日志名字的长度
    static const int MAX_LINES = 50000; // 日志的长度

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_; // 记录写的行数
    int toDay_; // 记录当前的日期

    bool isOpen_;
 
    Buffer buff_; // 同读写使用的Buffer
    int level_;
    bool isAsync_;

    FILE* fp_;
    std::unique_ptr<BlockDeque<std::string>> deque_;  // 阻塞队列
    std::unique_ptr<std::thread> writeThread_; // 写的子线程
    std::mutex mtx_; // 互斥锁
};

// 定义宏函数
// 判断GetLevel()<=level，判断是否在写日志的范围内
// 调用log->write写
// 调用log->flush刷新
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H