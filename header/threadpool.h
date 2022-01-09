#ifndef __THREADPOLL_H
#define __THREADPOLL_H

#include "locker.h"
#include "user.h"
#include "global.h"
#include "tools.h"

struct Request
{
    int epollfd;
    int connfd;

    Request(int e, int c) : epollfd(e), connfd(c) {}
};

class Threadpool
{

public:
    Threadpool(int thread_num = 5, int max_requests = 1000);
    ~Threadpool();
    //添加工作
    bool addwork(Request *request);
    //线程执行函数
    static void *worker(void *arg);

private:
    //缓冲区大小
    const int BUF_SIZE = 1000;
    const char split_char = char(2);
    const char end_char = char(3);

private:
    //池中线程数量
    int m_thread_number;
    //最大请求数量
    int m_max_requests;
    //线程号数组
    pthread_t *m_threads;
    //工作队列
    std::list<Request *> m_workqueue;
    //工作队列的互斥锁
    locker m_queuelocker;
    //工作时的信号量
    sem m_queuestat;
    //线程的运行状态
    bool m_stop;

private:
    //线程运行函数
    void run();
    //处理用户请求
    void process(int epollfd, int connfd, char *buf, char *mes, char *temp_buf);
};

Threadpool::Threadpool(int thread_num, int max_requests) : m_thread_number(thread_num), m_max_requests(max_requests), m_threads(NULL), m_stop(false)
{
    if (thread_num <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    for (int i = 0; i < m_thread_number; ++i)
    {
        //创建线程
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

Threadpool::~Threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

bool Threadpool::addwork(Request *request)
{
    //向工作队列中添加工作，如果工作过多则丢弃
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

void *Threadpool::worker(void *arg)
{
    Threadpool *pool = (Threadpool *)arg;
    pool->run();
    return pool;
}

void Threadpool::run()
{
    //申请缓冲区
    char *buf = new char[BUF_SIZE];
    char *mes = new char[BUF_SIZE];
    char *temp_buf = new char[BUF_SIZE];

    while (!m_stop)
    {
        //线程不断地从工作队列中取工作并执行
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        Request *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (request == NULL)
            continue;

        process(request->epollfd, request->connfd, buf, mes, temp_buf);

        delete request;
    }

    delete[] buf;
    delete[] mes;
    delete[] temp_buf;
}

void Threadpool::process(int epollfd, int connfd, char *buf, char *mes, char *temp_buf)
{
    int len = recv(connfd, buf, BUF_SIZE, 0);
    if (len == 0)
    {
        //由客户断开连接，告知主线程
        len = sprintf(mes, "%d%c", 1, split_char);
        len += sprintf(mes + len, "%d%c", connfd, end_char);
        Global::pipe_spin.lock();
        write(Global::pipefd[1], mes, len);
        Global::pipe_spin.unlock();
        return;
    }

    int pos = read_1(buf, 0, temp_buf);
    int order_id = atoi(temp_buf);

    switch (order_id)
    {
    case 0:
    {
        //查询消息
        //可能的返回值：13，14
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        //发送消息时头部值为13
        while (Global::users[userid].is_have_mes())
        {
            int len = sprintf(mes, "%d%c", 13, split_char);
            string s = Global::users[userid].getmes();
            len += sprintf(mes + len, "%s", s.c_str());
            send(connfd, mes, len, MSG_MORE);
        }

        //告知消息发送完毕，头部值为14
        int len = sprintf(mes, "%d%c", 14, end_char);
        send(connfd, mes, len, 0);

        break;
    }

    case 1:
    {
        //不会出现的命令
        break;
    }

    case 2:
    {
        //注册
        //可能的返回值：6
        int userid = User_manage::user_register();
        int len = sprintf(mes, "%d%c", 6, split_char);
        len += sprintf(mes + len, "%d%c", userid, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 3:
    {
        //设置密码
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_pwd(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 4:
    {
        //设置昵称
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_name(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 5:
    {
        //登录
        //交给主线程处理，可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        int t = 0;
        if (User_manage::userid_check(userid))
        {
            pos = read_1(buf, pos, temp_buf);
            User_manage::user_get_pwd(userid, buf);
            if (strcmp(buf, temp_buf) == 0)
            {
                //可以登录则告知主线程
                int len = sprintf(mes, "%d%c", 5, split_char);
                len += sprintf(mes + len, "%d%c", userid, split_char);
                len += sprintf(mes + len, "%d%c", connfd, end_char);

                Global::pipe_spin.lock();
                write(Global::pipefd[1], mes, len);
                Global::pipe_spin.unlock();
                break;
            }

            else
                t = 9;
        }

        else
            t = 8;

        int len = sprintf(mes, "%d%c", t, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 6:
    {
        //登出
        //交给主线程处理，可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int len = sprintf(mes, "%d%c", 6, split_char);
        len += sprintf(mes + len, "%d%c", userid, split_char);
        len += sprintf(mes + len, "%d%c", connfd, end_char);
        Global::pipe_spin.lock();
        write(Global::pipefd[1], mes, len);
        Global::pipe_spin.unlock();
        break;
    }

    case 7:
    {
        //向好友发送消息
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (!User_manage::userid_check(tarid))
        {
            //用户不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        if (!User_manage::user_check_fri(userid, tarid))
        {
            //与对方已经是好友关系
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        //告知对方
        pos = read_1(buf, pos, temp_buf);
        int len = sprintf(mes, "%d%c", 12, split_char);
        len += sprintf(mes + len, "%d%c", userid, split_char);
        len += sprintf(mes + len, "%s%c", temp_buf, end_char);
        Global::users[tarid].sendmes(mes);

        len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 8:
    {
        //检查密码
        //可能的返回值：7，9
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_get_pwd(userid, buf);
        int t;
        if (strcmp(buf, temp_buf) == 0)
            t = 7;
        else
            t = 9;

        int len = sprintf(mes, "%d%c", t, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 9:
    {
        //设置生日
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_birth(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 10:
    {
        //设置个性签名
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_sig(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 11:
    {
        //设置学校
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_school(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 12:
    {
        //获取昵称
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int res = User_manage::user_get_name(userid, temp_buf);
        if (res == 1)
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        else if (res == 2)
        {
            //昵称未设置
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
        }


        else
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 13:
    {
        //获取个性签名
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int res = User_manage::user_get_sig(userid, temp_buf);
        if (res == 1)
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        else if (res == 2)
        {
            //个性签名未设置
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
        }

        else
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 14:
    {
        //获取学校
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int res = User_manage::user_get_school(userid, temp_buf);
        if (res == 1)
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        else if (res == 2)
        {
            //学校未设置
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
        }

        else
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 15:
    {
        //获取生日
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int res = User_manage::user_get_birth(userid, temp_buf);
        if (res == 1)
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        else if (res == 2)
        {
            //生日未设置
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
        }

        else
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 16:
    {
        //获取性别
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);

        int res = User_manage::user_get_gender(userid, temp_buf);
        if (res == 1)
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        else if (res == 2)
        {
            //性别未设置
            int len = sprintf(mes, "%d%c", 3, end_char);
            send(connfd, mes, len, 0);
        }

        else
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 17:
    {
        //设置性别
        //可能的返回值：7
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        User_manage::user_set_gender(userid, temp_buf);

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 18:
    {
        //添加好友
        //可能的返回值：7，8，15
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (User_manage::userid_check(tarid))
        {
            if (User_manage::user_check_fri(userid, tarid))
            {
                //两人已经是好友
                int len = sprintf(mes, "%d%c", 15, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                //告知对方
                int len = sprintf(mes, "%d%c", 10, split_char);
                len += sprintf(mes + len, "%d%c", userid, end_char);
                Global::users[tarid].sendmes(mes);
                Global::users[tarid].add_fri_request(userid);

                len = sprintf(mes, "%d%c", 7, end_char);
                send(connfd, mes, len, 0);
            }
        }

        else
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 19:
    {
        //删除好友
        //可能的返回值：3，7，8
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (User_manage::userid_check(tarid))
        {
            if (!User_manage::user_check_fri(userid, tarid))
            {
                //还未添加对方为好友
                int len = sprintf(mes, "%d%c", 3, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                //删除好友关系
                User_manage::user_del_fri(userid, tarid);
                int len = sprintf(mes, "%d%c", 7, end_char);
                send(connfd, mes, len, 0);
            }
        }

        else
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
        }
        break;
    }

    case 20:
    {
        //同意添加好友
        //可能的返回值7，8，16
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (User_manage::userid_check(tarid))
        {
            if (Global::users[userid].check_fri_request(tarid))
            {
                //成功添加好友
                Global::users[userid].erase_fri_request(tarid);
                User_manage::user_add_fri(userid, tarid);
                User_manage::user_add_fri(tarid, userid);

                //告知对方
                int len = sprintf(mes, "%d%c", 11, split_char);
                len += sprintf(mes + len, "%d%c", userid, end_char);
                Global::users[tarid].sendmes(mes);

                len = sprintf(mes, "%d%c", 7, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                //对方未申请
                int len = sprintf(mes, "%d%c", 16, end_char);
                send(connfd, mes, len, 0);
            }
        }

        else
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
        }

        break;
    }

    case 21:
    {
        //不同意添加好友
        //可能的返回值7，8，16
        pos = read_1(buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (User_manage::userid_check(tarid))
        {
            if (Global::users[userid].check_fri_request(tarid))
            {
                //删除好友申请
                Global::users[userid].erase_fri_request(tarid);

                //告知对方
                int len = sprintf(mes, "%d%c", 17, split_char);
                len += sprintf(mes + len, "%d%c", userid, end_char);
                Global::users[tarid].sendmes(mes);

                len = sprintf(mes, "%d%c", 7, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                //对方未申请
                int len = sprintf(mes, "%d%c", 16, end_char);
                send(connfd, mes, len, 0);
            }
        }

        else
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
        }

        break;
    }

    case 22:
    {
        //获取在线状态
        pos = read_1(buf, pos, temp_buf);
        int tarid = atoi(temp_buf);

        if (User_manage::userid_check(tarid))
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            if (Global::users[tarid].is_online())
                len += sprintf(mes + len, "%d%c", 18, end_char);
            else
                len += sprintf(mes + len, "%d%c", 19, end_char);
        }

        else
        {
            //账号不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
        }
    }
    }

    Global::reset_onshot(epollfd, connfd);
    return;
}

#endif