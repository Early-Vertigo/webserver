/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 
#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
} 

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

// id-文件描述符 timeout-超时的时间 cb-超时要做的事情
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    // 找文件描述符有没有
    // =0 时没有，是新节点，插入
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        // 获取id是文件描述符
        i = heap_.size();
        ref_[id] = i;
        // 封装{id, Clock::now() + MS(timeout), cb}
        // 超时时间的设置 = 当前时间+现下计时的时间
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        // 新节点需要往上调整，siftup_
        siftup_(i);
    } 
    else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        // 已有节点需要和同级节点和子节点比较，siftdown_
        if(!siftdown_(i, heap_.size())) {
            // 如果没有的话就向上调整
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    // 比如，在原先的计时内进行了通信，则需要重新计时，进而需要重新调整id的结点
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    // 为什么siftdown_，因为时间变大了
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        // 获取第一个，对比第一个节点和当前时间的差值是否大于0，是的话就没有超时，
        // 直到找到<=0是已经超时的，那么就是超时了，执行cb，并且弹出该节点
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

// 获取下一个清除的结点
int HeapTimer::GetNextTick() {
    // tick()清除超时的节点，重新调整结点
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        // 再计算剩余时间最大的节点，得到小根堆最宽裕时间的长度，返回到webserver.cpp中的Start()的epoller_->Wait(timeMs)的timeMs
        // epoll_wait参数使用timeMs，如果timeMs内没有事件发生，则解除阻塞，否则epoller_wait不设置timeout的话就不会解除阻塞，会一直等待事件发生
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}