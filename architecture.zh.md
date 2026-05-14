
# SRT Router

SRT网关路由器是一个基于C++的网络服务项目，旨在提供安全可靠的传输协议支持。该项目采用了分层架构设计，清晰地划分了核心业务逻辑、工具库和第三方依赖，使得代码结构清晰、模块化程度高。

1. **完整的代码结构分析**
2. **核心模块依赖关系图**
3. **系统架构概念图（文字版）**

---

## 📁 1. 代码结构和组成模块分析

项目根目录为 `C:\Work\src\srt-gateway\srt-router`。整体结构是一个典型的基于CMake的C++工程，将本地业务逻辑、通用的工具库和第三方依赖进行了清晰的分层。

| 模块路径 | 描述 | 内容/功能推测 | 依赖关系 |
| :--- | :--- | :--- | :--- |
| **`SrtRouter/`** | **核心业务逻辑层** | 包含应用程序的入口点和主要的路由/处理逻辑。这是项目的主体代码。 | **依赖**：`libcommon`, `libwebserver`, `libwebsocketcommon`，以及所有第三方库。 |
| `SrtRouter/logger.cpp/h` | 日志记录器 | 负责整个应用的日志管理，保证日志的一致性。 | **内部依赖**：基础系统。 |
| `SrtRouter/util.cpp/h` | 工具类/辅助函数 | 放置通用的、非业务特定的辅助函数和数据结构。 | **内部依赖**：基础系统。 |
| `SrtRouter/main.cpp` | **程序入口点** | 负责程序的初始化、配置加载（`srtrouterconfig.json`）和启动核心服务（Web/WebSocket Server）。 | **依赖**：`libwebserver`，`util`。 |
| `SrtRouter/srtRouter.cpp/h` | **核心路由实现** | 实现具体的SRT路由和连接管理逻辑。 | **依赖**：`libcommon`, `libwebserver`。 |
| **`cmake/`** | **构建系统配置** | 包含 `common.cmake`，用于定义和管理构建过程中通用的宏、变量和库引用。 | **内部依赖**：CMake构建过程。 |
| **`lib/`** | **本地工具库** | 存放项目内部重用、可以独立编译的通用库。 | **自包含**：这是项目定制化的核心模块。 |
| `libcommon/` | 通用工具库 | 包含数据结构、常用常量、通用函数等。 | **依赖**：基础系统。 |
| `libwebserver/` | Web服务库 | 负责HTTP/HTTPS服务器的搭建和管理。 | **依赖**：`libcommon`。 |
| `libwebsocketcommon/` | WebSocket处理库 | 负责实时、双向通信（如WebSocket）的连接和消息处理。 | **依赖**：`libcommon`。 |
| **`3rd/`** | **第三方依赖库** | 包含项目所需的外部开源库（如Boost, libjson, openssl, spdlog）。虽然您要求略过分析，但它们是项目运行的**关键外部依赖**。 | **关键依赖**：Boost, JSON, 日志系统, 加密协议。 |

---

## 🧩 2. 核心模块依赖关系图

本项目是一个分层架构。依赖关系从最底层的工具库（第三方/本地）层，向上层应用逻辑递进。

### 🟢 外部依赖层 (External Dependencies)

这部分是所有模块共用的基础设施：

* `Boost` (全功能)
* `libjson` (JSON数据序列化与反序列化)
* `spdlog` (高级日志记录系统)
* `openssl` (加密、网络安全协议支持)

### 🟠 本地工具库层 (Local Libraries)

位于 `lib/` 目录下，提供给 `SrtRouter` 业务层的抽象和封装，实现了代码复用和解耦。

* `libcommon`
  * ➡️ **依赖**：*无（最低层）*
* `libwebserver`
  * ➡️ **依赖**：`libcommon`
* `libwebsocketcommon`
  * ➡️ **依赖**：`libcommon`

### 🔴 业务应用层 (Application Core)

这是最终执行任务的模块，它将所有依赖和工具库连接起来。

* **`SrtRouter`** (整个`SrtRouter`目录下的所有源代码)
  * **依赖**：
    1. **本地依赖**：`libcommon`, `libwebserver`, `libwebsocketcommon`
    2. **外部依赖**：`boost`, `libjson`, `spdlog`, `openssl`
  * **输入/配置**：读取 `srtrouterconfig.json`，并使用`README.md`或`version.properties`等文件。
  * **输出**：启动一个根据配置定义的Web/WebSocket服务。

**依赖总结路径：**
`{Boost, JSON, OpenSSL, spdlog}` $\rightarrow$ `libcommon` $\rightarrow$ `{libwebserver, libwebsocketcommon}` $\rightarrow$ **`SrtRouter` 应用程序**

---

### 💡 3. 系统架构概念图（文字化表达）

基于CMake的构建流程，项目是一个典型的**分层三层架构 (N-Tier Architecture)** 的网络服务。

**系统名称：SRT 网关路由器 (SrtRouter)**

```mermaid
graph TD
    A[配置输入: srtrouterconfig.json] --> B(应用层: SrtRouter Core);
    C[通用组件: 3rd/boost, libjson, spdlog, openssl] --> D[本地工具库层];
    D --> E[SrtRouter Core: main.cpp, srtRouter.cpp];
    
    subgraph ⚙️ 核心服务启动 (SrtRouter)
        E --> F(Web Server / WebSocket);
        E --> G(SRT路由逻辑);
    end
    
    subgraph 📚 本地工具库
        D1[libcommon]
        D2[libwebserver]
        D3[libwebsocketcommon]
        D1 --> D2;
        D1 --> D3;
        D2 --> E;
        D3 --> E;
        D1 --> G;
    end
    
    B --> C;
```

**流程描述：**

1. **启动初始化 (Bootstrap):** 进程通过 `main.cpp` 启动，读取 `srtrouterconfig.json` 获取运行时参数。
2. **基础设施层 (Infrastructure):** 系统利用 `openssl` (加密) 和 `libjson` (配置解析) 初始化网络连接。
3. **服务层 (Service Layer):** `SrtRouter` 核心模块通过 `libwebserver` 启动 HTTP/Web 服务，并通过 `libwebsocketcommon` 开启实时双向通信通道。
4. **业务逻辑层 (Business Logic):** 路由模块 `srtRouter.cpp` 负责接收和分发通过 WebSocket 或 HTTP 接收到的SRT流数据包，调用 `libcommon` 中的通用功能进行处理、鉴权和转发。
5. **数据流 (Data Flow):** 所有的处理过程都是围绕数据包的**接收** $\rightarrow$ **解析/处理** $\rightarrow$ **发送**循环进行的。

**总结：**

这是一个高度依赖C++原生能力、设计严谨的实时网络服务，将网络传输（Web/WS）与业务路由（SRT）模块化分离，并通过本地工具库和大量外部依赖来构建其复杂的功能集合。