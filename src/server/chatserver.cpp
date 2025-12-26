#include"chatserver.hpp"
#include"chatservice.hpp"
#include"json.hpp"
#include<iostream>
#include<string>
#include<functional>
using json=nlohmann::json;
using namespace std;
using namespace placeholders;

//初始化聊天服务器对象
ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg)
            :server_(loop,listenAddr,nameArg)
            ,loop_(loop)
{ 
    //注册连接回调
    server_.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
    //注册读写回调              
    server_.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));  
    //设置IO线程数量
    server_.setThreadNum(4);
}
//启动服务
void ChatServer::start()
{
    server_.start();
}
//上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    //客户端断开连接
    if(!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown(); 
    }
}
//上报读写事件相关信息的回调函数    
void ChatServer::onMessage(const TcpConnectionPtr& conn,Buffer*buffer,Timestamp time)
{
    string buf=buffer->retrieveAllAsString();
    json js=json::parse(buf);//转换成json对象 反序列化成json对象   dump()是序列化
    //完全解耦网络模块和业务模块
    //通过js["msgid"]获取业务的handler处理器
    auto msghandler=ChatService::instance()->getHandler(js["msgid"].get<int>());
    msghandler(conn,js,time);//这个函数对象就是msgid对应的业务函数，执行
}