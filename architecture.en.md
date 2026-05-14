# SRT Router

The SRT Gateway Router is a C++-based network service project aimed at providing secure and reliable transport protocol support. The project adopts a layered architecture design, clearly dividing core business logic, tool libraries, and third-party dependencies, making the code structure clear and highly modular.

1. **Complete code structure analysis**
2. **Core module dependency diagram**
3. **System architecture conceptual diagram (text version)**

---

## 📁 1. Code Structure and Component Module Analysis

The project root directory is `C:\Work\src\srt-gateway\srt-router`. The overall structure is a typical CMake-based C++ project, which clearly layers local business logic, general tool libraries, and third-party dependencies.

| Module Path | Description | Content/Function Inference | Dependencies |
| :--- | :--- | :--- | :--- |
| **`SrtRouter/`** | **Core Business Logic Layer** | Contains the application's entry point and main routing/processing logic. This is the main code of the project. | **Depends on**: `libcommon`, `libwebserver`, `libwebsocketcommon`, and all third-party libraries. |
| `SrtRouter/logger.cpp/h` | Logger | Responsible for log management across the entire application, ensuring log consistency. | **Internal dependencies**: Basic system. |
| `SrtRouter/util.cpp/h` | Utility Classes/Helper Functions | Places general, non-business-specific helper functions and data structures. | **Internal dependencies**: Basic system. |
| `SrtRouter/main.cpp` | **Program Entry Point** | Responsible for program initialization, configuration loading (`srtrouterconfig.json`), and starting core services (Web/WebSocket Server). | **Depends on**: `libwebserver`, `util`. |
| `SrtRouter/srtRouter.cpp/h` | **Core Routing Implementation** | Implements specific SRT routing and connection management logic. | **Depends on**: `libcommon`, `libwebserver`. |
| **`cmake/`** | **Build System Configuration** | Contains `common.cmake`, used to define and manage common macros, variables, and library references during the build process. | **Internal dependencies**: CMake build process. |
| **`lib/`** | **Local Tool Libraries** | Stores project-internal reusable, independently compilable general libraries. | **Self-contained**: This is the project's customized core module. |
| `libcommon/` | General Tool Library | Contains data structures, common constants, general functions, etc. | **Depends on**: Basic system. |
| `libwebserver/` | Web Service Library | Responsible for setting up and managing HTTP/HTTPS servers. | **Depends on**: `libcommon`. |
| `libwebsocketcommon/` | WebSocket Processing Library | Responsible for real-time, bidirectional communication (such as WebSocket) connection and message handling. | **Depends on**: `libcommon`. |
| **`3rd/`** | **Third-Party Dependency Libraries** | Contains external open-source libraries required by the project (such as Boost, libjson, openssl, spdlog). Although you requested to skip analysis, they are the **key external dependencies** for project operation. | **Key dependencies**: Boost, JSON, logging system, encryption protocols. |

---

## 🧩 2. Core Module Dependency Diagram

This project is a layered architecture. Dependencies progress from the bottom layer tool libraries (third-party/local) to upper layer application logic.

### 🟢 External Dependencies Layer (External Dependencies)

This part is the infrastructure shared by all modules:

* `Boost` (full functionality)
* `libjson` (JSON data serialization and deserialization)
* `spdlog` (advanced logging system)
* `openssl` (encryption, network security protocol support)

### 🟠 Local Tool Libraries Layer (Local Libraries)

Located in the `lib/` directory, provides abstraction and encapsulation to the `SrtRouter` business layer, achieving code reuse and decoupling.

* `libcommon`
  * ➡️ **Depends on**: *None (lowest layer)*
* `libwebserver`
  * ➡️ **Depends on**: `libcommon`
* `libwebsocketcommon`
  * ➡️ **Depends on**: `libcommon`

### 🔴 Business Application Layer (Application Core)

This is the module that ultimately executes tasks, connecting all dependencies and tool libraries.

* **`SrtRouter`** (all source code under the entire `SrtRouter` directory)
  * **Dependencies**:
    1. **Local dependencies**: `libcommon`, `libwebserver`, `libwebsocketcommon`
    2. **External dependencies**: `boost`, `libjson`, `spdlog`, `openssl`
  * **Input/Configuration**: Reads `srtrouterconfig.json`, and uses files like `README.md` or `version.properties`.
  * **Output**: Starts a Web/WebSocket service defined by the configuration.

**Dependency Summary Path:**
`{Boost, JSON, OpenSSL, spdlog}` $\rightarrow$ `libcommon` $\rightarrow$ `{libwebserver, libwebsocketcommon}` $\rightarrow$ **`SrtRouter` Application**

---

### 💡 3. System Architecture Conceptual Diagram (Textual Expression)

Based on the CMake build process, the project is a typical **layered three-tier architecture (N-Tier Architecture)** network service.

**System Name: SRT Gateway Router (SrtRouter)**

```mermaid
graph TD
    A[Configuration Input: srtrouterconfig.json] --> B(Application Layer: SrtRouter Core);
    C[Common Components: 3rd/boost, libjson, spdlog, openssl] --> D[Local Tool Libraries Layer];
    D --> E[SrtRouter Core: main.cpp, srtRouter.cpp];
    
    subgraph ⚙️ Core Service Startup (SrtRouter)
        E --> F(Web Server / WebSocket);
        E --> G(SRT Routing Logic);
    end
    
    subgraph 📚 Local Tool Libraries
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

**Process Description:**

1. **Startup Initialization (Bootstrap):** The process starts through `main.cpp`, reads `srtrouterconfig.json` to obtain runtime parameters.
2. **Infrastructure Layer (Infrastructure):** The system uses `openssl` (encryption) and `libjson` (configuration parsing) to initialize network connections.
3. **Service Layer (Service Layer):** The `SrtRouter` core module starts HTTP/Web services through `libwebserver`, and opens real-time bidirectional communication channels through `libwebsocketcommon`.
4. **Business Logic Layer (Business Logic):** The routing module `srtRouter.cpp` is responsible for receiving and distributing SRT stream data packets received via WebSocket or HTTP, calling general functions in `libcommon` for processing, authentication, and forwarding.
5. **Data Flow (Data Flow):** All processing processes revolve around the **receive** $\rightarrow$ **parse/process** $\rightarrow$ **send** cycle of data packets.

**Summary:**

This is a highly dependent on C++ native capabilities, rigorously designed real-time network service that modularly separates network transmission (Web/WS) and business routing (SRT) modules, and builds its complex function set through local tool libraries and numerous external dependencies.