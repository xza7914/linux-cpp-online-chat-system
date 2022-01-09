#ifndef __GLOBAL_H
#define __GLOBAL_H

#include <stdio.h>
#include <exception>
#include <pthread.h>
#include <list>
#include <iostream>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unordered_map>
#include "user.h"
#include "locker.h"

class Global {
public:
    //最大用户数量
    static const int MAX_USER_NUM = 10000;

public:
    Global() = default;
    ~Global();

public:
    //用于线程通信的管道
    static int pipefd[2];
    //用于互斥访问管道的自旋锁
    static spinlock pipe_spin;
    //管理用户的User对象数组
    static User users[MAX_USER_NUM];
    //socket文件描述符到该连接上登录用户账号的映射
    static unordered_map<int, int> c_to_uid;
    //互斥使用Global函数的自旋锁
    static spinlock g_spin;

public:
    //在epoll上注册事件
    static void addfd(int epollfd, int fd, uint32_t ets);
    //在epoll上删除事件
    static void delfd(int epollfd, int fd);
    //重置epoll上的事件
    static void reset_onshot(int epollfd, int fd);
    //将文件描述符设置为非阻塞
    static void setnonbloack(int fd);
};

int Global::pipefd[2];
spinlock Global::pipe_spin;
User Global::users[MAX_USER_NUM];
unordered_map<int, int> Global::c_to_uid;
spinlock Global::g_spin;

void Global::delfd(int epollfd, int fd)
{
    g_spin.lock();
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    g_spin.unlock();
}

void Global::addfd(int epollfd, int fd, uint32_t ets)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ets;

    g_spin.lock();
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    g_spin.unlock();

    setnonbloack(fd);
}

void Global::reset_onshot(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    g_spin.lock();
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    g_spin.unlock();
}

void Global::setnonbloack(int fd)
{
    int oldop = fcntl(fd, F_GETFL);
    int newop = oldop | O_NONBLOCK;
    fcntl(fd, F_SETFL, newop);
}

#endif