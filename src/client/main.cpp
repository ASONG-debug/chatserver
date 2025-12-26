#include "public.hpp"
#include "group.hpp"
#include "user.hpp"

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>

#include "json.hpp"
#include <iostream>
#include <thread>
#include<condition_variable>
#include<mutex>
#include<atomic>
#include <string>
#include <string>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;
// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentGroupList;
// 显示当前成功登录用户的基本信息
void showCurrentUserData();
// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);
// 控制聊天页面程序
bool isMainMenuRunning = false;

// 主线程用作发送，子线程用作接收
int main(int argc, char *argv[])
{
    // 客户端的连接
    if (argc != 3)
    {
        printf("usage:./ChatClient ip port\n");
        printf("example:./ChatClient 192.168.48.132 6000\n\n");
        exit(EXIT_FAILURE);
    }
    int clientfd;
    struct sockaddr_in servaddr;
    char buf[1024];
    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("socket() failed.\n");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(clientfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("connect(%s:%s) failed.\n", argv[1], argv[2]);
        close(clientfd);
        exit(EXIT_FAILURE);
    }
    printf("connect ok.\n");

    for (;;)
    {
        // 显示首页面菜单  登录 注册 退出
        cout << "=========================Welcome!=========================" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "==========================================================" << endl;
        cout << "choice:";
        int choice=0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0); //+1 是有发送字符串结束符\0
            if (len == -1)
            {
                cerr << "send login msg error!"<<endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (len == -1)
                {
                    cerr << "recv reg msg error!";
                }
                else
                {
                    // 接收服务端消息
                    json responsejs = json::parse(buffer);
                    if (responsejs["errno"] != 0) // 登陆失败
                    {
                        cerr << responsejs["errmsg"]<<endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);
                        // 记录当前用户的好友列表
                        if (responsejs.contains("friends"))
                        {
                            g_currentUserFriendList.clear();

                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json friendMessageJs = json::parse(str);
                                User user;
                                user.setId(friendMessageJs["id"].get<int>());
                                user.setName(friendMessageJs["name"]);
                                user.setState(friendMessageJs["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }
                        // 记录当前用户的群组列表信息
                        if (responsejs.contains("groups"))
                        {
                            g_currentGroupList.clear();
                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                // 群用户
                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentGroupList.push_back(group);
                            }
                        }
                    }
                    // 显示登录用户的基本信息
                    showCurrentUserData();

                    // 显示当前用户的离线消息(私聊消息，群聊消息)
                    if (responsejs.contains("offlinemsg")) // 有离线消息
                    {
                        vector<string> vec = responsejs["offlinemsg"];
                        for (string &str : vec)
                        {
                            json js = json::parse(str);
                            // time + [id] + name +" said: "+xxx
                            if (ONE_CHAT_MSG == js["msgid"].get<int>())
                            {
                                cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                     << " said: " << js["msg"].get<string>() << endl;
                            }
                            else
                            {
                                cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                                     << " said: " << js["msg"].get<string>() << endl;
                            }
                        }
                    }
                    // 登录成功，启动接收线程负责接收数据
                    // 这个线程只启动一次，但由于重复登录会循环，需要设置静态变量保证只执行一次
                    static int threadnumber = 0;
                    if (threadnumber == 0)
                    {
                        std::thread readTask(readTaskHandler, clientfd);
                        readTask.detach(); // 分离线程
                        threadnumber++;
                    }
                    // 进入聊天主菜单页面
                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }
            }
        }
        break;
        case 2: // register
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50); // 读取整行，遇到回车才结束
            cout << "password:";
            cin.getline(pwd, 50);

            // 给服务端发送js需要对应msgid
            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), request.size() + 1, 0); //+1 是有发送字符串结束符\0
            if (len == -1)
            {
                cerr << "send reg msg error!";
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (len == -1)
                {
                    cerr << "recv reg msg error!";
                }
                else
                {
                    // 接收服务端消息
                    json responsejs = json::parse(buffer);
                    if (responsejs["errno"] == 1) // 注册失败
                    {
                        cerr << name << " register error!" << endl;
                    }
                    else // 注册成功
                    {
                        cout << name << " register successs,userid is " << responsejs["id"]
                             << ",do not forget it!" << endl;
                    }
                }
            }
        }
        break;
        case 3:
            close(clientfd);
            exit(EXIT_SUCCESS);
            break;
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
}

// 显示当前成功登录用户的基本信息
void showCurrentUserData()
{
    cout << "=========================login user=========================" << endl;
    cout << "current login user=>id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "-------------------------friend list------------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "-------------------------group list------------------------" << endl;
    if (!g_currentGroupList.empty())
    {
        for (Group &group : g_currentGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "============================================================" << endl;
}
// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(EXIT_FAILURE);
        }
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        // 打印私聊信息
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        // 打印群聊消息
        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}
// 获取系统时间
string getCurrentTime()
{
    // 获取当前时间点
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    // 转换为本地时间结构体
    struct tm *ptm = localtime(&tt);

    char date[60] = {0};
    // 格式化字符串: YYYY-MM-DD HH:MM:SS
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);

    return std::string(date);
}

//
void help(int fd = 0, string str = "");

void chat(int, string);

void addfriend(int, string);

void creategroup(int, string);

void addgroup(int, string);

void groupchat(int, string);

void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) // 没找到这个命令
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用对应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1)); // 截取冒号后面输入的参数
    }
}

void help(int, string)
{
    cout << "show command list>>>" << endl;
    for (auto &mp : commandMap)
    {
        cout << mp.first << " : " << mp.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}

void chat(int clientfd, string str)
{
    // friendid:message
    int idx = str.find(':');
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}

void creategroup(int clientfd, string str)
{
    // groupname:groupdesc
    int idx = str.find(':');
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error ->" << buffer << endl;
    }
}

void addgroup(int clientfd, string str)
{
    // groupid
    int groupid = atoi(str.c_str());

    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;

    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error ->" << buffer << endl;
    }
}

void groupchat(int clientfd, string str)
{
    // groupid:message
    int idx = str.find(':');
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error ->" << buffer << endl;
    }
}

void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();

    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size() + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error ->" << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}