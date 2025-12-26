#pragma once
#include<string>
#include<vector>
#include"groupuser.hpp"
using namespace std;
//Group表的ORM类 （数据库映射成对象）
class Group
{
private:
    int id;
    string name;
    string desc;
    vector<GroupUser> users;
public:
    Group(int id=-1,string name="",string desc="")
    {
        this->id=id;
        this->name=name;
        this->desc=desc;
    }
    void setId(int id){this->id=id;}
    void setName(string name){this->name=name;}
    void setDesc(string desc){this->desc=desc;}
    int getId(){return this->id;}
    string getName(){return this->name;}
    string getDesc(){return this->desc;}
    vector<GroupUser> &getUsers(){return this->users;}//&引用，不必开辟新空间

};