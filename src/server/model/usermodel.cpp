#include "usermodel.hpp"
#include "db.h"
#include <iostream>
using namespace std;
// User表的增加方法
bool UserModel::insert(User &user)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into user(name,password,state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取成功插入的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// User表的查询 根据用户号码查询用户信息
User UserModel::query(int id)
{
    char sql[1024] = {0};
    sprintf(sql, "SELECT * FROM user WHERE id = %d", id);

    User user;
    MySQL mysql;
    if (mysql.connect())
    {
        // 查询到之后用user对象接收
        MYSQL_RES *res = mysql.query(sql);
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr)
        {
            user.setId(atoi(row[0]));
            user.setName(row[1]);
            user.setPwd(row[2]);
            user.setState(row[3]);
            mysql_free_result(res); // 释放指针
            return user;
        }
    }
    // 没查到,返回默认id -1
    return user;
}

// User表的更新
bool UserModel::updateState(User &user)
{
    char sql[1024] = {0};
    sprintf(sql, "UPDATE user SET state = '%s' WHERE id = %d", user.getState().c_str(), user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}
// 重置用户的状态信息
void UserModel::resetState()
{
    char sql[1024] = {"UPDATE user SET state = 'offline' WHERE state = 'online'"};

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}