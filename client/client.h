#ifndef __CLIENT_H
#define __CLIENT_H

#include <iostream>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "data.h"
#include "../header/tools.h"
using namespace std;

class Client
{
    const int BUF_SIZE = 1000;
    const char split_char = char(2);
    const char end_char = char(3);

public:
    Client();
    ~Client();

private:
    //客户端运行开关
    int client_live;
    // ip地址
    const char *ip;
    //端口号
    int port;

    //epoll文件描述符
    int epollfd;
    epoll_event *events;
    //连接号
    int connfd;
    //管道
    static int pipefd[2];
    //计时线程号
    pthread_t pid;
    //已登录的id
    int user_id;
    //是否在线
    bool online;

    //缓冲区
    char *mes;
    char *recv_buf;
    char *buf;
    char *temp_buf;

    int recv_buf_now;
    //函数返回值
    int ret;

    Trie trie;

public:
    void run();

private:
    //初始化
    void init();
    //解析用户命令
    void parse_from_user();
    //解析服务器消息
    void parse_from_ser();
    //在epoll上注册事件
    void addfd(int epollfd, int fd, uint32_t ets = EPOLLIN);
    //在epoll上删除事件
    void delfd(int epollfd, int fd);
    //计时函数
    static void *time_clock(void *arg);
    //设置密码
    void setpwd(int userid);
    //注册并获取id
    int user_register();
    //登录
    void login();
    //检验用户名是否合法
    bool check_name_legal(string s);
    //检验密码是否合法
    bool check_pwd_legal(string s);
    //检验出生日期是否合法
    bool check_date_legal(string s);
    //检验学校是否合法
    bool check_school_legal(string s);
    //检验个性签名是否合法
    bool check_sig_legal(string s);
    //验证账号是否合法
    int check_userid_legal(string s);
    //验证性别是否合法
    bool check_gender_legal(string s);
};

void Client::addfd(int epollfd, int fd, uint32_t ets)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ets;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void Client::delfd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

void *Client::time_clock(void *arg)
{
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    while (1)
    {
        gettimeofday(&t1, NULL);
        long u1 = t0.tv_sec * 1000000 + t0.tv_usec;
        long u2 = t1.tv_sec * 1000000 + t1.tv_usec;
        //计时间隔设为500ms
        if (u2 - u1 >= 500000)
        {
            t0 = t1;
            write(pipefd[1], "1", 1);
        }
    }
}

Client::Client()
{
    client_live = true;
    ip = "127.0.0.1";
    port = 5005;
    mes = new char[BUF_SIZE + 5];
    recv_buf = new char[BUF_SIZE + 5];
    buf = new char[BUF_SIZE + 5];
    temp_buf = new char[BUF_SIZE + 5];
    recv_buf_now = 0;
    connfd = socket(PF_INET, SOCK_STREAM, 0);
    epollfd = epoll_create(5);
    events = new epoll_event[2];
    pipe(pipefd);
}

Client::~Client()
{
    delete[] mes;
    delete[] recv_buf;
    delete[] buf;
    delete[] temp_buf;
    delete[] events;
    close(connfd);
    close(epollfd);
    close(pipefd[0]);
    close(pipefd[1]);
}

void Client::init()
{
    //将命令插入字典树
    for (int i = 0; i < order_num; ++i)
    {
        trie.insert(string(order[i]), i);
    }

    //连接服务器
    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);

    int res = connect(connfd, (struct sockaddr *)&server_address, sizeof(server_address));
    perror("connect");

    res = recv(connfd, recv_buf, BUF_SIZE, 0);
    int pos = read_1(recv_buf, 0, temp_buf);
    ret = atoi(temp_buf);
    return;
}

