# Protobuf 核心 API 学习指南

> 本文档整理了在 mprpc 项目中使用到的 Protobuf 核心类和方法，帮助理解 RPC 框架的反射机制。

---

## 目录

- [1. Message - 消息基类](#1-message---消息基类)
- [2. Service - 服务基类](#2-service---服务基类)
- [3. ServiceDescriptor - 服务描述符](#3-servicedescriptor---服务描述符)
- [4. MethodDescriptor - 方法描述符](#4-methoddescriptor---方法描述符)
- [5. Closure - 回调闭包](#5-closure---回调闭包)
- [6. RpcController - RPC 控制器](#6-rpccontroller---rpc-控制器)
- [7. 完整调用示例](#7-完整调用示例)
- [8. 常见问题 FAQ](#8-常见问题-faq)

---

## 1. Message - 消息基类

### 类定义

```cpp
namespace google::protobuf {
    class Message {
    public:
        // ========== 序列化/反序列化 ==========
        
        // 序列化：对象 → 字节流
        bool SerializeToString(std::string* output) const;
        
        // 反序列化：字节流 → 对象
        bool ParseFromString(const std::string& data);
        
        // 序列化到数组
        bool SerializeToArray(void* data, int size) const;
        
        // 从数组反序列化
        bool ParseFromArray(const void* data, int size);
        
        // ========== 反射相关 ==========
        
        // 克隆当前对象（原型模式）
        virtual Message* New() const = 0;
        
        // 获取消息的描述符
        virtual const Descriptor* GetDescriptor() const = 0;
        
        // 获取反射接口（用于动态访问字段）
        virtual const Reflection* GetReflection() const = 0;
        
        // ========== 其他 ==========
        
        // 清空所有字段
        virtual void Clear() = 0;
        
        // 拷贝另一个消息的内容
        virtual void CopyFrom(const Message& from) = 0;
    };
}
```

### 在 mprpc 中的使用

#### 1.1 反序列化请求参数

**位置**：`rpcprovider.cc:153`

```cpp
// 从网络接收的字节流
std::string args_str = recv_buffer.substr(4 + header_size, args_size);

// 动态创建请求对象
google::protobuf::Message* request = service->GetRequestPrototype(method).New();

// 反序列化：字节流 → 对象
if(!request->ParseFromString(args_str)) {
    std::cout << "request parse error" << std::endl;
}

// 此时 request 已经填充了数据，可以传给业务方法
```

**数据流向**：
```
网络字节流 (args_str)
    ↓
ParseFromString()
    ↓
LoginRequest 对象
    - name = "zhangsan"
    - pwd = "123456"
```

#### 1.2 序列化响应对象

**位置**：`rpcprovider.cc:177`

```cpp
// 业务方法已经填充好 response
google::protobuf::Message* response = ...;

std::string response_str;

// 序列化：对象 → 字节流
if(response->SerializeToString(&response_str)) {
    // 通过网络发送
    conn->send(response_str);
}
```

**数据流向**：
```
LoginResponse 对象
    - success = true
    - errcode = 0
    ↓
SerializeToString()
    ↓
网络字节流 (response_str)
```

#### 1.3 克隆对象（原型模式）

```cpp
// 获取原型对象
const Message& prototype = service->GetRequestPrototype(method);

// 克隆创建新实例
Message* new_instance = prototype.New();

// 等价于（如果知道具体类型）：
LoginRequest* new_instance = new LoginRequest();
```

### 关键点

✅ **所有 Protobuf 消息类型都继承自 Message**  
✅ **序列化/反序列化是 RPC 通信的基础**  
✅ **New() 方法用于动态创建对象（原型模式）**  

---

## 2. Service - 服务基类

### 类定义

```cpp
namespace google::protobuf {
    class Service {
    public:
        // ========== 反射相关 ==========
        
        // 获取服务的描述符
        virtual const ServiceDescriptor* GetDescriptor() = 0;
        
        // ========== 动态调用 ==========
        
        // 动态调用方法（核心！）
        virtual void CallMethod(
            const MethodDescriptor* method,      // 方法描述符
            RpcController* controller,           // 控制器（可为 nullptr）
            const Message* request,              // 请求对象
            Message* response,                   // 响应对象
            Closure* done                        // 完成回调
        ) = 0;
        
        // ========== 原型获取 ==========
        
        // 获取请求类型的原型对象
        virtual const Message& GetRequestPrototype(
            const MethodDescriptor* method
        ) const = 0;
        
        // 获取响应类型的原型对象
        virtual const Message& GetResponsePrototype(
            const MethodDescriptor* method
        ) const = 0;
    };
}
```

### 在 mprpc 中的使用

#### 2.1 动态调用方法

**位置**：`rpcprovider.cc:170`

```cpp
// 已经准备好的参数
Service* service = m_serviceMap[service_name].m_service;
const MethodDescriptor* method = m_methodMap[method_name];
Message* request = ...;   // 已反序列化
Message* response = ...;  // 空对象
Closure* done = ...;      // 回调函数

// 动态调用方法
service->CallMethod(method, nullptr, request, response, done);

// 等价于（如果知道具体类型）：
// userService->Login(nullptr, request, response, done);
```

**CallMethod 内部实现**（Protobuf 生成的代码）：

```cpp
void UserServiceRpc::CallMethod(
    const MethodDescriptor* method,
    RpcController* controller,
    const Message* request,
    Message* response,
    Closure* done)
{
    // 根据方法名分发到具体方法
    if(method->name() == "Login") {
        Login(controller,
              dynamic_cast<const LoginRequest*>(request),
              dynamic_cast<LoginResponse*>(response),
              done);
    }
    else if(method->name() == "Register") {
        Register(controller,
                 dynamic_cast<const RegisterRequest*>(request),
                 dynamic_cast<RegisterResponse*>(response),
                 done);
    }
}
```

#### 2.2 动态创建请求/响应对象

**位置**：`rpcprovider.cc:152-159`

```cpp
// 获取请求类型的原型，并克隆创建新实例
Message* request = service->GetRequestPrototype(method).New();

// 获取响应类型的原型，并克隆创建新实例
Message* response = service->GetResponsePrototype(method).New();
```

**原理**：

```cpp
// GetRequestPrototype 内部实现（Protobuf 生成）
const Message& UserServiceRpc::GetRequestPrototype(
    const MethodDescriptor* method) const
{
    if(method->name() == "Login") {
        static LoginRequest prototype;  // 静态原型对象
        return prototype;
    }
    else if(method->name() == "Register") {
        static RegisterRequest prototype;
        return prototype;
    }
}
```

### 关键点

✅ **Service 是所有 RPC 服务的基类**  
✅ **CallMethod 实现了动态方法调用**  
✅ **GetRequestPrototype/GetResponsePrototype 用于动态创建对象**  

---

## 3. ServiceDescriptor - 服务描述符

### 类定义

```cpp
namespace google::protobuf {
    class ServiceDescriptor {
    public:
        // 获取服务名称
        const std::string& name() const;
        
        // 获取服务的完整名称（包含包名）
        const std::string& full_name() const;
        
        // 获取方法数量
        int method_count() const;
        
        // 通过索引获取方法描述符
        const MethodDescriptor* method(int index) const;
        
        // 通过名称查找方法
        const MethodDescriptor* FindMethodByName(
            const std::string& name
        ) const;
        
        // 获取所属的文件描述符
        const FileDescriptor* file() const;
    };
}
```

### 在 mprpc 中的使用

#### 3.1 服务注册时遍历方法

**位置**：`rpcprovider.cc:22-46`

```cpp
void RpcProvider::NotifyService(Service* service)
{
    ServiceInfo service_info;
    
    // 1. 获取服务描述符
    const ServiceDescriptor* pserviceDesc = service->GetDescriptor();
    
    // 2. 获取服务名称
    std::string service_name = pserviceDesc->name();
    std::cout << "service_name:" << service_name << std::endl;
    
    // 3. 获取方法数量
    int methodCnt = pserviceDesc->method_count();
    
    // 4. 遍历所有方法
    for(int i = 0; i < methodCnt; ++i) {
        // 5. 获取方法描述符
        const MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        
        // 6. 获取方法名称
        std::string method_name = pmethodDesc->name();
        std::cout << "method_name:" << method_name << std::endl;
        
        // 7. 保存到映射表
        service_info.m_methodMap.insert({method_name, pmethodDesc});
    }
    
    // 8. 保存服务信息
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}
```

**输出示例**：
```
service_name:UserServiceRpc
method_name:Login
method_name:Register
method_name:GetFriendList
```

### 关键点

✅ **ServiceDescriptor 包含服务的元数据**  
✅ **通过它可以遍历服务的所有方法**  
✅ **用于服务注册阶段构建映射表**  

---

## 4. MethodDescriptor - 方法描述符

### 类定义

```cpp
namespace google::protobuf {
    class MethodDescriptor {
    public:
        // 获取方法名称
        const std::string& name() const;
        
        // 获取方法的完整名称
        const std::string& full_name() const;
        
        // 获取输入类型（请求类型）
        const Descriptor* input_type() const;
        
        // 获取输出类型（响应类型）
        const Descriptor* output_type() const;
        
        // 获取所属的服务
        const ServiceDescriptor* service() const;
        
        // 获取方法索引
        int index() const;
    };
}
```

### 在 mprpc 中的使用

#### 4.1 保存方法描述符

**位置**：`rpcprovider.cc:40`

```cpp
// 保存方法名 → 方法描述符的映射
service_info.m_methodMap.insert({method_name, pmethodDesc});
```

#### 4.2 使用方法描述符

**位置**：`rpcprovider.cc:148-170`

```cpp
// 1. 从映射表中查找方法描述符
const MethodDescriptor* method = mit->second;

// 2. 用于创建请求/响应对象
Message* request = service->GetRequestPrototype(method).New();
Message* response = service->GetResponsePrototype(method).New();

// 3. 用于动态调用方法
service->CallMethod(method, nullptr, request, response, done);
```

### 关键点

✅ **MethodDescriptor 包含方法的元数据**  
✅ **通过它可以获取方法的输入/输出类型**  
✅ **是动态调用的关键参数**  

---

## 5. Closure - 回调闭包

### 类定义

```cpp
namespace google::protobuf {
    class Closure {
    public:
        // 执行回调
        virtual void Run() = 0;
        
        virtual ~Closure();
    };
    
    // ========== 创建回调的工厂函数 ==========
    
    // 无参数回调
    template<typename Class>
    Closure* NewCallback(
        Class* object,
        void (Class::*method)()
    );
    
    // 单参数回调
    template<typename Class, typename Arg1>
    Closure* NewCallback(
        Class* object,
        void (Class::*method)(Arg1),
        Arg1 arg1
    );
    
    // 双参数回调
    template<typename Class, typename Arg1, typename Arg2>
    Closure* NewCallback(
        Class* object,
        void (Class::*method)(Arg1, Arg2),
        Arg1 arg1,
        Arg2 arg2
    );
    
    // ... 更多参数的重载
}
```

### 在 mprpc 中的使用

#### 5.1 创建回调闭包

**位置**：`rpcprovider.cc:160-168`

```cpp
// 创建回调，绑定 SendRpcResponse 方法和参数
google::protobuf::Closure* done = google::protobuf::NewCallback<
    RpcProvider,                          // 类类型
    const muduo::net::TcpConnectionPtr&,  // 第一个参数类型
    google::protobuf::Message*            // 第二个参数类型
>(
    this,                                 // 对象指针
    &RpcProvider::SendRpcResponse,        // 成员函数指针
    conn,                                 // 第一个参数的值
    response                              // 第二个参数的值
);
```

**等价理解**：

```cpp
// 创建一个闭包，当调用 done->Run() 时，相当于执行：
this->SendRpcResponse(conn, response);
```

#### 5.2 执行回调

**位置**：`example/callee/userservice.cc:44`

```cpp
void UserService::Login(..., Closure* done)
{
    // 1. 处理业务逻辑
    std::string name = request->name();
    std::string pwd = request->pwd();
    bool login_result = Login(name, pwd);
    
    // 2. 填充响应
    response->set_success(login_result);
    response->set_errcode(0);
    
    // 3. 执行回调（触发框架发送响应）
    done->Run();
}
```

**调用链**：

```
业务层：done->Run()
    ↓
框架层：SendRpcResponse(conn, response)
    ↓
    1. response->SerializeToString(&response_str)
    2. conn->send(response_str)
    3. conn->shutdown()
```

### 回调的作用

#### 为什么需要回调？

**问题**：业务方法可能是异步的

```cpp
void Login(..., Closure* done)
{
    // 发起异步数据库查询
    db->asyncQuery("SELECT ...", [response, done](Result result) {
        // 数据库结果回来后
        response->set_success(result.success);
        
        // 通知框架发送响应
        done->Run();
    });
    
    // 这里不能立即发送响应，因为数据库查询还没完成
}
```

**解决方案**：由业务层决定何时调用 `done->Run()`

### 关键点

✅ **Closure 是回调闭包，用于延迟执行**  
✅ **NewCallback 可以绑定成员函数和参数**  
✅ **业务层通过 done->Run() 通知框架发送响应**  

---

## 6. RpcController - RPC 控制器

### 类定义

```cpp
namespace google::protobuf {
    class RpcController {
    public:
        // 重置控制器状态
        virtual void Reset() = 0;
        
        // 检查是否失败
        virtual bool Failed() const = 0;
        
        // 获取错误信息
        virtual std::string ErrorText() const = 0;
        
        // 设置失败状态
        virtual void SetFailed(const std::string& reason) = 0;
        
        // 是否被取消
        virtual bool IsCanceled() const = 0;
        
        // 取消 RPC 调用
        virtual void StartCancel() = 0;
        
        // 设置取消回调
        virtual void NotifyOnCancel(Closure* callback) = 0;
    };
}
```

### 在 mprpc 中的使用

**位置**：`rpcprovider.cc:170`

```cpp
// 目前传的是 nullptr，表示不需要控制器
service->CallMethod(method, nullptr, request, response, done);
```

### 使用场景（扩展）

如果需要错误处理和取消功能，可以这样用：

```cpp
class MyRpcController : public RpcController {
    // 实现接口...
};

void UserService::Login(RpcController* controller, ...)
{
    // 检查是否被取消
    if(controller && controller->IsCanceled()) {
        return;
    }
    
    // 业务逻辑
    bool success = doLogin(...);
    
    if(!success) {
        // 设置错误
        controller->SetFailed("用户名或密码错误");
        done->Run();
        return;
    }
    
    // 正常响应
    response->set_success(true);
    done->Run();
}
```

### 关键点

✅ **RpcController 用于错误处理和取消控制**  
✅ **目前 mprpc 项目中传的是 nullptr**  
✅ **可以作为后续扩展功能**  

---

## 7. 完整调用示例

### 7.1 服务注册阶段

```cpp
// ========== 用户代码 ==========
UserService userService;
RpcProvider provider;
provider.NotifyService(&userService);

// ========== 框架内部 ==========
void RpcProvider::NotifyService(Service* service)
{
    // 1. 获取服务描述符
    const ServiceDescriptor* desc = service->GetDescriptor();
    std::string service_name = desc->name();  // "UserServiceRpc"
    
    // 2. 遍历所有方法
    for(int i = 0; i < desc->method_count(); ++i) {
        const MethodDescriptor* method = desc->method(i);
        std::string method_name = method->name();  // "Login"
        
        // 3. 保存到映射表
        m_methodMap[method_name] = method;
    }
    
    // 4. 保存服务对象
    m_serviceMap[service_name].m_service = service;
}
```

### 7.2 请求处理阶段

```cpp
void RpcProvider::OnMessage(...)
{
    // ========== 1. 解析协议 ==========
    std::string recv_buffer = buffer->retrieveAllAsString();
    
    // 读取 header_size
    uint32_t header_size = 0;
    recv_buffer.copy((char*)&header_size, 4, 0);
    
    // 提取并反序列化 RpcHeader
    std::string rpc_header_str = recv_buffer.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    rpcHeader.ParseFromString(rpc_header_str);
    
    std::string service_name = rpcHeader.service_name();  // "UserServiceRpc"
    std::string method_name = rpcHeader.method_name();    // "Login"
    uint32_t args_size = rpcHeader.args_size();
    
    // 提取参数
    std::string args_str = recv_buffer.substr(4 + header_size, args_size);
    
    // ========== 2. 查找服务和方法 ==========
    Service* service = m_serviceMap[service_name].m_service;
    const MethodDescriptor* method = m_methodMap[method_name];
    
    // ========== 3. 动态创建请求/响应对象 ==========
    Message* request = service->GetRequestPrototype(method).New();
    request->ParseFromString(args_str);
    
    Message* response = service->GetResponsePrototype(method).New();
    
    // ========== 4. 创建回调 ==========
    Closure* done = NewCallback<RpcProvider,
                                const TcpConnectionPtr&,
                                Message*>
                                (this, &SendRpcResponse, conn, response);
    
    // ========== 5. 动态调用方法 ==========
    service->CallMethod(method, nullptr, request, response, done);
}
```

### 7.3 业务处理阶段

```cpp
void UserService::Login(RpcController* controller,
                        const LoginRequest* request,
                        LoginResponse* response,
                        Closure* done)
{
    // ========== 1. 获取请求参数 ==========
    std::string name = request->name();
    std::string pwd = request->pwd();
    
    // ========== 2. 执行业务逻辑 ==========
    bool login_result = doLogin(name, pwd);
    
    // ========== 3. 填充响应 ==========
    response->set_success(login_result);
    response->set_errcode(login_result ? 0 : -1);
    response->set_errmsg(login_result ? "登录成功" : "用户名或密码错误");
    
    // ========== 4. 执行回调 ==========
    done->Run();  // 触发 SendRpcResponse
}
```

### 7.4 响应发送阶段

```cpp
void RpcProvider::SendRpcResponse(const TcpConnectionPtr& conn,
                                   Message* response)
{
    // ========== 1. 序列化响应 ==========
    std::string response_str;
    if(response->SerializeToString(&response_str)) {
        // ========== 2. 发送 ==========
        conn->send(response_str);
    }
    
    // ========== 3. 断开连接 ==========
    conn->shutdown();
}
```

---

## 8. 常见问题 FAQ

### Q1: 为什么需要 `GetRequestPrototype(method).New()` 而不是直接 `new LoginRequest()`？

**答**：因为框架不知道具体类型。

```cpp
// 框架只知道：
std::string service_name = "UserServiceRpc";
std::string method_name = "Login";

// 不知道：
// - 请求类型是 LoginRequest 还是 RegisterRequest？
// - 响应类型是 LoginResponse 还是 RegisterResponse？

// 所以只能通过反射动态创建
Message* request = service->GetRequestPrototype(method).New();
```

### Q2: `service->CallMethod(...)` 和 `done->Run()` 有什么区别？

**答**：

- `CallMethod`：框架调用业务方法（框架 → 业务）
- `done->Run()`：业务通知框架发送响应（业务 → 框架）

```cpp
// 框架层
service->CallMethod(method, nullptr, request, response, done);
    ↓
// 业务层
UserService::Login(...) {
    // 处理业务
    done->Run();
}
    ↓
// 框架层
SendRpcResponse(conn, response);
```

### Q3: `ParseFromString` 和 `SerializeToString` 的区别？

**答**：

- `ParseFromString`：反序列化（字节流 → 对象）
- `SerializeToString`：序列化（对象 → 字节流）

```cpp
// 反序列化
LoginRequest request;
request.ParseFromString(bytes);  // bytes → request

// 序列化
LoginResponse response;
std::string bytes;
response.SerializeToString(&bytes);  // response → bytes
```

### Q4: 为什么 `request` 的类型是 `Message*` 而不是 `LoginRequest*`？

**答**：因为框架是通用的，不能写死具体类型。

```cpp
// 框架代码（通用）
Message* request = service->GetRequestPrototype(method).New();

// 实际指向的动态类型是 LoginRequest*
// 在 CallMethod 内部会进行类型转换：
const LoginRequest* typed_request = dynamic_cast<const LoginRequest*>(request);
```

### Q5: Protobuf 的反射和 C++ 的反射有什么区别？

**答**：

| 特性 | Protobuf 反射 | C++ 反射（提案） |
|------|--------------|-----------------|
| 实现方式 | 代码生成 | 语言特性 |
| 时机 | 运行时 | 编译期/运行时 |
| 范围 | 仅 Protobuf 消息 | 所有 C++ 类型 |
| 标准化 | 已实现 | 尚未标准化 |

Protobuf 反射是通过 `protoc` 编译器生成元数据代码实现的，不是 C++ 语言本身的特性。

### Q6: 为什么需要 `Closure` 回调？

**答**：因为业务方法可能是异步的。

```cpp
// 同步方法
void Login(..., Closure* done) {
    bool result = doLogin(...);
    response->set_success(result);
    done->Run();  // 立即调用
}

// 异步方法
void Login(..., Closure* done) {
    db->asyncQuery(..., [response, done](Result result) {
        response->set_success(result.success);
        done->Run();  // 等数据库结果回来后再调用
    });
    // 这里不能立即发送响应
}
```

由业务层决定何时调用 `done->Run()`，框架才知道何时发送响应。

---

## 9. 学习建议

### 必须掌握（优先级高）

1. ✅ **Message** 的 `SerializeToString` 和 `ParseFromString`
2. ✅ **Service** 的 `CallMethod`、`GetRequestPrototype`、`GetResponsePrototype`
3. ✅ **ServiceDescriptor** 的 `name()`、`method_count()`、`method()`
4. ✅ **MethodDescriptor** 的 `name()`
5. ✅ **Closure** 的 `Run()` 和 `NewCallback`

### 可以暂时不深究

1. ⏸️ **Descriptor** - Message 的描述符（字段级别的反射）
2. ⏸️ **Reflection** - 动态访问字段的接口
3. ⏸️ **RpcController** - 目前项目中传的是 `nullptr`

### 学习路径

```
1. 理解序列化/反序列化
   ↓
2. 理解 Service 和 Message 的关系
   ↓
3. 理解反射机制（Descriptor）
   ↓
4. 理解动态调用（CallMethod）
   ↓
5. 理解回调机制（Closure）
   ↓
6. 完整走通一次 RPC 调用流程
```

---

## 10. 参考资料

- [Protobuf 官方文档](https://protobuf.dev/)
- [Protobuf C++ API Reference](https://protobuf.dev/reference/cpp/api-docs/)
- [mprpc 项目 README](../README.md)

---

**最后更新**：2026-04-27  
**作者**：FrankXie2003
