#ifndef __DATA_H
#define __DATA_H

const int order_num = 23;
const int info_num = 11;
const int info_to_user_num = 5;

const char *info[100] = {
    "服务器忙",                       //0
    "主动端口连接成功，并接受连接号", //无效
    "空值",                           //2
    "还未添加对方为好友",             //3
    "服务器已断开连接",               //4
    "未知的错误",                     //5
    "注册成功，并接受账号",           //6
    "操作成功",                       //7
    "账户不存在",                     //8
    "密码错误",                       //9
    "好友申请",                       //10
    "添加好友成功",                   //11
    "好友消息",                       //12
    "后续还有消息",                   //13
    "后续无消息",                     //14
    "不能重复添加好友",               //15
    "对方未申请",                     //16
    "对方拒绝了你的好友申请",         //17
    "在线",                           //18
    "离线",                           //19
};

const char *order[100] = {
    "query",    //0
    "quit",     //1
    "register", //2
    "setpwd",   //3
    "setname",  //4
    "login",    //5
    "logout",   //6
    "talkto",   //7
    "checkpwd", //8
    "setbirth", //9
    "setsig",   //10
    "setsch",   //11
    "getname",  //12
    "getsig",   //13
    "getsch",   //14
    "getbirth", //15
    "getgen",   //16
    "setgen",   //17
    "addfri",   //18
    "delete",   //19
    "agree",    //20
    "refuse",   //21
    "isonline", //22
};

const char *info_to_user[100] = {
    "命令过长",       //0
    "无法识别的命令", //1
    "请先登录",       //2
};

#endif