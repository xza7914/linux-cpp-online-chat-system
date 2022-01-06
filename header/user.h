#ifndef __USER_H
#define __USER_H

#include <string>
#include <cstring>
#include <mysql/mysql.h>
#include <list>
#include <unordered_set>
#include <unistd.h>
#include <fcntl.h>
#include "locker.h"
using namespace std;

class User
{
    const int BUF_SIZE = 1000;
    const char split_char = 2;
    const char end_char = 3;

public:
    User();
    ~User();

private:
    //连接号
    int connfd;
    //是否在线
    bool online;
    //消息链表
    list<string> mes_list;
    //好友申请集合
    unordered_set<int> fri_request;
    //互斥锁
    locker lock;
    //用户账号
    int userid;
    //用户消息文件名
    char *file;

public:
    //初始化
    void init(int userid);
    //获取连接号
    int getconnfd();
    //设置连接号
    void setconnfd(int connfd);
    //登录
    void login();
    //登出
    void logout();
    //发送消息
    void sendmes(const char *mes);
    //添加好友请求
    void add_fri_request(int id);
    //检查好友请求
    bool check_fri_request(int id);
    //移除好友请求
    void erase_fri_request(int id);
    //查询是否在线
    bool is_online();
    //获取消息
    string getmes();
    //查询队列中是否还有消息
    bool is_have_mes();
};

User::User()
{
    file = new char[50];
}

User::~User()
{
    delete[] file;
}

void User::init(int userid)
{
    lock.lock();
    this->userid = userid;
    sprintf(file, "../mesfile/mf%d.txt", userid);
    lock.unlock();
}

int User::getconnfd()
{
    int res;
    lock.lock();
    res = connfd;
    lock.unlock();
    return connfd;
}

void User::setconnfd(int connfd)
{
    lock.lock();
    this->connfd = connfd;
    lock.unlock();
}

void User::login()
{
    lock.lock();
    online = true;

    int fd = open(file, O_RDONLY);
    if (fd != -1)
    {
        //将消息文件中的消息提取到消息队列中
        char buf1[BUF_SIZE];
        char buf2[BUF_SIZE];
        int buf2_now = 0;
        while (1)
        {
            int len = read(fd, buf1, BUF_SIZE);

            for (int i = 0; i < len; ++i)
            {
                if (buf1[i] == end_char)
                {
                    buf2[buf2_now++] = end_char;
                    buf2[buf2_now] = '\0';
                    buf2_now = 0;
                    mes_list.push_back(string(buf2));
                }

                else
                {
                    buf2[buf2_now++] = buf1[i];
                }
            }

            if (len < BUF_SIZE)
                break;
        }
        close(fd);
    }
    remove(file);
    lock.unlock();
}

void User::logout()
{
    lock.lock();
    online = false;

    if (mes_list.empty())
    {
        //消息队列为空则直接返回
        lock.unlock();
        return;
    }

    //将未来得及接受的消息存储在消息文件中
    int fd = open(file, O_WRONLY | O_APPEND | O_CREAT);
    while (mes_list.size())
    {
        string s = mes_list.front();
        mes_list.pop_front();
        write(fd, s.c_str(), s.length());
    }
    close(fd);
    lock.unlock();
}

void User::sendmes(const char *mes)
{
    lock.lock();
    if (online)
    {
        //此时用户在线
        mes_list.push_back(string(mes));
    }

    else
    {
        //此时用户不在线
        int fd = open(file, O_WRONLY | O_APPEND | O_CREAT);
        write(fd, mes, strlen(mes));
        close(fd);
    }
    lock.unlock();
}

void User::add_fri_request(int id)
{
    lock.lock();
    fri_request.insert(id);
    lock.unlock();
}

bool User::check_fri_request(int id)
{
    bool res = false;
    lock.lock();
    if (fri_request.count(id))
        res = true;
    lock.unlock();
    return res;
}

void User::erase_fri_request(int id)
{
    lock.lock();
    fri_request.erase(id);
    lock.unlock();
}

bool User::is_online()
{
    bool res = false;
    lock.lock();
    res = online;
    lock.unlock();
    return res;
}

string User::getmes()
{
    string s;
    lock.lock();
    s = mes_list.front();
    mes_list.pop_front();
    lock.unlock();
    return s;
}

bool User::is_have_mes()
{
    bool res = false;
    lock.lock();
    if (mes_list.size())
        res = true;
    lock.unlock();
    return res;
}

class User_manage
{
    const int MAX_USERID = 10000;
    const int BUF_SIZE = 1000;

public:
    User_manage();
    ~User_manage();

private:
    //访问user_manage函数的锁
    locker user_manage_lock;
    //访问mysql的锁
    locker mysql_lock;
    //mysql结果
    MYSQL_RES *result;
    //结果行
    MYSQL_ROW row;
    //mysql连接
    MYSQL *connection;
    //缓冲区
    char *buf;

public:
    //用户注册
    int user_register();
    //检查用户是否存在
    bool userid_check(int userid);
    //添加用户
    void user_add(int userid);
    //设置mysql连接
    void set_connection(MYSQL *c);

