# srt-router

[功能](#功能) |
[技术优势与应用场景](#技术优势与应用场景) |
[使用方法](#使用方法) |
[安装](#安装) |
[配置](#配置) |
[API 接口](#api-接口) |
[许可](#许可) |
[联系方式](#联系方式)

![srt-router](logo.png)
一个用于通过单个 UDP 端口使用标准 Stream ID 格式复用 SRT 呼叫方流的 SRT 路由器。

[English](./README.md) | 简体中文

## 功能

- 透明路由：将流从输入到输出不做修改地转发，确保原始音视频数据和传输参数的完整性。
- 端口复用：在监听模式下通过一个 UDP 端口服务多个 SRT 流，简化网络配置和端口管理。
- 标准输入：接收遵循标准 Stream ID 格式的 SRT 呼叫方流，支持关键参数如 m（模式）和 r（资源标识符）（例如 streamid=#!::m=publish,r=srt1）。
- 标准输出：使用标准 Stream ID 格式发送到目标 SRT 呼叫方流，同样支持 m 和 r 参数（例如 streamid=#!::m=request,r=srt1）。
- 基于路径的服务：根据 Stream ID 中的 r 参数，将不同流分配到独立的逻辑路径进行管理。
- 一对多连接：支持单个对等点同时与多个对等点建立连接以进行流分发。
- 控制接口：提供 HTTP 端口，支持 RESTful API 查询服务器状态并管理配置。
- 热重载：在不断开现有流的情况下重新加载配置，实现无缝的服务更新。
- 指标与日志：支持输出到 stdio 和文件，便于故障排查和系统监控。
- 性能：为低延迟和高吞吐量设计，适用于实时流媒体应用。

## 技术优势与应用场景

该工具旨在发挥 SRT 协议在复杂网络环境中的优势。

SRT 基于 UDP，在公共网络上传输时通过 ARQ（自动重传请求）和 FEC（前向纠错）等机制，确保安全、可靠、低延迟和抗丢包传输。

通过使用端口复用和解析标准 Stream ID，srt-router 进一步简化了大规模 SRT 流聚合和分发节点的部署复杂度。

它适用于将来自互联网的多个 SRT 推流（Caller 模式）汇聚到单一入口点，再根据业务逻辑（通过 r 参数识别）分发到不同后端处理单元或分发网络的场景。

示例包括大型直播平台的内容聚合、多信号路由，或面向边缘计算节点的流媒体网关。

## 使用方法

运行 srt-router，请使用以下命令：

```bash
./srt-router
```

## 安装

安装 srt-router，请按以下步骤操作：

1. 克隆仓库：

   ```bash
    git clone https://github.com/wying71/srt-router.git
   ```

2. 进入项目目录：

   ```bash
    cd srt-router
   ```

3. 构建项目：

   ```bash
    ./build_srtrouter.sh
   ```

4. 运行应用：

   ```bash
    ./srt-router
   ```

## 配置

srt-router 可以通过 JSON 文件进行配置。以下是示例配置：

```json
{
  "srt": {
    "enable": true,
    "listenPort": 8890,
    "encryptions": [
      {
        "path": "*",
        "publishPassphrase": "",
        "requestPassphrase": "",
        "encryptionType": ""
      }
    ]
  },
  "api": {
    "enable": true,
    "listenPort": 3000
  },
  "logging": {
    "logLevel": "info",
    "logFilePath": "logs/srtRouter.log",
    "maxLogSize": 10485760,
    "logKeepDays": 30
  },
  "maxStreamsLimit": 100
}
```

在此配置中：

- `srt.enable`：启用或禁用 SRT 路由功能。
- `srt.listenPort`：srt-router 监听传入 SRT 流的 UDP 端口。
- `srt.encryptions`：针对不同路径的加密配置数组，指定 publish 和 request 的密码短语以及加密类型。
- `api.enable`：启用或禁用 HTTP 控制接口。
- `api.listenPort`：HTTP 控制接口的端口。
- `logging.logFilePath`：日志写入的文件路径。
- `logging.logLevel`：日志级别（例如 "info"、"debug"、"error"）。
- `maxStreamsLimit`：路由器可处理的最大并发流数量。

## API 接口

srt-router 提供以下控制和监控 API 接口：

- `GET /api/health`：检查 srt-router 的健康状态，供监控和告警系统使用。
- `GET /api/status`：获取 srt-router 的当前状态，包括活动流和资源使用情况。
- `GET /api/streams`：获取活动流列表及其详细信息。
- `GET /api/streams/{streamId}`：按流 ID 获取特定流的详细信息。
- `GET /api/config`：检索当前配置，用于验证和审计。
- `POST /api/config`：在不重启服务的情况下更新配置（热重载）。

TODO：添加更多用于流管理、日志检索和性能指标的 API 接口。

- `GET /api/logs`：检索最近日志，便于故障排查和监控。
- `GET /api/metrics`：获取性能指标和统计信息，用于监控和优化。
- `PUT /api/streams/{streamId}`：更新活动流的配置（例如加密设置），无需重启即可根据网络状况或业务需求动态调整。
- `POST /api/config/reload`：触发手动重新加载配置，立即应用更改。
- `GET /api/streams/{streamId}/stats`：检索特定流的实时统计信息，包括比特率、延迟和丢包情况。
- `GET /api/streams/{streamId}/logs`：检索特定流的日志，便于详细故障排查。
- `GET /api/streams/{streamId}/config`：检索特定流的当前配置，用于监控和管理。
- `DELETE /api/streams/{streamId}`：按 ID 终止特定流，允许手动控制活动连接。
- `GET /api/streams/{streamId}/connections`：检索特定流的活动连接列表，包括对等方信息和连接状态。

## 许可

srt-router 使用 MIT 许可证授权。详情请参阅 [LICENSE](LICENSE) 文件。

## 联系方式

如有问题、意见或贡献，请在 GitHub 仓库上提出 issue 或提交 pull request：[https://github.com/wying71/srt-router](https://github.com/wying71/srt-router).
