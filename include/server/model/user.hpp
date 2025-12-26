#pragma once
#include<string>
using namespace std;
//User表的ORM类 （数据库映射成对象）
class User
{
private:
    int id_;
    string name_;
    string password_;
    string state_;
public:
    User(int id=-1,string name="",string pwd="",string state="offline")
    {
        this->id_=id;
        this->name_=name;
        this->password_=pwd;
        this->state_=state;
    }
    void setId(int id){this->id_=id;}
    void setName(string name){this->name_=name;}
    void setPwd(string pwd){this->password_=pwd;}
    void setState(string state){this->state_=state;}

    int getId(){return this->id_;}
    string getName(){return this->name_;}
    string getPwd(){return this->password_;}
    string getState(){return this->state_;}
};