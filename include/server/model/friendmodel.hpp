#pragma once
#include"user.hpp"
#include<vector>
using namespace std;
//维护好友信息的操作接口
//friend表的操作
class FriendModel
{
public:
    //添加好友关系
    void insert(int userid,int friendid);
    //返回用户好友列表 联合查询friend,user表
    vector<User> query(int userid);

private:

};