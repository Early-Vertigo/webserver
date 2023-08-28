/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex> // 互斥锁
#include <condition_variable> // 条件变量
#include <queue> // queue容器
#include <thread> // C++的线程库
#include <functional>
class ThreadPool {
public:
    // 构造函数
    // explicit:防止构造函数临时转换，必须以构造函数的方式创建
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            // 默认8个线程，初始化pool成员，assert(做断言)

            // 创建指定个数的线程池
            for(size_t i = 0; i < threadCount; i++) {
                // 
                std::thread([pool = pool_] {
                    std::unique_lock<std::mutex> locker(pool->mtx); // locker锁
                    // while死循环
                    while(true) {
                        // 任务队列不为空
                        if(!pool->tasks.empty()) {
                            // 从任务队列取头部的任务
                            auto task = std::move(pool->tasks.front());
                            // 移除容器中的该任务
                            pool->tasks.pop();
                            // unlock(解锁，为的是while的上一个循环的lock解锁)
                            locker.unlock();
                            // task()，任务执行的代码
                            task();
                            // lock加锁，等待下一次while循环解锁
                            locker.lock();
                        } 
                        else if(pool->isClosed) break; // pool被关闭，则断开while循环，并detach()
                        else pool->cond.wait(locker); // 设置cond的阻塞，当AddTask运行后，唤醒线程(至于是哪个线程被唤醒取决于实时情况)
                    }
                }).detach(); // 线程分离
            }
    }

    // 无参构造函数，用于默认实现
    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    // 析构函数
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true; // 关闭该线程池
            }
            pool_->cond.notify_all(); // 唤醒所有cond.wait(locker)的线程，使所有的线程结束while循环后detach()，回收资源 
        }
    }

    // 模板
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }

private:
    // 私有成员Pool，线程池结构体
    struct Pool {
        // 互斥锁
        std::mutex mtx;
        // 条件变量
        std::condition_variable cond;
        // bool值-是否关闭
        bool isClosed;
        // 保存任务的队列
        std::queue<std::function<void()>> tasks;
    };
    // 线程池
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H