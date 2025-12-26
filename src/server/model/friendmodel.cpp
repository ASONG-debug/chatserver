#include "friendmodel.hpp"
#include"db.h"
#include<vector>
#include<string>
using namespace std;
// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values('%d', '%d')",userid,friendid);
    
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 返回用户好友列表 联合查询friend,user表
vector<User> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "SELECT user.id, user.name, user.state FROM friend INNER JOIN user ON friend.friendid = user.id WHERE friend.userid = %d", userid);

    vector<User>vec;
    User user;
    MySQL mysql;
    if (mysql.connect())
    {
        // 查询到之后用user对象接收
        MYSQL_RES *res = mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            //把userid用户的所有离线消息放入vec中返回
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);//释放指针
            return vec;
        }
    }
    return vec;
}



