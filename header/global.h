#ifndef __GLOBAL_H
#define __GLOBAL_H

#include <unordered_map>
#include "user.h"
#include "locker.h"

int pipefd[2];
spinlock pipe_spin;

const int MAX_USER_NUM = 10000;
User users[MAX_USER_NUM];
unordered_map<int, int> connfd_to_userid;

User_manage user_manage;

#endif