void Client::run()
{
    init();
    if (ret == 0)
    {
        cout << "连接服务器失败" << endl;
        return;
    }

    //将标准输入流设为非阻塞的
    int oldop = fcntl(STDIN_FILENO, F_GETFL);
    int newop = oldop | O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, newop);

    addfd(epollfd, STDIN_FILENO, EPOLLIN | EPOLLET);
    addfd(epollfd, pipefd[0]);

    //创建计时线程
    pthread_create(&pid, NULL, time_clock, NULL);

    while (client_live)
    {
        int num = epoll_wait(epollfd, events, 2, -1);
        for (int i = 0; i < num; ++i)
        {
            int fd = events[i].data.fd;
            //用户消息
            if (fd == STDIN_FILENO)
            {
                int len = read(fd, buf, BUF_SIZE - 100);
                if (len == BUF_SIZE - 100)
                {
                    //检测到命令过长，则提示错误并读空标准输入流
                    while (1)
                    {
                        len = read(fd, buf, BUF_SIZE - 100);
                        if (len < BUF_SIZE - 00)
                            break;
                    }
                    cout << info_to_user[0] << endl;
                    continue;
                }

                for (int i = 0; i < len; ++i)
                {
                    if (buf[i] == '\n')
                    {
                        recv_buf[recv_buf_now] = end_char;
                        recv_buf_now = 0;
                        parse_from_user();
                    }

                    else if (buf[i] == ' ')
                    {
                        recv_buf[recv_buf_now++] = split_char;
                    }

                    else
                    {
                        recv_buf[recv_buf_now++] = buf[i];
                    }
                }
            }

            //定时器到时，发送询问消息
            else
            {
                int len = read(fd, buf, BUF_SIZE - 100);
                if (!online)
                    continue;

                len = sprintf(mes, "%d%c", 0, split_char);
                len += sprintf(mes + len, "%d%c", user_id, end_char);
                send(connfd, mes, len, 0);

                bool flag = true;
                int now = 0;
                while (flag)
                {
                    int len = recv(connfd, buf, BUF_SIZE, 0);
                    for (int i = 0; i < len; ++i)
                    {
                        if (buf[i] == end_char)
                        {
                            recv_buf[now++] = end_char;
                            recv_buf[now] = '\0';
                            now = 0;
                            parse_from_ser();

                            if (ret == 1)
                            {
                                flag = false;
                                break;
                            }
                        }

                        else
                        {
                            recv_buf[now++] = buf[i];
                        }
                    }
                }
            }
        }
    }
}

