#include "server.h"

vector<int> Server::sock_arr(1100, 0);
unordered_map<string, int> Server::name_sock_map; // 名字和套接字描述符
pthread_mutex_t Server::name_sock_mutx;           // 互斥锁，锁住需要修改name_sock_map的临界区
unordered_map<int, set<int>> Server::group_map;   // 记录群号和套接字描述符集合
pthread_mutex_t Server::group_mutx;               // 互斥锁，锁住需要修改group_map的临界区


// 构造函数
Server::Server(int port, string ip) : server_port(port), server_ip(ip)
{
    pthread_mutex_init(&name_sock_mutx, NULL);
}

// 析构函数
Server::~Server()
{
    for (int i = 0; i < sock_arr.size(); ++i)
    {
        if (sock_arr[i])
            close(i);
    }
    close(server_sockfd);
}

// 服务器开始服务
void Server::run()
{
    // 定义sockfd
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // 定义sockaddr_in
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;                           // TCP/IP协议族
    server_sockaddr.sin_port = htons(server_port);                  // server_port;//端口号
    server_sockaddr.sin_addr.s_addr = inet_addr(server_ip.c_str()); // ip地址，127.0.0.1是环回地址，相当于本机ip

    // bind，成功返回0，出错返回-1
    if (bind(server_sockfd, (struct sockaddr *)&server_sockaddr, sizeof(server_sockaddr)) == -1)
    {
        perror("bind"); // 输出错误原因
        exit(1);        // 结束程序
    }

    // listen，成功返回0，出错返回-1
    if (listen(server_sockfd, 20) == -1)
    {
        perror("listen"); // 输出错误原因
        exit(1);          // 结束程序
    }

    // 客户端套接字
    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);

    // 不断取出新连接并创建子线程为其服务
    while (1)
    {
        int conn = accept(server_sockfd, (struct sockaddr *)&client_addr, &length);
        if (conn < 0)
        {
            perror("connect"); // 输出错误原因
            exit(1);           // 结束程序
        }
        sock_arr[conn] = 1;
        cout << "文件描述符为" << conn << "的客户端成功连接\n";
        // 创建线程
        thread t(Server::RecvMsg, conn);
        t.detach(); // 置为分离状态，不能用join，join会导致主线程阻塞
    }
}

void Server::RecvMsg(int conn)
{
    tuple<bool, string, string, int, int> info;

    get<0>(info) = false;
    get<3>(info) = -1;

    // 接收缓冲区
    char buffer[1000];
    // 不断接收数据
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(conn, buffer, sizeof(buffer), 0);
        // 客户端发送exit或者异常结束时，退出
        if (strcmp(buffer, "content:exit") == 0 || strcmp(buffer, "groupms:exit") == 0 || len <= 0)
        {
            close(conn);
            sock_arr[conn] = 0;
            break;
        }
        cout << "收到套接字描述符为" << conn << "发来的信息：" << buffer << endl;

        string rec(buffer);
        handleReq(conn, rec, info);
    }
}

