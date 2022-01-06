#ifndef __REQUEST_H
#define __REQUEST_H

#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "locker.h"
#include "user.h"
#include "tools.h"
#include "global.h"
using namespace std;

class Request
{
    const int BUF_SIZE = 1000;
    const char split_char = char(2);
    const char end_char = char(3);

public:
    Request(int c, char *s1);
    ~Request();

private:
    //连接套接字
    int connfd;
    //缓冲区
    char *buf;
    char *mes;
    char *temp_buf;

public:
    void process();

private:
    //用户关闭连接
    void user_close();
    //用户下线
    void user_quit(int connid);
    //句法解析
    void parse();
};

Request::Request(int connfd, char *s1) : connfd(connfd)
{
    buf = new char[BUF_SIZE + 5];
    mes = new char[BUF_SIZE + 5];
    temp_buf = new char[BUF_SIZE + 5];
    strcpy(buf, s1);
}

Request::~Request()
{
    delete[] buf;
    delete[] mes;
    delete[] temp_buf;
}

void Request::process()
{
    parse();
}

void Request::parse()
{
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
        while (users[userid].is_have_mes())
        {
            int len = sprintf(mes, "%d%c", 13, split_char);
            string s = users[userid].getmes();
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
        int userid = user_manage.user_register();
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
        user_manage.user_set_pwd(userid, temp_buf);

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
        user_manage.user_set_name(userid, temp_buf);

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
        if (user_manage.userid_check(userid))
        {
            pos = read_1(buf, pos, temp_buf);
            user_manage.user_get_pwd(userid, buf);
            if (strcmp(buf, temp_buf) == 0)
            {
                //可以登录则告知主线程
                int len = sprintf(mes, "%d%c", 5, split_char);
                len += sprintf(mes + len, "%d%c", userid, split_char);
                len += sprintf(mes + len, "%d%c", connfd, end_char);

                pipe_spin.lock();
                write(pipefd[1], mes, len);
                pipe_spin.unlock();
                break;
            }

            else
            {
                t = 9;
            }
        }

        else
        {
            t = 8;
        }

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
        pipe_spin.lock();
        write(pipefd[1], mes, len);
        pipe_spin.unlock();
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

        if (!user_manage.userid_check(tarid))
        {
            //用户不存在
            int len = sprintf(mes, "%d%c", 8, end_char);
            send(connfd, mes, len, 0);
            break;
        }

        if (!user_manage.user_check_fri(userid, tarid))
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
        users[tarid].sendmes(mes);

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
        user_manage.user_get_pwd(userid, buf);
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
        user_manage.user_set_birth(userid, temp_buf);

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
        user_manage.user_set_sig(userid, temp_buf);

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
        user_manage.user_set_school(userid, temp_buf);

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

        int res = user_manage.user_get_name(userid, temp_buf);
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

        int res = user_manage.user_get_sig(userid, temp_buf);
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

        int res = user_manage.user_get_school(userid, temp_buf);
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

        int res = user_manage.user_get_birth(userid, temp_buf);
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

        int res = user_manage.user_get_gender(userid, temp_buf);
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
        user_manage.user_set_gender(userid, temp_buf);

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

        if (user_manage.userid_check(tarid))
        {
            if (user_manage.user_check_fri(userid, tarid))
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
                users[tarid].sendmes(mes);
                users[tarid].add_fri_request(userid);

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

        if (user_manage.userid_check(tarid))
        {
            if (!user_manage.user_check_fri(userid, tarid))
            {
                //还未添加对方为好友
                int len = sprintf(mes, "%d%c", 3, end_char);
                send(connfd, mes, len, 0);
            }

            else
            {
                //删除好友关系
                user_manage.user_del_fri(userid, tarid);
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

        if (user_manage.userid_check(tarid))
        {
            if (users[userid].check_fri_request(tarid))
            {
                //成功添加好友
                users[userid].erase_fri_request(tarid);
                user_manage.user_add_fri(userid, tarid);
                user_manage.user_add_fri(tarid, userid);

                //告知对方
                int len = sprintf(mes, "%d%c", 11, split_char);
                len += sprintf(mes + len, "%d%c", userid, end_char);
                users[tarid].sendmes(mes);

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

        if (user_manage.userid_check(tarid))
        {
            if (users[userid].check_fri_request(tarid))
            {
                //删除好友申请
                users[userid].erase_fri_request(tarid);

                //告知对方
                int len = sprintf(mes, "%d%c", 17, split_char);
                len += sprintf(mes + len, "%d%c", userid, end_char);
                users[tarid].sendmes(mes);

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

        if (user_manage.userid_check(tarid))
        {
            int len = sprintf(mes, "%d%c", 7, split_char);
            if (users[tarid].is_online())
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

    return;
}

#endif