void Client::parse_from_ser()
{
    int pos = read_1(recv_buf, 0, temp_buf);
    int mes_id = atoi(temp_buf);

    if (mes_id == 14)
    {
        ret = 1;
        return;
    }

    pos = read_1(recv_buf, pos, temp_buf);
    int info_id = atoi(temp_buf);

    switch (info_id)
    {
    case 10:
    {
        //好友请求
        pos = read_1(recv_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        cout << "用户" << userid << "想添加您为好友" << endl;
        break;
    }

    case 11:
    {
        //添加好友成功
        pos = read_1(recv_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        cout << "您已成功与用户" << userid << "成为好友" << endl;
        break;
    }

    case 12:
    {
        //好友的消息
        pos = read_1(recv_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(recv_buf, pos, temp_buf);
        cout << "好友" << userid << "对您说:" << temp_buf << endl;
        break;
    }

    case 17:
    {
        //添加好友失败
        pos = read_1(recv_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        cout << "用户" << userid << "拒绝了您的好友申请" << endl;
        break;
    }
    }

    ret = 0;
}

void Client::parse_from_user()
{
    int pos = read_1(recv_buf, 0, temp_buf);
    int order_id = trie.search(string(temp_buf));

    switch (order_id)
    {
    case -1:
    {
        //无法识别的命令
        cout << info_to_user[1] << endl;
        break;
    }

    case 0:
    {
        //用户无法直接使用bindwith
        cout << info_to_user[1] << endl;
        break;
    }

    case 1:
    {
        //退出
        client_live = 0;
        cout << "已退出" << endl;
        break;
    }

    case 2:
    {
        //注册
        int userid = user_register();
        setpwd(userid);
        if (ret == -1)
        {
            cout << "您可以使用默认的密码111111来登录" << endl;
        }

        else
        {
            cout << "您已成功注册并设置了密码，现在您可以使用新密码来登录了" << endl;
        }
        break;
    }

    case 3:
    {
        //设置密码
        //可能的返回值：7，9
        if (online)
        {
            //验证旧密码
            cout << "请输入旧密码" << endl;

            int len = read(STDIN_FILENO, recv_buf, BUF_SIZE);
            recv_buf[len - 1] = '\0';

            if (!check_pwd_legal(string(recv_buf)))
            {
                break;
            }

            len = sprintf(mes, "%d%c", 8, split_char);
            len += sprintf(mes + len, "%d%c", user_id, split_char);
            len += sprintf(mes + len, "%s%c", recv_buf, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            pos = read_1(recv_buf, 0, temp_buf);
            int info_id = atoi(temp_buf);
            if (info_id == 9)
            {
                cout << "密码错误" << endl;
            }

            if (info_id == 7)
            {
                //通过则重新设置密码
                setpwd(user_id);
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 4:
    {
        //设置昵称
        //可能的返回值：7
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            if (!check_name_legal(string(temp_buf)))
            {
                cout << "昵称格式不合法" << endl;
                break;
            }

            int len = sprintf(mes, "%d%c", 4, split_char);
            len += sprintf(mes + len, "%d%c", user_id, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            cout << "设置成功" << endl;
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 5:
    {
        //登录
        //可能的返回值：7，8，9
        pos = read_1(recv_buf, pos, temp_buf);
        int userid = check_userid_legal(string(temp_buf));
        if (userid == -1)
        {
            cout << "账号格式不合法" << endl;
            break;
        }

        pos = read_1(recv_buf, pos, temp_buf);
        if (!check_pwd_legal(temp_buf))
        {
            break;
        }

        if (online)
        {
            cout << "不能重复登陆" << endl;
            break;
        }

        int len = sprintf(mes, "%d%c", 5, split_char);
        len += sprintf(mes + len, "%d%c", userid, split_char);
        len += sprintf(mes + len, "%s%c", temp_buf, end_char);
        send(connfd, mes, len, 0);

        len = recv(connfd, recv_buf, BUF_SIZE, 0);
        pos = read_1(recv_buf, 0, temp_buf);
        int info_id = atoi(temp_buf);
        if (info_id == 8 || info_id == 9)
        {
            cout << info[info_id] << endl;
        }

        else
        {
            cout << "登录成功" << endl;
            user_id = userid;
            online = true;
        }
        break;
    }

    case 6:
    {
        //登出
        //可能的返回值：7
        if (online == true)
        {
            int len = sprintf(mes, "%d%c", 6, split_char);
            len += sprintf(mes + len, "%d%c", user_id, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            online = false;
            cout << "登出成功" << endl;
        }

        else
        {
            cout << info_to_user[2] << endl;
        }

        break;
    }

    case 7:
    {
        //向好友发送消息
        //可能的返回值：3，7，8
        if (online == true)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            int tarid = atoi(temp_buf);
            pos = read_1(recv_buf, pos, temp_buf);

            int len = sprintf(mes, "%d%c", 7, split_char);
            len += sprintf(mes + len, "%d%c", user_id, split_char);
            len += sprintf(mes + len, "%d%c", tarid, split_char);
            len += sprintf(mes + len, "%s%c", temp_buf, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            pos = read_1(recv_buf, 0, temp_buf);
            int info_id = atoi(temp_buf);

            if (info_id == 7)
            {
                cout << "发送成功" << endl;
            }

            else if (info_id == 3)
            {
                cout << "您还未添加对方为好友" << endl;
            }

            else
            {
                cout << "账号不存在" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 9:
    {
        //设置生日
        //可能的返回值：7
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            if (!check_date_legal(string(temp_buf)))
            {
                cout << "日期格式不合法" << endl;
            }

            else
            {
                int len = sprintf(mes, "%d%c", 9, split_char);
                len += sprintf(mes + len, "%d%c", user_id, split_char);
                len += sprintf(mes + len, "%s%c", temp_buf, end_char);
                send(connfd, mes, len, 0);

                len = recv(connfd, recv_buf, BUF_SIZE, 0);
                cout << "设置成功" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 10:
    {
        //设置个性签名
        //可能的返回值：7
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            if (!check_sig_legal(string(temp_buf)))
            {
                cout << "个性签名格式不合法" << endl;
            }

            else
            {
                int len = sprintf(mes, "%d%c", 10, split_char);
                len += sprintf(mes + len, "%d%c", user_id, split_char);
                len += sprintf(mes + len, "%s%c", temp_buf, end_char);
                send(connfd, mes, len, 0);

                len = recv(connfd, recv_buf, BUF_SIZE, 0);
                cout << "设置成功" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 11:
    {
        //设置学校
        //可能的返回值：7
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            if (!check_school_legal(string(temp_buf)))
            {
                cout << "学校格式不合法" << endl;
            }

            else
            {
                int len = sprintf(mes, "%d%c", 11, split_char);
                len += sprintf(mes + len, "%d%c", user_id, split_char);
                len += sprintf(mes + len, "%s%c", temp_buf, end_char);
                send(connfd, mes, len, 0);

                len = recv(connfd, recv_buf, BUF_SIZE, 0);
                cout << "设置成功" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 17:
    {
        //设置性别
        //可能的返回值：7
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            if (!check_gender_legal(string(temp_buf)))
            {
                cout << "性别格式不合法" << endl;
            }

            else
            {
                int len = sprintf(mes, "%d%c", 17, split_char);
                len += sprintf(mes + len, "%d%c", user_id, split_char);
                len += sprintf(mes + len, "%s%c", temp_buf, end_char);
                send(connfd, mes, len, 0);

                len = recv(connfd, recv_buf, BUF_SIZE, 0);
                cout << "设置成功" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 22:
    {
        //获取信息
        //可能的返回值：7，8
        if (online)
        {
            if (recv_buf[pos - 1] == end_char)
            {
                int len = sprintf(mes, "%d%c", order_id, split_char);
                len += sprintf(mes + len, "%d%c", user_id, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                pos = read_1(recv_buf, pos, temp_buf);
                int tarid = check_userid_legal(string(temp_buf));
                if (tarid == -1)
                {
                    cout << "账号格式不合法" << endl;
                    break;
                }

                else
                {
                    int len = sprintf(mes, "%d%c", order_id, split_char);
                    len += sprintf(mes + len, "%d%c", tarid, end_char);
                    send(connfd, mes, len, 0);
                }
            }

            int len = recv(connfd, recv_buf, BUF_SIZE, 0);
            pos = read_1(recv_buf, 0, temp_buf);
            int info_id = atoi(temp_buf);

            if (info_id == 8)
            {
                cout << "账号不存在" << endl;
            }

            else if (info_id == 3)
            {
                cout << "未设置该项信息" << endl;
            }

            else
            {
                if (order_id == 22)
                {
                    pos = read_1(recv_buf, pos, temp_buf);
                    int id = atoi(temp_buf);

                    if (id == 18)
                        cout << "在线" << endl;
                    else
                        cout << "离线" << endl;
                }

                else
                {
                    pos = read_1(recv_buf, pos, temp_buf);
                    cout << temp_buf << endl;
                }
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 18:
    case 19:
    {
        //18添加好友，19删除好友
        //可能的返回值：3,7，8，15
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            int tarid = check_userid_legal(string(temp_buf));
            if (tarid == -1)
            {
                cout << "账号格式不合法" << endl;
                break;
            }

            int len = sprintf(mes, "%d%c", order_id, split_char);
            len += sprintf(mes + len, "%d%c", user_id, split_char);
            len += sprintf(mes + len, "%d%c", tarid, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            pos = read_1(recv_buf, 0, temp_buf);
            int info_id = atoi(temp_buf);

            if (info_id == 8)
            {
                cout << "账号不存在" << endl;
            }

            else if (info_id == 15)
            {
                cout << "不能重复添加好友" << endl;
            }

            else if (info_id == 3)
            {
                cout << "您还未添加对方为好友" << endl;
            }

            else
            {
                if (order_id == 18)
                    cout << "好友请求已发送" << endl;
                else
                    cout << "删除成功" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }

    case 20:
    case 21:
    {
        //同意或拒绝好友申请
        //可能的返回值：7，8，16
        if (online)
        {
            pos = read_1(recv_buf, pos, temp_buf);
            int tarid = check_userid_legal(string(temp_buf));
            if (tarid == -1)
            {
                cout << "账号格式不合法" << endl;
                break;
            }

            int len = sprintf(mes, "%d%c", order_id, split_char);
            len += sprintf(mes + len, "%d%c", user_id, split_char);
            len += sprintf(mes + len, "%d%c", tarid, end_char);
            send(connfd, mes, len, 0);

            len = recv(connfd, recv_buf, BUF_SIZE, 0);
            pos = read_1(recv_buf, 0, temp_buf);
            int info_id = atoi(temp_buf);

            if (info_id == 8)
            {
                cout << "账号不存在" << endl;
            }

            else if (info_id == 16)
            {
                cout << "对方未申请" << endl;
            }

            else
            {
                if (order_id == 20)
                    cout << "您已添加了对方，可以开始聊天了" << endl;
                else
                    cout << "您拒绝了对方的好友申请" << endl;
            }
        }

        else
        {
            cout << info_to_user[2] << endl;
        }
        break;
    }
    }
    return;
}

int Client::user_register()
{
    int len = sprintf(mes, "%d%c", 2, end_char);
    send(connfd, mes, len, 0);
    len = recv(connfd, recv_buf, BUF_SIZE, 0);

    int pos = read_1(recv_buf, 0, temp_buf);
    pos = read_1(recv_buf, pos, temp_buf);

    int id = atoi(temp_buf);

    cout << "您的账号为: " << id << endl;

    return id;
}

bool Client::check_name_legal(string s)
{
    if (s.length() > 30)
        return false;

    for (auto c : s)
    {
        if (c < 0 || isdigit(c) || isalpha(c))
            continue;
        else
            return false;
    }

    return true;
}

bool Client::check_date_legal(string s)
{
    int now = 0;
    int len = s.length();
    int year = 0;
    int month = 0;
    int day = 0;
    //读年份
    while (isdigit(s[now]))
    {
        year = year * 10 + s[now] - '0';
        if (year > 2022)
            return false;
        ++now;
        if (now == len)
            return false;
    }
    if (year < 1800)
        return false;
    if (s[now] != '-')
        return false;

    ++now;
    if (now == len)
        return false;
    //读月份
    while (isdigit(s[now]))
    {
        month = month * 10 + s[now] - '0';
        if (month > 12)
            return false;
        ++now;
        if (now == len)
            return false;
    }
    if (month == 0)
        return false;
    if (s[now] != '-')
        return false;

    ++now;
    if (now == len)
        return false;
    //读天数
    while (isdigit(s[now]))
    {
        day = day * 10 + s[now] - '0';
        if (day > 31)
            return false;
        ++now;
        if (now == len)
            break;
    }
    if (day == 0)
        return false;
    if (now != len)
        return false;

    if (month == 4 || month == 6 || month == 9 || month == 11)
    {
        if (day > 30)
            return false;
    }

    if (month == 2 && year % 4 == 0)
    {
        if (day > 29)
            return false;
    }

    if (month == 2 && year % 4 != 0)
    {
        if (day > 28)
            return false;
    }

    return true;
}

bool Client::check_pwd_legal(string s)
{
    int len = s.length();
    if (len < 5)
        return false;

    if (len > 30)
        return false;

    for (auto c : s)
    {
        if (isdigit(c) || isalpha(c))
            continue;
        else
            return false;
    }

    return true;
}

bool Client::check_school_legal(string s)
{
    if (s.length() > 60)
        return false;

    for (auto c : s)
    {
        if (c < 0 || isalpha(c) || isblank(c))
            continue;
        else
            return false;
    }

    return true;
}

bool Client::check_sig_legal(string s)
{
    if (s.length() > 90)
        return false;

    for (auto c : s)
    {
        if (c < 0 || isalpha(c) || isblank(c) || ispunct(c))
            continue;
        else
            return false;
    }

    return true;
}

bool Client::check_gender_legal(string s)
{
    if (s == string("男") || s == string("女"))
        return true;
    else
        return false;
}

int Client::check_userid_legal(string s)
{
    int userid = 0;
    for (auto c : s)
    {
        if (!isdigit(c))
        {
            return -1;
        }
        userid = userid * 10 + c - '0';
        if (userid > 10000)
        {
            return -1;
        }
    }
    return userid;
}

void Client::setpwd(int userid)
{
    cout << "请输入您的密码,密码可由数字、小写字母或者大写字母构成,密码的长度应介于5位到30位之间" << endl;

    int len = read(STDIN_FILENO, buf, BUF_SIZE);
    buf[len - 1] = '\0';
    if (!check_pwd_legal(string(buf)))
    {
        cout << "密码格式不合法" << endl;
        ret = -1;
        return;
    }

    cout << "请确认密码:" << endl;
    len = read(STDIN_FILENO, temp_buf, BUF_SIZE);
    temp_buf[len - 1] = '\0';
    if (!check_pwd_legal(string(temp_buf)))
    {
        cout << "密码格式不合法" << endl;
        ret = -1;
        return;
    }

    if (strcmp(buf, temp_buf) != 0)
    {
        cout << "两次输入的密码不同" << endl;
        ret = -1;
        return;
    }

    len = sprintf(mes, "%d%c", 3, split_char);
    len += sprintf(mes + len, "%d%c", userid, split_char);
    len += sprintf(mes + len, "%s%c", buf, end_char);
    send(connfd, mes, len, 0);

    recv(connfd, recv_buf, BUF_SIZE, 0);
    cout << "密码设置成功" << endl;
    ret = 0;
    return;
}

#endif
