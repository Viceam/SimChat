#include "client.h"

Client::Client(int port, string ip) : server_port(port), server_ip(ip) {}
Client::~Client()
{
    close(sock);
}

void Client::run()
{
    sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    servaddr.sin_addr.s_addr = inet_addr(server_ip.c_str());

    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    cout << "连接服务器成功\n";

    handleClient(sock);
}

void Client::SendMsg(int conn)
{
    string msg;
    while (1)
    {
        cin >> msg;
        if (conn > 0)
            msg = "content:" + msg;
        else
            msg = "groupms:" + msg;
        int ret = send(abs(conn), msg.c_str(), msg.length(), 0);

        if (msg.substr(8) == "exit" || ret <= 0)
            break;
    }
}

void Client::RecvMsg(int conn)
{
    char buffer[1000];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(conn, buffer, sizeof(buffer), 0);
        if (len <= 0)
            break;
        cout << buffer;
    }
}

void Client::show_menu()
{
    cout << " ------------------\n";
    cout << "|                  |\n";
    cout << "|   请选择功能:    |\n";
    cout << "|    0:退出        |\n";
    cout << "|    1:登录        |\n";
    cout << "|    2:注册        |\n";
    cout << "|                  |\n";
    cout << " ------------------ \n\n";
}

void Client::handleClient(int conn)
{
    string choice;
    string name;
    string password, confirm_password;

    bool login = false;
    string login_name;

    ifstream f("cookie.txt");
    string cookie_str;
    if (f.good())
    {
        f >> cookie_str;
        f.close();
        cookie_str = "cookie:" + cookie_str;
        send(sock, cookie_str.c_str(), cookie_str.length(), 0);

        char cookie_ans[100];
        memset(cookie_ans, 0, sizeof(cookie_ans));
        recv(sock, cookie_ans, sizeof(cookie_ans), 0);

        string ans(cookie_ans);
        if (ans != "NULL")
        {
            login = true;
            login_name = ans;
        }
    }
    if (!login)
        show_menu();

    while (1)
    {
        if (login)
            break;
        if (!login)
            cin >> choice;
        if (choice == "0")
            break;
        else if (choice == "1")
        {
            while (1)
            {
                cout << "用户名:";
                cin >> name;
                cout << "密码:";
                cin >> password;

                string str = "login" + name;
                str += "pass:" + password;

                send(sock, str.c_str(), str.length(), 0);
                char buffer[1000];
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer), 0);

                string resp(buffer);

                if (resp.substr(0, 2) == "ok")
                {
                    login = true;
                    login_name = name;

                    // 本地建立cookie文件保存sessionid
                    string sessionid = resp.substr(2);
                    string cmd = "cat > cookie.txt <<end \n" + sessionid + "\nend";
                    system(cmd.c_str());

                    cout << "登录成功\n\n";
                    break;
                }
                else
                {
                    cout << "用户名或密码错误！\n\n";
                }
            }
        }
        else if (choice == "2")
        {
            cout << "输入用户名：";
            cin >> name;
            while (1)
            {
                cout << "密码：";
                cin >> password;
                cout << "确认密码：";
                cin >> confirm_password;
                if (confirm_password == password)
                    break;
                else
                    cout << "两次输入的密码不一致!\n\n";
            }

            name = "name:" + name;
            password = "pass:" + password;
            send(conn, (name + password).c_str(), name.length() + password.length(), 0);
            char buffer[1000];
            memset(buffer, 0, sizeof(buffer));
            recv(sock, buffer, sizeof(buffer), 0);
            string rec(buffer);

            if (rec == "ok")
            {
                cout << "注册成功！\n";
                cout << "\n继续选择: ";
            }
            else
            {
                cout << "用户名已存在，注册失败！\n\n";
                cout << "\n继续选择: ";
            }
        }
        else
        {
            cout << "功能不存在，请重新输入: \n";
        }
    }

    while (login)
    {
        system("clear");
        cout << "                 欢迎," << login_name << endl;
        show_chatmenu();

        cin >> choice;
        if (choice == "0")
            break;
        if (choice == "1")
        {
            string username;
            cout << "请输入对方用户名: \n";
            cin >> username;
            string sendstr("target:" + username + "from:" + login_name); // 标识目标用户+源用户
            send(sock, sendstr.c_str(), sendstr.length(), 0);            // 先向服务器发送目标用户、源用户
            cout << "建立连接成功，可发送消息(输入exit退出): \n";
            thread t(Client::SendMsg, conn);
            thread t1(Client::RecvMsg, conn);
            t.join();
            t1.join();
            break;
        }
        else if (choice == "2")
        {
            cout << "请输入群号: ";
            int num;
            cin >> num;
            string sendstr("group:" + to_string(num));
            send(sock, sendstr.c_str(), sendstr.length(), 0);
            cout << "加入成功，可发送消息(输入exit退出): \n";
            thread t1(Client::SendMsg, -conn); // 创建发送线程，传入负数，和私聊区分开
            thread t2(Client::RecvMsg, conn);  // 创建接收线程
            t1.join();
            t2.join();
            break;
        }
        else
        {
            cout << "功能不存在，请重新输入: \n";
        }
    }

    close(sock);
}

void Client::show_chatmenu()
{
    cout << " -------------------------------------------\n";
    cout << "|                                           |\n";
    cout << "|            请选择功能：                   |\n";
    cout << "|              0:退出                       |\n";
    cout << "|              1:发起私聊                   |\n";
    cout << "|              2:发起群聊                   |\n";
    cout << "|                                           |\n";
    cout << " ------------------------------------------- \n\n";
}