void Server::handleReq(int conn, const string &str, tuple<bool, string, string, int, int> &info)
{
    char buffer[1000];
    string name, pass;

    bool if_login = get<0>(info);      // 记录当前服务对象是否成功登录
    string login_name = get<1>(info);  // 记录当前服务对象的名字
    string target_name = get<2>(info); // 记录目标对象的名字
    int target_conn = get<3>(info);    // 目标对象的套接字描述符
    int group_num = get<4>(info);      // 记录所处群号

    // 连接MYSQL数据库
    MYSQL *con = mysql_init(NULL);
    if (mysql_real_connect(con, "127.0.0.1", "root", NULL, "ChatProject", 0, NULL, CLIENT_MULTI_STATEMENTS))
        cout << "连接mysql成功\n";
    else
    {
        cout << "连接mysql失败\n";
    }

    // 连接redis数据库
    redisContext *redis_target = redisConnect("127.0.0.1", 6379);
    if (redis_target->err)
    {
        redisFree(redis_target);
        cout << "连接redis失败" << endl;
    }
    else
    {
        cout << "连接redis成功";
    }

    // 先接收cookie检查redis是否保存该用户的登录状态
    if (str.find("cookie:") != str.npos)
    {
        string cookie = str.substr(7);
        // 查询该cookie是否存在：hget cookie name
        string redis_str = "hget " + cookie + " name";
        redisReply *r = (redisReply *)redisCommand(redis_target, redis_str.c_str());
        string send_res;
        // 存在
        if (r->str)
        {
            cout << "查询redis结果: " << r->str << endl;
            send_res = r->str;
            name_sock_map[send_res] = conn;
            login_name = send_res;
        }
        // 不存在
        else
            send_res = "NULL";
        send(conn, send_res.c_str(), send_res.length() + 1, 0);
    }
    // 注册
    else if (str.find("name:") != str.npos)
    {
        int p1 = str.find("name:"), p2 = str.find("pass:");
        name = str.substr(p1 + 5, p2 - 5);
        pass = str.substr(p2 + 5, str.length() - p2 - 4);
        string search = "SELECT * FROM USER WHERE NAME=\"";
        search += name;
        search += "\";";
        cout << "sql语句:" << search << endl;
        auto search_res = mysql_query(con, search.c_str());
        auto result = mysql_store_result(con);
        int col = mysql_num_fields(result); // 获取列数
        int row = mysql_num_rows(result);   // 获取行数

        if (search_res == 0 && row != 0)
        {
            char buf[100] = "wrong";
            cout << "查询到用户名已存在\n";
            send(conn, buf, strlen(buf), 0);
        }
        else
        {
            search = "INSERT INTO USER VALUES (\"";
            search += name;
            search += "\",\"";
            search += pass;
            search += "\");";
            cout << "sql语句:" << search << endl
                 << endl;
            mysql_query(con, search.c_str());
            char buf[100] = "ok";
            send(conn, buf, strlen(buf), 0);
        }
    }
    // 登录
    else if (str.find("login") != str.npos)
    {
        int p1 = str.find("login"), p2 = str.find("pass:");
        name = str.substr(p1 + 5, p2 - 5);
        pass = str.substr(p2 + 5, str.length() - p2 - 4);
        string search = "SELECT * FROM USER WHERE NAME=\"";
        search += name;
        search += "\";";
        cout << "sql语句:" << search << endl;
        auto search_res = mysql_query(con, search.c_str());
        auto result = mysql_store_result(con);
        int col = mysql_num_fields(result); // 获取列数
        int row = mysql_num_rows(result);   // 获取行数
        // 查询到用户名
        if (search_res == 0 && row != 0)
        {
            cout << "查询成功\n";
            auto info = mysql_fetch_row(result); // 获取一行的信息
            cout << "查询到用户名:" << info[0] << " 密码:" << info[1] << endl;
            // 密码正确
            if (info[1] == pass)
            {
                cout << "登录密码正确\n\n";
                string str1 = "ok";
                if_login = true;
                login_name = name; // 记录下当前登录的用户名

                pthread_mutex_lock(&name_sock_mutx);   // 上锁
                name_sock_map[login_name] = conn;      // 记录下名字和文件描述符的对应关系
                pthread_mutex_unlock(&name_sock_mutx); // 解锁

                // 随机生成sessionid并发送到客户端
                srand(time(NULL)); // 初始化随机数种子
                for (int i = 0; i < 10; i++)
                {
                    int type = rand() % 3; // type为0代表数字，为1代表小写字母，为2代表大写字母
                    if (type == 0)
                        str1 += '0' + rand() % 9;
                    else if (type == 1)
                        str1 += 'a' + rand() % 26;
                    else if (type == 2)
                        str1 += 'A' + rand() % 26;
                }
                // 将sessionid存入redis
                string redis_str = "hset " + str1.substr(2) + " name " + login_name;
                redisReply *r = (redisReply *)redisCommand(redis_target, redis_str.c_str());
                // 设置生存时间,默认300秒
                redis_str = "expire " + str1.substr(2) + " 300";
                r = (redisReply *)redisCommand(redis_target, redis_str.c_str());
                cout << "随机生成的sessionid为: " << str1.substr(2) << endl;

                send(conn, str1.c_str(), str1.length() + 1, 0);
            }
            // 密码错误
            else
            {
                cout << "登录密码错误\n\n";
                char str1[100] = "wrong";
                send(conn, str1, strlen(str1), 0);
            }
        }
        // 没找到用户名
        else
        {
            cout << "查询失败\n\n";
            char str1[100] = "wrong";
            send(conn, str1, strlen(str1), 0);
        }
    }
    // 设定目标的文件描述符
    else if (str.find("target:") != str.npos)
    {
        int pos1 = str.find("from");
        string target = str.substr(7, pos1 - 7), from = str.substr(pos1 + 5);
        target_name = target;
        // 找不到这个目标
        if (name_sock_map.find(target) == name_sock_map.end())
            cout << "源用户为" << login_name << ",目标用户" << target_name << "仍未登录，无法发起私聊\n";
        // 找到了目标
        else
        {
            cout << "源用户" << login_name << "向目标用户" << target_name << "发起的私聊即将建立";
            cout << ",目标用户的套接字描述符为" << name_sock_map[target] << endl;
            target_conn = name_sock_map[target];
        }
    }
    // 消息转发
    else if (str.find("content:") != str.npos)
    {
        if (target_conn == -1)
        {
            cout << "找不到目标用户" << target_name << "的套接字，将尝试重新寻找目标用户的套接字\n";
            if (name_sock_map.find(target_name) != name_sock_map.end())
            {
                target_conn = name_sock_map[target_name];
                cout << "重新查找目标用户套接字成功\n";
            }
            else
            {
                cout << "查找仍然失败，转发失败！\n";
            }
        }
        string recv_str(str);
        string send_str = recv_str.substr(8);
        cout << "用户" << login_name << "向" << target_name << "发送:" << send_str << endl;
        send_str = "[" + login_name + "]:" + send_str + "\n";
        send(target_conn, send_str.c_str(), send_str.length(), 0);
    }
    // 入群
    else if (str.find("group:") != str.npos)
    {
        group_num = stoi(str.substr(6));
        cout << "用户" << login_name << "绑定群聊号为：" << group_num << endl;
        pthread_mutex_lock(&group_mutx); // 上锁
        group_map[group_num].insert(conn);
        pthread_mutex_unlock(&group_mutx); // 解锁

        string msg = "[系统]用户" + login_name + "加入了群聊\n";
        for (int usr : group_map[group_num])
        {
            if (usr != conn)
                send(usr, msg.c_str(), msg.length(), 0);
        }
    }
    // 转发群消息
    else if (str.find("groupms:") != str.npos)
    {
        string text = "[" + login_name + "]:" + str.substr(8) + "\n";
        cout << "群聊信息：" << text << endl;
        for (int usr : group_map[group_num])
        {
            if (usr != conn)
                send(usr, text.c_str(), text.length(), 0);
        }
    }

    // 更新
    get<0>(info) = if_login;    // 记录当前服务对象是否成功登录
    get<1>(info) = login_name;  // 记录当前服务对象的名字
    get<2>(info) = target_name; // 记录目标对象的名字
    get<3>(info) = target_conn; // 目标对象的套接字描述符
    get<4>(info) = group_num;   // 记录所处群号
}
