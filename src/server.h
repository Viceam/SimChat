#ifndef SERVER_H
#define SERVER_H

#include "global.h"

class Server
{
private:
    int server_port;
    int server_sockfd;
    string server_ip;
    static vector<int> sock_arr;
    static unordered_map<string, int> name_sock_map;  // 名字和套接字描述符
    //static unordered_map<string, string> from_to_map; // 记录用户xx要向用户yy发送信息
    static pthread_mutex_t name_sock_mutx;            // 互斥锁，锁住需要修改name_sock_map的临界区
    static unordered_map<int, set<int>> group_map;    // 记录群号和套接字描述符集合
    static pthread_mutex_t group_mutx;                // 互斥锁，锁住需要修改group_map的临界区
    //static pthread_mutex_t from_mutex;                // 互斥锁，锁住修改from_to_map的临界区

public:
    Server(int port, string ip);
    ~Server();
    void run();
    static void RecvMsg(int conn);
    static void handleReq(int conn, const string &msg, tuple<bool, string, string, int, int> &info);
    //static void setnonblocking(int conn); // 将套接字设为非阻塞
};

#endif
