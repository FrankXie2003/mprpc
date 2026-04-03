#pragma once

#include "mprpcconfig.h"

// mprpc框架的基础类.负责框架的一些初始化操作
class MprpcApplication
{
public:
    static void Init(int argc,char** argv);
    static MprpcApplication& GetInstance();
private:
    //普通成员变量不能在静态方法中
    inline static MprpcConfig m_config;

    MprpcApplication();
    MprpcApplication(const MprpcApplication&) = delete;
    MprpcApplication(MprpcApplication&&) = delete;
};