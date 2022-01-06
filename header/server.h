#ifndef __SERVER_H
#define __SERVER_H

#include <mysql/mysql.h>
#include "threadpool.h"
#include "request.h"
#include "user.h"
#include "locker.h"
#include "tools.h"
#include "global.h"
using namespace std;

class Server
{
    const int BUF_SIZE = 1000;
    const int MAX_EVENTS_NUM = 15;
    const int MAX_CONN_NUM = 3;
    const char split_char = char(2);
    const char end_char = char(3);

public:
    Server();
    ~Server();

private:
    //服务器运行开关
    bool server_live;
    //epoll文件描述符
    int epollfd;
    //epoll事件数组
    epoll_event *events;
    //监听socket
    int listenfd;
    //连接数量
    int conn_num;
    //ip地址
    const char *ip;
    //端口号
    int port;
    //发送的消息
    char *mes;
    //句法解析过程的临时缓存
    char *buf;
    char *temp_buf;

    //接受线程消息的缓存
    int pipe_buf_now;
    char *pipe_buf;

    //mysql连接
    MYSQL *connection;

    //线程池
    Threadpool<Request> threadpool;

public:
    void run();

private:
    //服务器初始化
    void init();
    //在epoll上注册事件
    void addfd(int epollfd, int fd, uint32_t ets = EPOLLIN);
    //在epoll上删除事件
    void delfd(int epollfd, int fd);
    //解析线程消息
    void parse_thread();
    //mysql启动
    void my_mysql_init();
};

Server::Server()
{
    server_live = true;
    ip = "127.0.0.1";
    port = 5005;
    conn_num = 0;
    epollfd = epoll_create(5);
    events = new epoll_event[MAX_EVENTS_NUM * 2 + 5];
    mes = new char[BUF_SIZE + 5];
    buf = new char[BUF_SIZE + 5];
    pipe_buf = new char[BUF_SIZE + 5];
    temp_buf = new char[BUF_SIZE + 5];
    pipe_buf_now = 0;
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    pipe(pipefd);
}

Server::~Server()
{
    close(epollfd);
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    if (connection != NULL)
    {
        mysql_close(connection);
    }
    delete[] events;
    delete[] mes;
    delete[] buf;
    delete[] pipe_buf;
    delete[] temp_buf;
}

void Server::delfd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
}

void Server::addfd(int epollfd, int fd, uint32_t ets)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ets;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void Server::my_mysql_init()
{
    //mysql初始化
    connection = mysql_init(NULL);
    if (connection != NULL)
    {
        perror("mysql_init");
    }
    else
    {
        cout << "mysql初始化失败" << endl;
        exit(0);
    }

    //mysql连接
    const char *host = "localhost";
    const char *user = "root";
    const char *pwd = "112188";
    const char *db_name = "db";

    connection = mysql_real_connect(connection, host, user, pwd, db_name, 0, NULL, 0);
    if (connection != NULL)
    {
        perror("mysql_connect");
    }
    else
    {
        cout << "mysql连接失败" << endl;
        exit(0);
    }

    user_manage.set_connection(connection);
}

void Server::init()
{
    //初始化用户类数组
    for (int i = 0; i < MAX_USER_NUM; ++i)
    {
        users[i].init(i);
    }

    //监听tcp端口
    sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(ip);

    int res = bind(listenfd, (sockaddr *)&server_address, sizeof(server_address));
    res = listen(listenfd, 5);
    perror("listen_1");

    addfd(epollfd, listenfd);
    addfd(epollfd, pipefd[0]);

    //初始化数据库管理系统
    my_mysql_init();

    perror("init");
}

void Server::run()
{
    init();
    // cout << "odsnfaosef\n";

    while (server_live)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENTS_NUM, -1);
        for (int i = 0; i < num; ++i)
        {
            int fd = events[i].data.fd;

            if (fd == listenfd)
            {
                //新的连接到来
                sockaddr_in cli_addr;
                socklen_t cli_addr_len = sizeof(cli_addr);
                int connfd = accept(listenfd, (sockaddr *)&cli_addr, &cli_addr_len);

                if (conn_num >= MAX_CONN_NUM)
                {
                    //如果连接数过多则发送0号消息告诉客户端服务器忙
                    int t = 0;
                    int len = sprintf(mes, "%d%c", t, end_char);
                    send(connfd, mes, len, 0);
                    close(connfd);
                }

                else
                {
                    //发送1号消息告诉客户端可以进行下一步
                    int t = 1;
                    int len = sprintf(mes, "%d%c", t, split_char);

                    conn_num++;
                    cout << "一名用户已连接，现在有" << conn_num << "名用户" << endl;

                    // cout << mes << '\n';
                    send(connfd, mes, len, 0);
                    addfd(epollfd, connfd);
                }
            }

            else if (fd == pipefd[0])
            {
                //来自线程的消息
                int len = read(pipefd[0], buf, BUF_SIZE);
                for (int i = 0; i < len; ++i)
                {
                    if (buf[i] == end_char)
                    {
                        pipe_buf[pipe_buf_now] = end_char;
                        pipe_buf_now = 0;
                        parse_thread();
                    }

                    else
                    {
                        pipe_buf[pipe_buf_now++] = buf[i];
                    }
                }
            }

            else
            {
                int len = recv(fd, buf, BUF_SIZE, 0);
                if (len <= 0)
                {
                    conn_num--;
                    delfd(epollfd, fd);
                    close(fd);
                    if (connfd_to_userid.count(fd))
                    {
                        users[connfd_to_userid[fd]].logout();
                        connfd_to_userid.erase(fd);
                    }
                    cout << "一名用户已退出，现有" << conn_num << "名用户" << endl;
                }

                else
                {
                    buf[len] = '\0';
                    Request *request = new Request(fd, buf);
                    threadpool.addwork(request);
                }
            }
        }
    }
}

void Server::parse_thread()
{
    int pos = read_1(pipe_buf, 0, temp_buf);
    int id = atoi(temp_buf);

    switch (id)
    {
    case 0:
    {
        break;
    }

    case 1:
    {
        break;
    }

    case 5:
    {
        //用户登录
        pos = read_1(pipe_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(pipe_buf, pos, temp_buf);
        int connfd = atoi(temp_buf);

        connfd_to_userid[connfd] = userid;
        users[userid].setconnfd(connfd);
        users[userid].login();

        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
        break;
    }

    case 6:
    {
        //用户登出
        pos = read_1(pipe_buf, pos, temp_buf);
        int userid = atoi(temp_buf);
        pos = read_1(pipe_buf, pos, temp_buf);
        int connfd = atoi(temp_buf);

        users[userid].logout();
        connfd_to_userid.erase(connfd);
        
        int len = sprintf(mes, "%d%c", 7, end_char);
        send(connfd, mes, len, 0);
    }
    }
}

#endif