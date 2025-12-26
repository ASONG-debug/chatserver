#pragma once
#include<string>
#include"user.hpp"
using namespace std;
//群组用户，从User类直接继承，复用User的其他信息
class GroupUser:public User
{
private:
    string role;//群内角色

public:
    void setRole(string role){this->role=role;}
    string getRole(){return this->role;}

};