    //获取密码
    int user_get_pwd(int userid, char *pwd);
    //设置密码
    int user_set_pwd(int userid, char *pwd);
    //获取昵称
    int user_get_name(int userid, char *name);
    //设置昵称
    int user_set_name(int userid, char *name);
    //获取个性签名
    int user_get_sig(int userid, char *sig);
    //设置个性签名
    int user_set_sig(int userid, char *sig);
    //获取学校
    int user_get_school(int userid, char *school);
    //设置学校
    int user_set_school(int userid, char *school);
    //获取生日
    int user_get_birth(int userid, char *birth);
    //设置生日
    int user_set_birth(int userid, char *birth);
    //获取性别
    int user_get_gender(int userid, char *gender);
    //设置性别
    int user_set_gender(int userid, char *gender);
    //增加好友关系
    void user_add_fri(int id1, int id2);
    //删除好友关系
    void user_del_fri(int id1, int id2);
    //检测好友关系
    bool user_check_fri(int id1, int id2);
};

User_manage::User_manage()
{
    buf = new char[BUF_SIZE];
}

User_manage::~User_manage()
{
    delete[] buf;
}

void User_manage::set_connection(MYSQL *c)
{
    connection = c;
}

bool User_manage::userid_check(int userid)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT id FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    bool res;
    if (row == NULL)
        res = false;
    else
        res = true;
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

void User_manage::user_add(int userid)
{
    mysql_lock.lock();
    sprintf(buf, "INSERT INTO users (id, pwd) VALUES (%d,'111111');", userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
}

int User_manage::user_register()
{
    int res;
    user_manage_lock.lock();
    srand((unsigned)time(NULL));
    while (1)
    {
        int id = rand() % 10000;
        if (!userid_check(id))
        {
            res = id;
            user_add(id);
            break;
        }
    }
    user_manage_lock.unlock();
    return res;
}

int User_manage::user_get_pwd(int userid, char *pwd)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT pwd FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
    {
        res = 1;
    }

    else
    {
        strcpy(pwd, row[0]);
        res = 0;
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_pwd(int userid, char *pwd)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET pwd='%s' WHERE id=%d;", pwd, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

int User_manage::user_get_name(int userid, char *name)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT name FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
        res = 1;

    else
    {
        if (row[0] == NULL)
            res = 2;

        else
        {
            strcpy(name, row[0]);
            res = 0;
        }
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_name(int userid, char *name)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET name='%s' WHERE id=%d;", name, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

int User_manage::user_get_sig(int userid, char *sig)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT sig FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
        res = 1;

    else
    {
        if (row[0] == NULL)
            res = 2;

        else
        {
            strcpy(sig, row[0]);
            res = 0;
        }
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_sig(int userid, char *sig)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET sig='%s' WHERE id=%d;", sig, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

int User_manage::user_get_school(int userid, char *school)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT school FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
        res = 1;

    else
    {
        if (row[0] == NULL)
            res = 2;

        else
        {
            strcpy(school, row[0]);
            res = 0;
        }
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_school(int userid, char *school)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET school='%s' WHERE id=%d;", school, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

int User_manage::user_get_birth(int userid, char *birth)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT birthday FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
        res = 1;

    else
    {
        if (row[0] == NULL)
            res = 2;

        else
        {
            strcpy(birth, row[0]);
            res = 0;
        }
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_birth(int userid, char *birth)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET birthday='%s' WHERE id=%d;", birth, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

int User_manage::user_get_gender(int userid, char *gender)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT gender FROM users WHERE id=%d;", userid);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    int res;
    if (row == NULL)
        res = 1;

    else
    {
        if (row[0] == NULL)
            res = 2;

        else
        {
            strcpy(gender, row[0]);
            res = 0;
        }
    }
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

int User_manage::user_set_gender(int userid, char *gender)
{
    mysql_lock.lock();
    sprintf(buf, "UPDATE users SET gender='%s' WHERE id=%d;", gender, userid);
    mysql_query(connection, buf);
    mysql_lock.unlock();
    return 0;
}

void User_manage::user_add_fri(int id1, int id2)
{
    mysql_lock.lock();
    sprintf(buf, "INSERT INTO fris (userid1,userid2)VALUES(%d,%d);", id1, id2);
    mysql_query(connection, buf);
    mysql_lock.unlock();
}

void User_manage::user_del_fri(int id1, int id2)
{
    mysql_lock.lock();
    sprintf(buf, "DELETE FROM fris WHERE userid1=%d AND userid2=%d OR userid1=%d AND userid2=%d;", id1, id2, id2, id1);
    mysql_query(connection, buf);
    mysql_lock.unlock();
}

bool User_manage::user_check_fri(int id1, int id2)
{
    mysql_lock.lock();
    sprintf(buf, "SELECT id FROM fris WHERE userid1=%d AND userid2=%d;", id1, id2);
    mysql_query(connection, buf);
    result = mysql_use_result(connection);
    row = mysql_fetch_row(result);
    bool res;
    if (row == NULL)
        res = false;
    else
        res = true;
    mysql_free_result(result);
    mysql_lock.unlock();
    return res;
}

#endif