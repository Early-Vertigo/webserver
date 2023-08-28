/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */

#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    {
    srcDir_ = getcwd(nullptr, 256); // 获取当前的工作路径，返回char*
    assert(srcDir_); // 做判断，断言的
    strncat(srcDir_, "/resources/", 16); // 拼接，即从当前工作路径拼接下属的resources文件夹
    HttpConn::userCount = 0; // HttpConn对象用于保存连接的客户端信息，userCount
    HttpConn::srcDir = srcDir_; // srcDir
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 初始化事件模式
    InitEventMode_(trigMode);
    // 初始化套接字socket
    if(!InitSocket_()) { isClose_ = true;} // 如果初始化socket失败则关闭服务器
    // 正常情况下，socket初始化成功，则开始监听描述符，注意是否有客户端连接
    // 判断是否打开日志
    if(openLog) {
        // Log::Instance()->init获取实例并初始化
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

// 设置监听的文件描述符和通信的文件描述符的模式
void WebServer::InitEventMode_(int trigMode) {
    // 监听文件描述符设置的事件，EPOLLRDHUP，检测对方是否正常关闭；不再使用ret检测是否==-1
    listenEvent_ = EPOLLRDHUP;
    // 
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        // ET模式
        connEvent_ |= EPOLLET;
        break;
    case 2:
        // ET模式
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        // ET模式
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        // 
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    // 按位与操作，检测是否是ET检测方式
    HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    // 打印日志
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    // while死循环，只要服务器不关闭，就一直运行
    while(!isClose_) {
        // 解决超时连接
        // 在heaptimer.cpp中
        // 计算剩余时间最大的节点，得到小根堆最宽裕时间的长度，返回到webserver.cpp中的Start()的epoller_->Wait(timeMs)的timeMs
        // epoll_wait参数使用timeMs，如果timeMs内没有事件发生，则解除阻塞，否则epoll_wait不设置timeout的话就不会解除阻塞，会一直等待事件发生
        // 这里的事件是DealRead_和DealWrite_，只要这些事件发生，就会解除阻塞，如果这些事件没有发生，那么就会在超时事件后解除阻塞
        if(timeoutMS_ > 0) {
            timeMS = timer_->GetNextTick();
        }
        // 通过封装的epoll_wait获取检测事件的个数
        int eventCnt = epoller_->Wait(timeMS);
        // 遍历事件
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i); // 获取fd
            uint32_t events = epoller_->GetEvents(i); // 获取事件
            if(fd == listenFd_) {
                DealListen_(); // 处理事件监听，建立新连接
            }
            // 出现特定错误
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]); // 关闭对象
            }
            // 事件不是监听的，并且是EPOLLIN
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 处理读操作
            }
            // 事件不是监听的，并且是EPOLLOUT
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]); // 处理写操作
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    users_[fd].init(fd, addr); // users_是哈希表集合，保存的个数以及客户端信息
    // 如果超时
    if(timeoutMS_ > 0) {
        // 超时则通过add调用CloseConn_断联
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    // 对新连接的客户端监听是否有数据到达，所以EPOLLIN
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听事件
void WebServer::DealListen_() {
    struct sockaddr_in addr; // 保存连接的客户端的信息
    socklen_t len = sizeof(addr); // 获取len，accept使用
    // 为什么先do，先获取到所有的连接客户端的描述符，再进行监听事件和ET的判断
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { return;} // fd<=0，失败或断联
        else if(HttpConn::userCount >= MAX_FD) { // 连接数量>=最大预设数
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        // 添加客户端
        AddClient_(fd, addr);
    } while(listenEvent_ & EPOLLET); // ET模式下非阻塞
}

// 读操作是交给工作模块(子线程)操作，是Reactor
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    // 线程池添加任务，添加的是，Onread_操作
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// client(客户端)
// 子线程中操作的
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读取客户端的数据
    // 如果ret显示错误，并且错误号不是EAGAIN，则是错误的情况，关闭该客户端的连接
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    // ret正常，则正常处理read操作，业务逻辑的处理
    OnProcess(client);
}

void WebServer::OnProcess(HttpConn* client) {
    // 调用client的process()处理业务逻辑
    if(client->process()) {
        // 修改业务逻辑成功，修改client的Fd，改为EPOLLOUT等待写，回到主线程的客户端检测，检测到写则变为OnWrite_
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    // 创建客户端信息存储的结构体
    struct sockaddr_in addr;
    // 判断端口
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    // 
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);//htonl字节序转换
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    // 创建监听描述符
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }
    // 
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    // 绑定，并检测ret是否错误
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    // 监听
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    // 调用AddFd，
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    // 设置非阻塞
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    // 获取原先的值 int flag = fcntl(fd, F_GETFD, 0)
    // 按位或 flag = flag | O_NONBLOCK , 该操作等于 flag |= O_NONBLOCK;
    // 再赋值给flag
    // 变为 fcntl(fd, F_SETFL, flag)
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


