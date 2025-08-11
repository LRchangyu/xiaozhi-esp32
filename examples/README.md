# 小智ESP32开发示例

本目录包含小智ESP32项目的各种开发示例和模板文件。

## 文件列表

### 1. `custom_board_template.cc`
**功能**: 自定义开发板实现模板，展示如何添加语音控制LED和设备控制功能

**特性**:
- 完整的LED控制实现（RGB、亮度、闪烁、预设模式）
- 串口设备控制（继电器、传感器、电机）
- MCP工具注册示例
- 错误处理和状态管理

**使用方法**:
1. 复制文件到 `main/boards/your-board/` 目录
2. 根据实际硬件修改GPIO引脚定义
3. 在 `main/boards/your-board/CMakeLists.txt` 中添加源文件
4. 配置开发板选择

### 2. `bluetooth_control_example.cc` (计划添加)
**功能**: 蓝牙设备控制示例

### 3. `advanced_led_effects.cc` (计划添加)
**功能**: 高级LED效果实现

## 快速开始

### 创建自定义开发板

1. **创建开发板目录**:
```bash
mkdir main/boards/my-board
cd main/boards/my-board
```

2. **复制模板文件**:
```bash
cp ../../examples/custom_board_template.cc my_board.cc
```

3. **修改硬件配置**:
编辑 `my_board.cc` 中的GPIO定义：
```cpp
#define LED_STRIP_GPIO GPIO_NUM_8    // 修改为实际LED引脚
#define UART_TX_PIN GPIO_NUM_17      // 修改为实际串口TX引脚
#define UART_RX_PIN GPIO_NUM_16      // 修改为实际串口RX引脚
#define RELAY_CONTROL_PIN GPIO_NUM_18 // 修改为实际继电器控制引脚
```

4. **创建CMakeLists.txt**:
```cmake
idf_component_register(
    SRCS "my_board.cc"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_event nvs_flash led_strip
)
```

5. **创建config.h**:
```cpp
#pragma once

#define BOARD_NAME "My Custom Board"
#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_4
#define AUDIO_I2S_GPIO_LRCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7
```

6. **编译和烧录**:
```bash
idf.py set-target esp32s3
idf.py menuconfig  # 选择 "My Custom Board"
idf.py build
idf.py flash
```

## 语音控制示例

### LED控制指令

用户可以通过语音发出以下指令：

- **基础控制**:
  - "打开灯" → 调用 `self.light.turn_on`
  - "关闭灯" → 调用 `self.light.turn_off`
  - "把灯调亮一点" → 调用 `self.light.set_brightness`

- **颜色控制**:
  - "把灯调成红色" → 调用 `self.light.set_rgb` (r=255, g=0, b=0)
  - "设置灯光为绿色" → 调用 `self.light.set_rgb` (r=0, g=255, b=0)

- **模式控制**:
  - "开启睡眠模式" → 调用 `self.light.set_preset` (preset="sleep")
  - "切换到暖光模式" → 调用 `self.light.set_preset` (preset="warm")
  - "开启派对模式" → 调用 `self.light.set_preset` (preset="party")

- **效果控制**:
  - "让灯闪烁三次" → 调用 `self.light.blink` (times=3)

### 设备控制指令

- **继电器控制**:
  - "打开继电器" → 调用 `self.relay.control` (state=true)
  - "关闭继电器" → 调用 `self.relay.control` (state=false)

- **传感器读取**:
  - "读取温度" → 调用 `self.sensor.read_temperature`
  - "查看湿度" → 调用 `self.sensor.read_humidity`

- **电机控制**:
  - "电机向前转100步" → 调用 `self.motor.control` (direction="forward", steps=100)
  - "停止电机" → 调用 `self.motor.control` (direction="stop")

## 技术说明

### MCP工具命名规范

工具名称采用层次化命名：
- `self.模块.功能`: 如 `self.light.turn_on`
- `self.设备.操作`: 如 `self.camera.take_photo`
- `self.子系统.控制`: 如 `self.motor.control`

### 参数类型支持

- `kPropertyTypeBoolean`: 布尔值 (true/false)
- `kPropertyTypeInteger`: 整数，可设置范围
- `kPropertyTypeString`: 字符串

### 返回值格式

- **成功**: 返回 `true` 或具体数据
- **失败**: 返回包含错误信息的JSON字符串

### 错误处理

所有MCP工具都应该包含适当的错误处理：

```cpp
mcp_server.AddTool("self.device.control", 
    "设备控制", 
    PropertyList({
        Property("action", kPropertyTypeString)
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        try {
            std::string action = properties["action"].value<std::string>();
            
            if (!IsDeviceReady()) {
                return "{\"error\": \"Device not ready\"}";
            }
            
            bool result = ExecuteAction(action);
            return result ? true : "{\"error\": \"Action failed\"}";
            
        } catch (const std::exception& e) {
            return "{\"error\": \"Exception: " + std::string(e.what()) + "\"}";
        }
    });
```

## 注意事项

1. **内存管理**: 确保动态分配的内存得到正确释放
2. **线程安全**: 在多线程环境中使用互斥锁保护共享资源
3. **错误处理**: 为所有可能失败的操作添加错误处理
4. **性能优化**: 避免在MCP回调中执行耗时操作
5. **硬件兼容**: 确保GPIO引脚配置与实际硬件匹配

## 扩展开发

### 添加新的MCP工具

1. 在 `RegisterMcpTools()` 函数中添加新工具
2. 实现对应的硬件控制函数
3. 测试语音控制功能
4. 更新设备状态

### 集成新硬件

1. 在初始化函数中添加硬件配置
2. 实现硬件控制接口
3. 注册相应的MCP工具
4. 测试功能完整性

通过以上示例和指南，开发者可以快速为小智ESP32项目添加自定义的语音控制功能。