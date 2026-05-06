#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <iostream>
#include <unistd.h>

int main(int argc, char** argv)
{
    // 必须先 Init，因为 ZkClient::Start 需要读配置
    MprpcApplication::Init(argc, argv);

    ZkClient zkCli;
    zkCli.Start();
    std::cout << "===== ZK 连接成功 =====" << std::endl;

    // 测试创建节点
    const char* path = "/test_service";
    const char* data = "hello_zk";
    zkCli.Create(path, data, strlen(data));

    // 测试读取节点
    std::string read_data = zkCli.GetData(path);
    std::cout << "读取节点数据: " << read_data << std::endl;

    // 保持进程，方便用 zkCli.sh 验证节点是否存在
    std::cout << "===== 按 Ctrl+C 退出 =====" << std::endl;
    pause();

    return 0;
}
