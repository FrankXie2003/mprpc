#pragma once

#include <unordered_map>
#include <string>

// 框架读取配置文件类
// rpcserver_ip  rpcserver_port  zookeeper_ip  zookeeper_port
class MprpcConfig
{
public:
private:
    std::unordered_map<std::string, std::string> m_configMap;
};