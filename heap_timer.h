#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include <vector>
#include <algorithm>
#include "../log/log.h"
#include "../http/http_conn.h"
using namespace std;
class util_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    util_timer *timer;
    int heap_idx;
};

class util_timer
{
public:
    time_t expire;
    void (*cb_func)(client_data*);
    client_data *user_data;
    int heap_idx;
    util_timer():heap_idx(-1){}
};

class heap_timer
{
private:
    vector<util_timer*>heap_;
    void sift_up(int idx)
    {
        while ((idx>0))
        {
            int parent=(idx-1)/2;
            if(heap_[parent]->expire<heap_[idx]->expire) break;
            swap(heap_[idx],heap_[parent]);
            idx=parent;
        }
        
    }

    void sift_down(int idx,int n)
    {
         while (idx * 2 + 1 < n) {
            int left = idx * 2 + 1;
            int right = idx * 2 + 2;
            int min_child = left;
            
            if (right < n && heap_[right]->expire < heap_[left]->expire) {
                min_child = right;
            }
            
            if (heap_[idx]->expire <= heap_[min_child]->expire) break;
            
            swap_node(idx, min_child);
            idx = min_child;
        }
    }

    void swap_node(int idx1,int idx2)
    {
        swap(heap_[idx1],heap_[idx2]);
        heap_[idx1]->heap_idx=idx1;
        heap_[idx2]->heap_idx=idx2;
    }

    void pop_top()
    {
        if (heap_.empty()) return;
        
        util_timer* top = heap_[0];
        delete top;
        
        if (heap_.size() > 1) {
            heap_[0] = heap_.back();
            heap_[0]->heap_idx = 0;
            heap_.pop_back();
            sift_down(0, heap_.size());
        } else {
            heap_.pop_back();
        }
    }

    void clear()
    {
        for (auto& timer : heap_) {
            delete timer;
        }
        heap_.clear();
    }

public:
    heap_timer() {}

    ~heap_timer()
    {
        clear();
    }

    void add_timer(util_timer* timer)
    {
        if (!timer) return;
        
        timer->heap_idx = heap_.size();
        heap_.push_back(timer);
        sift_up(heap_.size() - 1);
    }

    void adjust_timer(util_timer* timer)
    {
        if (!timer || timer->heap_idx < 0 || 
            timer->heap_idx >= static_cast<int>(heap_.size())) 
            return;
            
        // 根据新位置决定上浮或下沉
        if (timer->heap_idx > 0 && 
            heap_[(timer->heap_idx-1)/2]->expire > heap_[timer->heap_idx]->expire) {
            sift_up(timer->heap_idx);
        } else {
            sift_down(timer->heap_idx, heap_.size());
        }
    }

    void del_timer(util_timer* timer)
    {
        if (!timer || timer->heap_idx < 0 || 
            timer->heap_idx >= static_cast<int>(heap_.size())) 
            return;
            
        // 延迟删除策略：标记回调函数为空
        timer->cb_func = nullptr;
    }

    void tick()
    {
        time_t cur = time(nullptr);
        while (!heap_.empty()) {
            util_timer *top = heap_[0];
            if (top->expire > cur) break;
            
            // 执行有效回调
            if (top->cb_func) {
                top->cb_func(top->user_data);
            }
            
            // 移除堆顶元素
            pop_top();
        }
    }
};

class Utils
{
public:
    static int*u_pipefd;
    heap_timer m_timer_heap;
    static int u_epollfd;
    int m_TIMESLOT;
    Utils(){}
    ~Utils(){}
    void init(int timeslot)
    {
        m_TIMESLOT=timeslot;
    }
    
    int setnonblocking(int fd)
    {

    }

    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
    {

    }

    static void sig_handler(int sig)
    {

    }

    void addsig(int sig, void(handler)(int), bool restart = true)
    {

    }

    void timer_handler()
    {

    }

    void show_error(int connfd, const char *info)
    {

    }

};

// 静态成员初始化
int *Utils::u_pipefd = 0;  // 初始化为空指针
int Utils::u_epollfd = 0;  // 初始化为0

// 定时器回调函数（超时处理函数）
// 参数: user_data - 关联的客户端数据
void cb_func(client_data *user_data) {
    // 从epoll中删除该socket描述符
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    // 确保用户数据有效
    assert(user_data);
    // 关闭socket连接
    close(user_data->sockfd);
    // 减少当前用户连接计数
   // http_conn::m_user_count--;
}
#endif