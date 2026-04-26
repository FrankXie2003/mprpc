# mprpc - 基于 Protobuf 和 muduo 的 RPC 框架

一个轻量级、高性能的 C++ RPC 框架，基于 Google Protobuf 和 muduo 网络库实现。

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [技术栈](#技术栈)
- [架构设计](#架构设计)
- [快速开始](#快速开始)
- [核心组件详解](#核心组件详解)
- [Protobuf 反射机制详解](#protobuf-反射机制详解)

---

## 项目简介

mprpc 是一个分布式 RPC（Remote Procedure Call）框架，旨在简化微服务之间的通信。开发者只需定义 Protobuf 服务接口，框架自动处理网络通信、序列化/反序列化、服务注册和方法调度等底层细节。

### 适用场景

- 微服务架构中的服务间通信
- 分布式系统的远程方法调用
- 需要高性能网络通信的 C++ 后端服务

---

## 核心特性

✅ **简单易用**：基于 Protobuf IDL 定义服务，自动生成代码  
✅ **高性能**：基于 muduo 的 Reactor 模式，支持多线程并发  
✅ **动态调度**：利用 Protobuf 反射机制，运行时动态调用方法  
✅ **解耦设计**：业务逻辑与网络层完全分离  
✅ **配置灵活**：支持配置文件管理服务地址和端口  

---

## 技术栈

| 组件 | 技术 | 作用 |
|------|------|------|
| **序列化** | Google Protobuf | 数据序列化/反序列化、服务定义 |
| **网络库** | muduo | 高性能 TCP 网络通信 |
| **构建工具** | CMake | 跨平台构建管理 |
| **平台** | Linux | 主要支持 Linux（依赖 epoll） |

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                     RPC 调用方 (Caller)                  │
│  ┌──────────────────────────────────────────────────┐   │
│  │  RpcChannel (客户端通道)                         │   │
│  │  - 序列化请求                                     │   │
│  │  - 发送网络请求                                   │   │
│  │  - 接收并反序列化响应                             │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            │
                            │ TCP 网络通信
                            ↓
┌─────────────────────────────────────────────────────────┐
│                  RPC 服务提供方 (Provider)               │
│  ┌──────────────────────────────────────────────────┐   │
│  │  RpcProvider (服务端框架)                        │   │
│  │  ┌────────────────────────────────────────────┐  │   │
│  │  │  1. 接收网络请求                           │  │   │
│  │  │  2. 解析 RPC 协议                          │  │   │
│  │  │  3. 反射查找服务和方法                     │  │   │
│  │  │  4. 动态调用业务方法                       │  │   │
│  │  │  5. 序列化响应并发送                       │  │   │
│  │  └────────────────────────────────────────────┘  │   │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  业务服务 (UserService, FriendService...)        │   │
│  │  - 实现具体的业务逻辑                             │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### RPC 通信协议

```
┌────────────┬──────────────────┬──────────────────┐
│ header_size│   header_str     │    args_str      │
│  (4 bytes) │   (变长)         │    (变长)        │
└────────────┴──────────────────┴──────────────────┘
     │              │                    │
     │              │                    └─→ 方法参数（Protobuf 序列化）
     │              └─→ RpcHeader (service_name, method_name, args_size)
     └─→ header_str 的长度
```

**RpcHeader 定义**：
```protobuf
message RpcHeader {
    string service_name = 1;  // 服务名称，如 "UserServiceRpc"
    string method_name = 2;   // 方法名称，如 "Login"
    uint32 args_size = 3;     // 参数大小
}
```

---

## 快速开始

### 1. 定义服务接口（Protobuf）

创建 `user.proto`：

```protobuf
syntax = "proto3";

package fixbug;

option cc_generic_services = true;  // 生成 Service 基类

message LoginRequest {
    bytes name = 1;
    bytes pwd = 2;
}

message LoginResponse {
    int32 errcode = 1;
    bytes errmsg = 2;
    bool success = 3;
}

service UserServiceRpc {
    rpc Login(LoginRequest) returns(LoginResponse);
}
```

### 2. 实现服务端

```cpp
#include "user.pb.h"
#include "rpcprovider.h"
#include "mprpcapplication.h"

// 实现业务服务
class UserService : public fixbug::UserServiceRpc {
public:
    void Login(::google::protobuf::RpcController* controller,
               const ::fixbug::LoginRequest* request,
               ::fixbug::LoginResponse* response,
               ::google::protobuf::Closure* done) override {
        // 获取请求参数
        std::string name = request->name();
        std::string pwd = request->pwd();
        
        // 执行业务逻辑
        bool login_result = doLogin(name, pwd);  // 你的登录逻辑
        
        // 填充响应
        response->set_success(login_result);
        response->set_errcode(login_result ? 0 : -1);
        response->set_errmsg(login_result ? "登录成功" : "用户名或密码错误");
        
        // 执行回调（框架会自动发送响应）
        done->Run();
    }
    
private:
    bool doLogin(const std::string& name, const std::string& pwd) {
        // 实际的登录验证逻辑
        return true;
    }
};

int main(int argc, char** argv) {
    // 1. 初始化框架（加载配置文件）
    MprpcApplication::Init(argc, argv);
    
    // 2. 创建 RPC 服务提供者
    RpcProvider provider;
    
    // 3. 注册服务
    UserService userService;
    provider.NotifyService(&userService);
    
    // 4. 启动服务（阻塞运行）
    provider.Run();
    
    return 0;
}
```

### 3. 配置文件

创建 `test.conf`：

```ini
# RPC 服务器配置
rpcserverip=127.0.0.1
rpcserverport=8000

# ZooKeeper 配置（可选）
zookeeperip=127.0.0.1
zookeeperport=2181
```

### 4. 编译运行

```bash
# 编译
mkdir build && cd build
cmake ..
make

# 运行服务端
./server -i ../test.conf
```

---

## 核心组件详解

### 1. MprpcApplication - 框架初始化

**职责**：框架的全局管理类，负责初始化和配置管理

**核心方法**：
```cpp
// 初始化框架，加载配置文件
static void Init(int argc, char** argv);

// 获取单例
static MprpcApplication& GetInstance();

// 获取配置对象
static MprpcConfig& GetConfig();
```

**使用示例**：
```cpp
MprpcApplication::Init(argc, argv);  // 从命令行参数读取配置文件路径
std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
```

---

### 2. MprpcConfig - 配置管理

**职责**：解析和管理配置文件

**配置格式**：
```ini
# 注释以 # 开头
key=value
```

**核心方法**：
```cpp
// 加载配置文件
void LoadConfigFile(const char* config_file);

// 查询配置项
std::string Load(const std::string& key);
```

---

### 3. RpcProvider - RPC 服务提供者（核心）

**职责**：RPC 服务端的核心组件，负责服务注册、网络通信和方法调度

#### 3.1 核心数据结构

```cpp
// 服务信息结构体
struct ServiceInfo {
    google::protobuf::Service* m_service;  // 服务对象指针
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;
};

// 全局服务注册表
std::unordered_map<std::string, ServiceInfo> m_serviceMap;
```

**数据结构示意**：
```
m_serviceMap
├── "UserServiceRpc"
│   ├── m_service → UserService 对象
│   └── m_methodMap
│       ├── "Login" → MethodDescriptor*
│       └── "Register" → MethodDescriptor*
└── "FriendServiceRpc"
    └── ...
```

#### 3.2 核心方法

##### NotifyService() - 服务注册

```cpp
void NotifyService(::google::protobuf::Service* service);
```

**功能**：将 Protobuf Service 注册到框架中

**执行流程**：
```
1. 通过反射获取服务描述符 (ServiceDescriptor)
   ↓
2. 提取服务名称
   ↓
3. 遍历服务的所有方法，获取方法描述符 (MethodDescriptor)
   ↓
4. 构建服务信息 (ServiceInfo)
   ↓
5. 存入全局服务注册表 (m_serviceMap)
```

**关键代码**：
```cpp
// 获取服务描述信息（反射）
const ServiceDescriptor* desc = service->GetDescriptor();
std::string service_name = desc->name();

// 遍历所有方法
for(int i = 0; i < desc->method_count(); ++i) {
    const MethodDescriptor* method = desc->method(i);
    std::string method_name = method->name();
    service_info.m_methodMap[method_name] = method;
}
```

##### Run() - 启动服务

```cpp
void Run();
```

**功能**：启动 RPC 服务器，开始监听客户端请求

**执行流程**：
```
1. 从配置文件读取 IP 和端口
   ↓
2. 创建 muduo::TcpServer
   ↓
3. 绑定连接回调 (OnConnection)
   ↓
4. 绑定消息回调 (OnMessage)
   ↓
5. 设置线程池大小
   ↓
6. 启动事件循环 (EventLoop::loop)
```

##### OnMessage() - 消息处理（最核心）

```cpp
void OnMessage(const muduo::net::TcpConnectionPtr& conn,
               muduo::net::Buffer* buffer,
               muduo::Timestamp);
```

**功能**：接收并处理客户端的 RPC 请求

**完整执行流程**：

```
┌─────────────────────────────────────────────────────────┐
│ 1. 接收网络数据                                          │
│    recv_buffer = buffer->retrieveAllAsString()          │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 2. 解析协议头                                            │
│    ┌─────────────────────────────────────────────────┐  │
│    │ [4字节] header_size                             │  │
│    │ [变长]  header_str (RpcHeader 序列化)           │  │
│    │ [变长]  args_str (方法参数序列化)               │  │
│    └─────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 3. 反序列化 RpcHeader                                    │
│    rpcHeader.ParseFromString(header_str)                │
│    ├─ service_name = "UserServiceRpc"                   │
│    ├─ method_name = "Login"                             │
│    └─ args_size = 100                                   │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 4. 查找服务和方法（反射）                                │
│    Service* service = m_serviceMap[service_name]        │
│    MethodDescriptor* method = m_methodMap[method_name]  │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 5. 动态创建请求和响应对象（反射）                        │
│    Message* request = service->GetRequestPrototype()    │
│    Message* response = service->GetResponsePrototype()  │
│    request->ParseFromString(args_str)                   │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 6. 创建回调闭包                                          │
│    Closure* done = NewCallback(                         │
│        this, &SendRpcResponse, conn, response)          │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 7. 动态调用业务方法（反射）                              │
│    service->CallMethod(method, nullptr,                 │
│                        request, response, done)         │
│    ↓                                                     │
│    [业务层] UserService::Login() 执行                    │
│    ↓                                                     │
│    done->Run() 触发回调                                  │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ 8. SendRpcResponse() 被调用                              │
│    ├─ 序列化 response                                    │
│    ├─ conn->send(response_str)                          │
│    └─ conn->shutdown() 断开连接                          │
└─────────────────────────────────────────────────────────┘
```

##### SendRpcResponse() - 响应发送

```cpp
void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn,
                     google::protobuf::Message* response);
```

**功能**：序列化响应并发送给客户端

**触发时机**：业务方法执行完成后，由 `done->Run()` 自动触发

**代码逻辑**：
```cpp
std::string response_str;
if(response->SerializeToString(&response_str)) {
    conn->send(response_str);  // 发送响应
}
conn->shutdown();  // 短连接模式，主动断开
```

---

## Protobuf 反射机制详解

### 什么是反射？

**反射（Reflection）** 是指程序在运行时能够检查、访问和修改自身结构和行为的能力。

### C++ 有反射吗？

**C++ 标准库本身不支持反射**，但 Protobuf 通过代码生成实现了一套反射机制。

> **注意**：C++20/23 引入的 `std::meta` 是编译期反射提案，目前还未正式标准化。Protobuf 的反射是运行时反射，两者不同。

### Protobuf 反射机制原理

Protobuf 在编译 `.proto` 文件时，会生成包含元数据的 C++ 代码，这些元数据在运行时可以被查询和使用。

#### 1. 核心反射类

| 类名 | 作用 |
|------|------|
| `Descriptor` | 描述 Message 的结构（字段信息） |
| `ServiceDescriptor` | 描述 Service 的结构（方法列表） |
| `MethodDescriptor` | 描述 Method 的签名（参数和返回类型） |
| `FieldDescriptor` | 描述 Message 中的字段 |

#### 2. 反射在 mprpc 中的应用

##### 应用 1：通过服务名字符串获取服务对象

```cpp
// 传统方式（编译期确定）
UserService* service = new UserService();
service->Login(...);

// 反射方式（运行时动态）
std::string service_name = "UserServiceRpc";  // 从网络接收
Service* service = m_serviceMap[service_name].m_service;  // 动态查找
```

##### 应用 2：通过方法名字符串获取方法描述符

```cpp
// 传统方式
service->Login(...);  // 编译期必须知道方法名

// 反射方式
std::string method_name = "Login";  // 从网络接收
const MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(method_name);
```

##### 应用 3：动态创建请求和响应对象

```cpp
// 传统方式
LoginRequest* request = new LoginRequest();
LoginResponse* response = new LoginResponse();

// 反射方式（不需要知道具体类型）
Message* request = service->GetRequestPrototype(method).New();
Message* response = service->GetResponsePrototype(method).New();
```

**原理**：
- `GetRequestPrototype(method)` 返回该方法的请求类型的原型对象
- `.New()` 克隆一个新实例

##### 应用 4：动态调用方法

```cpp
// 传统方式
service->Login(controller, request, response, done);

// 反射方式
service->CallMethod(method, controller, request, response, done);
```

**CallMethod 内部实现**（Protobuf 生成的代码）：
```cpp
void UserServiceRpc::CallMethod(const MethodDescriptor* method,
                                RpcController* controller,
                                const Message* request,
                                Message* response,
                                Closure* done) {
    // 根据方法名分发到具体方法
    if(method->name() == "Login") {
        Login(controller, 
              dynamic_cast<const LoginRequest*>(request),
              dynamic_cast<LoginResponse*>(response),
              done);
    }
    // ... 其他方法
}
```

#### 3. 反射的优势

✅ **动态性**：运行时根据字符串调用方法，无需硬编码  
✅ **扩展性**：新增服务和方法无需修改框架代码  
✅ **通用性**：框架代码与具体业务解耦  

#### 4. 反射的代价

❌ **性能开销**：相比直接调用有额外的查找和类型转换开销  
❌ **类型安全**：编译期无法检查类型错误  

### 反射示例代码

```cpp
#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include "user.pb.h"

void ReflectionDemo() {
    // 1. 获取服务描述符
    UserServiceRpc service;
    const ServiceDescriptor* serviceDesc = service.GetDescriptor();
    
    std::cout << "服务名: " << serviceDesc->name() << std::endl;
    std::cout << "方法数量: " << serviceDesc->method_count() << std::endl;
    
    // 2. 遍历所有方法
    for(int i = 0; i < serviceDesc->method_count(); ++i) {
        const MethodDescriptor* method = serviceDesc->method(i);
        std::cout << "方法名: " << method->name() << std::endl;
        std::cout << "  输入类型: " << method->input_type()->name() << std::endl;
        std::cout << "  输出类型: " << method->output_type()->name() << std::endl;
    }
    
    // 3. 通过方法名查找方法
    const MethodDescriptor* loginMethod = serviceDesc->FindMethodByName("Login");
    
    // 4. 动态创建请求对象
    Message* request = service.GetRequestPrototype(loginMethod).New();
    
    // 5. 通过反射设置字段值
    const Descriptor* requestDesc = request->GetDescriptor();
    const FieldDescriptor* nameField = requestDesc->FindFieldByName("name");
    const Reflection* reflection = request->GetReflection();
    reflection->SetString(request, nameField, "zhangsan");
}
```

---

## 总结

### mprpc 框架的核心价值

1. **简化开发**：开发者只需关注业务逻辑，无需处理网络细节
2. **高性能**：基于 muduo 的 Reactor 模式，支持高并发
3. **动态调度**：利用 Protobuf 反射，实现运行时方法调用
4. **易于扩展**：新增服务只需实现 Protobuf Service 接口

### 设计模式

- **外观模式**：RpcProvider 封装复杂的网络和序列化细节
- **观察者模式**：通过 Closure 回调实现异步响应
- **工厂模式**：动态创建 request/response 对象

---

## 许可证

MIT License

---

## 作者

FrankXie2003

---

**🚀 Happy Coding!**
