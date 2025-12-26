#pragma once
#include<unordered_map>
#include<functional>
#include<mutex>
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>

#include"redis.hpp"
#include"usermodel.hpp"
#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;

#include"json.hpp"
using json=nlohmann::json;

//处理消息的事件回调方法类型
using MsgHandler=std::function<void(const TcpConnectionPtr&,json&,Timestamp)>;
 
//聊天服务器业务类
class ChatService
{
public: 
    //获取单例对象的接口函数
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    //处理注销业务
    void loginout(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //服务器异常，业务重置方法
    void reset();
    //添加好友业务 msgid id friend
    void addFriend(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //创建群组业务
    void createGroup(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //加入群组业务
    void addGroup(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //群聊天业务
    void groupChat(const TcpConnectionPtr&conn,json&js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    void handleRedisSubscribeMessage(int userid,string msg);
private:
    ChatService();
    //存储 消息key(public.hpp中存放) 和对应业务处理方法
    unordered_map<int,MsgHandler>MsgHandlerMap_;
    
    //存储*在线用户*的通信连接 key是用户id
    unordered_map<int,TcpConnectionPtr>userConnMap_;

    //互斥锁 保证userConnMap_线程安全
    mutex connMutex_;

    //数据操作类对象
    UserModel usermodel_;
    OfflineMsgModel offlinemsgmodel_;
    FriendModel friendmodel_;
    GroupModel groupmodel_;

    //redis操作对象
    Redis redis_;
};
