# 小智ESP32项目中的MCP角色梳理

## MCP (Model Context Protocol) 概述

MCP是小智ESP32项目的核心控制协议，基于JSON-RPC 2.0标准，实现了云端AI与硬件设备之间的标准化通信。

## MCP角色分析

### 1. MCP服务器角色 (ESP32设备端)

**角色定义**: ESP32设备作为MCP服务器，向云端暴露设备能力

**主要职责**:
- 注册设备工具(Tools)和能力
- 接收并处理来自云端的工具调用请求
- 执行具体的硬件操作
- 返回执行结果给云端

**核心实现**: `main/mcp_server.cc`

**关键类**: `McpServer`

```cpp
class McpServer {
public:
    // 单例模式获取实例
    static McpServer& GetInstance();
    
    // 添加工具到MCP服务器
    void AddTool(const std::string& name, 
                 const std::string& description, 
                 const PropertyList& properties, 
                 std::function<ReturnValue(const PropertyList&)> callback);
    
    // 处理MCP消息
    bool HandleMcpMessage(const std::string& message);
    
    // 初始化通用工具
    void AddCommonTools();
};
```

### 2. MCP客户端角色 (云端后台)

**角色定义**: 云端后台服务作为MCP客户端，调用设备能力

**主要职责**:
- 发现设备支持的工具列表
- 基于AI理解生成工具调用请求
- 管理与设备的MCP会话
- 处理设备返回的执行结果

**通信流程**:
1. 发送`initialize`请求建立MCP会话
2. 调用`tools/list`获取设备工具列表
3. 通过`tools/call`调用具体工具
4. 接收设备执行结果

## 已注册的MCP工具分析

### 1. 设备状态查询工具

**工具名称**: `self.get_device_status`

**功能描述**: 获取设备实时状态信息

**参数**: 无

**返回值**: JSON格式的设备状态信息

**使用场景**:
- 查询当前音量、屏幕亮度等
- 作为其他控制操作的前置步骤

```cpp
AddTool("self.get_device_status",
    "Provides the real-time information of the device...",
    PropertyList(),
    [&board](const PropertyList& properties) -> ReturnValue {
        return board.GetDeviceStatusJson();
    });
```

### 2. 音频控制工具

**工具名称**: `self.audio_speaker.set_volume`

**功能描述**: 设置音响音量

**参数**: 
- `volume` (integer, 0-100): 音量大小

**返回值**: 布尔值表示是否成功

**使用场景**: 语音控制音量调节

```cpp
AddTool("self.audio_speaker.set_volume", 
    "Set the volume of the audio speaker...",
    PropertyList({
        Property("volume", kPropertyTypeInteger, 0, 100)
    }), 
    [&board](const PropertyList& properties) -> ReturnValue {
        auto codec = board.GetAudioCodec();
        codec->SetOutputVolume(properties["volume"].value<int>());
        return true;
    });
```

### 3. 屏幕控制工具

#### 亮度控制
**工具名称**: `self.screen.set_brightness`

**功能描述**: 设置屏幕亮度

**参数**:
- `brightness` (integer, 0-100): 亮度百分比

**使用场景**: 根据环境光线调节屏幕亮度

#### 主题切换
**工具名称**: `self.screen.set_theme`

**功能描述**: 切换屏幕主题

**参数**:
- `theme` (string): 主题名称 ("light" 或 "dark")

**使用场景**: 切换日间/夜间模式

### 4. 摄像头工具

**工具名称**: `self.camera.take_photo`

**功能描述**: 拍照并进行AI分析

**参数**:
- `question` (string): 对照片的提问

**返回值**: 包含照片分析结果的JSON对象

**使用场景**: 视觉问答、环境感知

```cpp
AddTool("self.camera.take_photo",
    "Take a photo and explain it...",
    PropertyList({
        Property("question", kPropertyTypeString)
    }),
    [camera](const PropertyList& properties) -> ReturnValue {
        if (!camera->Capture()) {
            return "{\"success\": false, \"message\": \"Failed to capture photo\"}";
        }
        // ... 处理照片和问题
    });
```

## MCP工具扩展机制

### 工具注册流程

1. **定义工具功能**: 确定工具的名称、描述和参数
2. **实现回调函数**: 编写具体的硬件操作逻辑
3. **注册到MCP服务器**: 调用`AddTool`方法注册

### 工具命名规范

采用层次化命名方式:
- `self.模块.功能`: 如`self.audio_speaker.set_volume`
- `self.设备.操作`: 如`self.camera.take_photo`
- `self.子系统.控制`: 如`self.screen.set_brightness`

### 参数类型支持

MCP工具支持以下参数类型:
- `kPropertyTypeBoolean`: 布尔值
- `kPropertyTypeInteger`: 整数（可设置范围）
- `kPropertyTypeString`: 字符串

### 返回值类型

工具可以返回:
- `bool`: 简单的成功/失败状态
- `int`: 数值结果
- `std::string`: 复杂的JSON格式数据

## MCP消息交互流程

### 1. 会话初始化

```json
{
  "jsonrpc": "2.0",
  "method": "initialize",
  "params": {
    "capabilities": {
      "vision": {
        "url": "http://example.com/vision",
        "token": "token123"
      }
    }
  },
  "id": 1
}
```

### 2. 工具列表查询

```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": {
    "cursor": ""
  },
  "id": 2
}
```

### 3. 工具调用示例

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.audio_speaker.set_volume",
    "arguments": {
      "volume": 80
    }
  },
  "id": 3
}
```

## 不同硬件平台的MCP角色差异

### 标准功能集

所有平台都支持的基础MCP工具:
- 设备状态查询
- 音频音量控制

### 平台特定功能

根据硬件配置，某些工具可能不可用:

**有屏幕的设备**:
- `self.screen.set_brightness`
- `self.screen.set_theme`

**有摄像头的设备**:
- `self.camera.take_photo`

**有LED的设备**:
- 各种LED控制工具（需要自定义实现）

## MCP协议优势

### 1. 标准化
- 基于JSON-RPC 2.0标准
- 统一的工具发现和调用机制
- 良好的互操作性

### 2. 可扩展性
- 插件化的工具注册机制
- 支持动态工具发现
- 便于添加新功能

### 3. 类型安全
- 明确的参数类型定义
- 参数范围验证
- 错误处理机制

### 4. 异步支持
- 非阻塞的工具调用
- 支持并发请求处理
- 实时状态通知

## 开发建议

### 1. 工具设计原则
- 功能单一且明确
- 参数简洁易懂
- 错误处理完整

### 2. 性能考虑
- 避免耗时操作阻塞MCP处理
- 合理使用异步执行
- 控制内存使用

### 3. 兼容性
- 考虑不同硬件平台的差异
- 提供优雅的降级机制
- 保持向后兼容

通过MCP协议，小智ESP32实现了AI大模型与硬件设备之间的无缝集成，为智能设备控制提供了标准化、可扩展的解决方案。