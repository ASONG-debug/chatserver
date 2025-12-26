#pragma once
#include"user.hpp"
//增删改查
//User表的数据操作类
class UserModel
{
private:
    
public:
    //User表的增加方法
    bool insert(User &user);
    //User表的查询 根据用户号码查询用户信息
    User query(int id);
    //User更新状态信息
    bool updateState(User &user);
    //重置用户的状态信息
    void resetState();
};
