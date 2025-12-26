#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <string>
using namespace std;
// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
// 注册消息以及对应的Handler回调操作 (构造函数)
ChatService::ChatService()
{
    MsgHandlerMap_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    MsgHandlerMap_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    MsgHandlerMap_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    MsgHandlerMap_.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    MsgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    MsgHandlerMap_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    MsgHandlerMap_.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    MsgHandlerMap_.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    //连接redis服务器
    if(redis_.connect())
    {
        //设置上报消息的回调
        redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}
// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = MsgHandlerMap_.find(msgid);
    if (it == MsgHandlerMap_.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &, json &, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find handler!";
        };
    }
    else
    {
        return MsgHandlerMap_[msgid];
    }
}
// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    usermodel_.resetState();
}

// 处理登录业务 需要: id password
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];

    User user = usermodel_.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; // 重复登录
            response["errmsg"] = "this account is using,input another!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功 记录用户连接
            {
                // 线程安全
                lock_guard<mutex> gb(connMutex_);
                userConnMap_.insert({id, conn});
            }
            //id用户登录成功，向redis订阅channel(id) 对此通道的消息感兴趣
            redis_.subscribe(id);

            // 登陆成功 更新用户状态信息online
            user.setState("online");
            usermodel_.updateState(user);

            // 服务器统一把所有消息发给客户端 把所有消息放在response中
            //(1.离线消息 2.好友信息 3.群组信息)每次登录都能看到
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 0表示没错
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线消息
            vector<string> vec = offlinemsgmodel_.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;

                // 读取离线消息后，删除
                offlinemsgmodel_.remove(id);
            }
            // 查询该用户的好友信息并返回
            vector<User> userVec = friendmodel_.query(id);
            if (!userVec.empty())
            {
                vector<string> friendList;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    friendList.push_back(js.dump());
                }
                response["friends"] = friendList;
            }
            // 查询用户的群组信息并返回
            vector<Group> groupuserVec = groupmodel_.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    // 群用户
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1; // 1表示有错
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}
// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    // 1.找到userConnMap表中conn,然后删除
    User user;
    {
        lock_guard<mutex> lock(connMutex_);
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); it++)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }
    //redis取消订阅通道
    redis_.unsubscribe(user.getId());

    // 2.通过id在数据库中修改其状态为"offline"
    // 可能没找到这个用户，返回的id是-1
    if (user.getId() != -1)
    {
        user.setState("offline");
        usermodel_.updateState(user);
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid=js["id"].get<int>();

    {
        lock_guard<mutex>lock(connMutex_);
        auto it=userConnMap_.find(userid);
        if(it!=userConnMap_.end())
        {
            userConnMap_.erase(it);
        }
    }
    //用户注销 下线，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    User user(userid,"","","offline");
    usermodel_.updateState(user);
}

// 处理注册业务 需要：name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPwd(password);

    bool state = usermodel_.insert(user);
    // 注册完成之后，要回复客户端
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0; // 0表示没错
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1; // 1表示有错
        conn->send(response.dump());
    }
}
// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //在同一个服务器上,可以找到对方的tcp连接
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // toid在线 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }
    //不在一个服务器上，查一下数据库看看是否在线,在线就将发来的消息发布在redis消息队列，等待对方订阅接收
    User user=usermodel_.query(toid);
    if(user.getState()=="online")
    {
        redis_.publish(toid,js.dump());
        return;
    }
    // toid不在线，存储离线消息
    offlinemsgmodel_.insert(toid, js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    // 写入friend表中
    friendmodel_.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    string name = js["groupname"];
    string desc = js["groupdesc"];
    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (groupmodel_.createGroup(group))
    {
        groupmodel_.enterGroup(userid, group.getId(), "creator");
    }
}
// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int groupid = js["groupid"];
    groupmodel_.enterGroup(userid, groupid, "normal");
}
// 群聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];
    int groupid = js["groupid"];
    vector<int> useridVec = groupmodel_.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(connMutex_);
    for (int id : useridVec)
    {
        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end())
        {
            // 转发消息
            it->second->send(js.dump());
        }
        else
        {
            User user=usermodel_.query(id);
            if(user.getState()=="online")
            {
                redis_.publish(id,js.dump());
            }
            // 存储离线消息
            offlinemsgmodel_.insert(id, js.dump());
        }
    }
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    lock_guard<mutex>lock(connMutex_);
    auto it=userConnMap_.find(userid);
    if(it!=userConnMap_.end())
    {
        it->second->send(msg);
        return;
    }
    //没找到 存储离线信息
    offlinemsgmodel_.insert(userid,msg);
}
