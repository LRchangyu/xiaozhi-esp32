# 语音控制ESP32灯光和设备控制实现指南

## 概述

本文档详细说明如何基于小智ESP32项目实现语音控制LED灯光开关，以及通过串口或蓝牙控制其他设备的方法。

## 一、语音控制LED灯光实现

### 1.1 基础LED控制工具实现

基于项目中已有的LED控制系统，我们可以快速实现语音控制灯光功能。

#### 步骤1：在自定义开发板中添加LED控制工具

在你的开发板实现文件中（如 `main/boards/your-board/your_board.cc`），添加以下MCP工具：

```cpp
void YourBoard::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();
    
    // 1. 基础开关控制
    mcp_server.AddTool("self.light.turn_on", 
        "打开灯光", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            TurnOnLed();
            return true;
        });
    
    mcp_server.AddTool("self.light.turn_off", 
        "关闭灯光", 
        PropertyList(), 
        [this](const PropertyList& properties) -> ReturnValue {
            TurnOffLed();
            return true;
        });
    
    // 2. RGB颜色控制
    mcp_server.AddTool("self.light.set_rgb", 
        "设置RGB颜色", 
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["r"].value<int>();
            int g = properties["g"].value<int>();
            int b = properties["b"].value<int>();
            SetLedColor(r, g, b);
            return true;
        });
    
    // 3. 亮度控制
    mcp_server.AddTool("self.light.set_brightness", 
        "设置灯光亮度", 
        PropertyList({
            Property("brightness", kPropertyTypeInteger, 0, 100)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            int brightness = properties["brightness"].value<int>();
            SetLedBrightness(brightness);
            return true;
        });
    
    // 4. 灯光效果
    mcp_server.AddTool("self.light.blink", 
        "闪烁灯光", 
        PropertyList({
            Property("color", kPropertyTypeString),
            Property("times", kPropertyTypeInteger, 1, 10),
            Property("interval", kPropertyTypeInteger, 100, 2000)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            std::string color = properties["color"].value<std::string>();
            int times = properties["times"].value<int>();
            int interval = properties["interval"].value<int>();
            BlinkLed(color, times, interval);
            return true;
        });
}
```

#### 步骤2：实现LED控制函数

在开发板类中实现具体的LED控制函数：

```cpp
class YourBoard : public WifiBoard {
private:
    led_strip_handle_t led_strip_;
    bool led_on_ = false;
    uint8_t current_brightness_ = 255;
    
public:
    void TurnOnLed() {
        led_on_ = true;
        SetLedColor(255, 255, 255); // 默认白色
    }
    
    void TurnOffLed() {
        led_on_ = false;
        SetLedColor(0, 0, 0);
    }
    
    void SetLedColor(uint8_t r, uint8_t g, uint8_t b) {
        if (led_strip_) {
            led_strip_set_pixel(led_strip_, 0, r, g, b);
            led_strip_refresh(led_strip_);
            led_on_ = (r > 0 || g > 0 || b > 0);
        }
    }
    
    void SetLedBrightness(uint8_t brightness) {
        current_brightness_ = brightness;
        // 重新应用当前颜色，但使用新亮度
        if (led_on_) {
            uint8_t r = (current_color_r_ * brightness) / 100;
            uint8_t g = (current_color_g_ * brightness) / 100;
            uint8_t b = (current_color_b_ * brightness) / 100;
            SetLedColor(r, g, b);
        }
    }
    
    void BlinkLed(const std::string& color, int times, int interval) {
        // 解析颜色字符串
        uint8_t r = 255, g = 255, b = 255; // 默认白色
        if (color == "red") { r = 255; g = 0; b = 0; }
        else if (color == "green") { r = 0; g = 255; b = 0; }
        else if (color == "blue") { r = 0; g = 0; b = 255; }
        
        // 创建闪烁任务
        xTaskCreate([](void* param) {
            auto* data = static_cast<BlinkData*>(param);
            for (int i = 0; i < data->times; i++) {
                data->board->SetLedColor(data->r, data->g, data->b);
                vTaskDelay(data->interval / portTICK_PERIOD_MS);
                data->board->SetLedColor(0, 0, 0);
                vTaskDelay(data->interval / portTICK_PERIOD_MS);
            }
            delete data;
            vTaskDelete(NULL);
        }, "blink_task", 2048, new BlinkData{this, r, g, b, times, interval}, 5, NULL);
    }
};
```

### 1.2 高级LED控制功能

#### 预设颜色模式

```cpp
mcp_server.AddTool("self.light.set_mode", 
    "设置灯光模式", 
    PropertyList({
        Property("mode", kPropertyTypeString) // warm, cool, party, sleep
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        std::string mode = properties["mode"].value<std::string>();
        if (mode == "warm") {
            SetLedColor(255, 147, 41); // 暖白色
        } else if (mode == "cool") {
            SetLedColor(255, 255, 255); // 冷白色
        } else if (mode == "party") {
            StartPartyMode(); // 彩虹灯效
        } else if (mode == "sleep") {
            SetLedColor(255, 0, 0); // 红色夜灯
            SetLedBrightness(10);
        }
        return true;
    });
```

#### 渐变效果

```cpp
mcp_server.AddTool("self.light.fade", 
    "灯光渐变效果", 
    PropertyList({
        Property("from_r", kPropertyTypeInteger, 0, 255),
        Property("from_g", kPropertyTypeInteger, 0, 255),
        Property("from_b", kPropertyTypeInteger, 0, 255),
        Property("to_r", kPropertyTypeInteger, 0, 255),
        Property("to_g", kPropertyTypeInteger, 0, 255),
        Property("to_b", kPropertyTypeInteger, 0, 255),
        Property("duration", kPropertyTypeInteger, 1000, 10000)
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        FadeParams params;
        params.from_r = properties["from_r"].value<int>();
        params.from_g = properties["from_g"].value<int>();
        params.from_b = properties["from_b"].value<int>();
        params.to_r = properties["to_r"].value<int>();
        params.to_g = properties["to_g"].value<int>();
        params.to_b = properties["to_b"].value<int>();
        params.duration = properties["duration"].value<int>();
        StartFadeEffect(params);
        return true;
    });
```

## 二、串口设备控制实现

### 2.1 串口通信基础设置

#### 步骤1：初始化UART

```cpp
#include <driver/uart.h>

class YourBoard : public WifiBoard {
private:
    static const int UART_NUM = UART_NUM_1;
    static const int TX_PIN = GPIO_NUM_17;
    static const int RX_PIN = GPIO_NUM_16;
    static const int BUFFER_SIZE = 1024;
    
public:
    esp_err_t InitUart() {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
        };
        
        ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, 
                                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUFFER_SIZE, 
                                           BUFFER_SIZE, 0, NULL, 0));
        return ESP_OK;
    }
};
```

#### 步骤2：添加串口控制MCP工具

```cpp
void YourBoard::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();
    
    // 发送串口命令
    mcp_server.AddTool("self.serial.send_command", 
        "通过串口发送命令到外部设备", 
        PropertyList({
            Property("command", kPropertyTypeString)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            std::string command = properties["command"].value<std::string>();
            SendUartCommand(command);
            return true;
        });
    
    // 控制外部继电器
    mcp_server.AddTool("self.relay.control", 
        "控制继电器开关", 
        PropertyList({
            Property("relay_id", kPropertyTypeInteger, 1, 8),
            Property("state", kPropertyTypeBoolean)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            int relay_id = properties["relay_id"].value<int>();
            bool state = properties["state"].value<bool>();
            std::string command = "RELAY" + std::to_string(relay_id) + 
                                (state ? ":ON" : ":OFF") + "\n";
            SendUartCommand(command);
            return true;
        });
    
    // 读取传感器数据
    mcp_server.AddTool("self.sensor.read_data", 
        "读取传感器数据", 
        PropertyList({
            Property("sensor_type", kPropertyTypeString) // temperature, humidity, distance
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            std::string sensor_type = properties["sensor_type"].value<std::string>();
            std::string command = "READ:" + sensor_type + "\n";
            std::string response = SendUartCommandWithResponse(command);
            return response;
        });
    
    // 控制步进电机
    mcp_server.AddTool("self.motor.move", 
        "控制步进电机移动", 
        PropertyList({
            Property("motor_id", kPropertyTypeInteger, 1, 4),
            Property("steps", kPropertyTypeInteger, -1000, 1000),
            Property("speed", kPropertyTypeInteger, 1, 100)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            int motor_id = properties["motor_id"].value<int>();
            int steps = properties["steps"].value<int>();
            int speed = properties["speed"].value<int>();
            
            std::string command = "MOTOR" + std::to_string(motor_id) + 
                                ":MOVE:" + std::to_string(steps) + 
                                ":SPEED:" + std::to_string(speed) + "\n";
            SendUartCommand(command);
            return true;
        });
}
```

#### 步骤3：实现串口通信函数

```cpp
void YourBoard::SendUartCommand(const std::string& command) {
    ESP_LOGI(TAG, "Sending UART command: %s", command.c_str());
    uart_write_bytes(UART_NUM, command.c_str(), command.length());
}

std::string YourBoard::SendUartCommandWithResponse(const std::string& command, int timeout_ms = 1000) {
    // 清空接收缓冲区
    uart_flush_input(UART_NUM);
    
    // 发送命令
    SendUartCommand(command);
    
    // 等待响应
    char buffer[BUFFER_SIZE];
    int len = uart_read_bytes(UART_NUM, buffer, BUFFER_SIZE - 1, 
                             timeout_ms / portTICK_PERIOD_MS);
    
    if (len > 0) {
        buffer[len] = '\0';
        ESP_LOGI(TAG, "UART response: %s", buffer);
        return std::string(buffer);
    } else {
        ESP_LOGW(TAG, "UART timeout or no response");
        return "{\"error\": \"No response from device\"}";
    }
}
```

## 三、蓝牙设备控制实现

### 3.1 蓝牙经典(SPP)控制

#### 步骤1：初始化蓝牙SPP

```cpp
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_spp_api.h"

class YourBoard : public WifiBoard {
private:
    static uint32_t spp_handle_;
    static bool spp_connected_;
    
    static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        switch (event) {
            case ESP_SPP_INIT_EVT:
                ESP_LOGI(TAG, "ESP_SPP_INIT_EVT");
                esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, "ESP32_SPP");
                break;
            case ESP_SPP_START_EVT:
                ESP_LOGI(TAG, "ESP_SPP_START_EVT");
                break;
            case ESP_SPP_SRV_OPEN_EVT:
                ESP_LOGI(TAG, "ESP_SPP_SRV_OPEN_EVT");
                spp_handle_ = param->srv_open.handle;
                spp_connected_ = true;
                break;
            case ESP_SPP_CLOSE_EVT:
                ESP_LOGI(TAG, "ESP_SPP_CLOSE_EVT");
                spp_connected_ = false;
                break;
            case ESP_SPP_DATA_IND_EVT:
                ESP_LOGI(TAG, "ESP_SPP_DATA_IND_EVT len=%d", param->data_ind.len);
                // 处理接收到的蓝牙数据
                break;
            default:
                break;
        }
    }
    
public:
    esp_err_t InitBluetooth() {
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
        ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
        ESP_ERROR_CHECK(esp_bluedroid_init());
        ESP_ERROR_CHECK(esp_bluedroid_enable());
        ESP_ERROR_CHECK(esp_spp_register_callback(esp_spp_cb));
        ESP_ERROR_CHECK(esp_spp_init(ESP_SPP_MODE_CB));
        return ESP_OK;
    }
};
```

#### 步骤2：添加蓝牙控制MCP工具

```cpp
void YourBoard::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();
    
    // 发送蓝牙命令
    mcp_server.AddTool("self.bluetooth.send_command", 
        "通过蓝牙发送命令", 
        PropertyList({
            Property("command", kPropertyTypeString)
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            if (!spp_connected_) {
                return "{\"error\": \"Bluetooth not connected\"}";
            }
            std::string command = properties["command"].value<std::string>();
            SendBluetoothCommand(command);
            return true;
        });
    
    // 控制蓝牙设备开关
    mcp_server.AddTool("self.bluetooth.device_control", 
        "控制蓝牙设备", 
        PropertyList({
            Property("device_type", kPropertyTypeString), // fan, light, speaker
            Property("action", kPropertyTypeString)       // on, off, toggle
        }), 
        [this](const PropertyList& properties) -> ReturnValue {
            if (!spp_connected_) {
                return "{\"error\": \"Bluetooth not connected\"}";
            }
            
            std::string device_type = properties["device_type"].value<std::string>();
            std::string action = properties["action"].value<std::string>();
            
            std::string command = device_type + ":" + action + "\n";
            SendBluetoothCommand(command);
            return true;
        });
}
```

### 3.2 蓝牙低功耗(BLE)控制

#### 步骤1：BLE设备扫描和连接

```cpp
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"

mcp_server.AddTool("self.ble.scan_devices", 
    "扫描附近的BLE设备", 
    PropertyList({
        Property("duration", kPropertyTypeInteger, 5, 30)
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        int duration = properties["duration"].value<int>();
        return StartBleScan(duration);
    });

mcp_server.AddTool("self.ble.connect_device", 
    "连接到BLE设备", 
    PropertyList({
        Property("device_address", kPropertyTypeString)
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        std::string address = properties["device_address"].value<std::string>();
        return ConnectBleDevice(address);
    });
```

## 四、实际应用示例

### 4.1 智能家居控制

```cpp
// 语音指令："打开客厅灯"
// LLM生成的MCP调用：
{
  "method": "tools/call",
  "params": {
    "name": "self.light.turn_on",
    "arguments": {}
  }
}

// 语音指令："把灯调成红色"
// LLM生成的MCP调用：
{
  "method": "tools/call",
  "params": {
    "name": "self.light.set_rgb",
    "arguments": {
      "r": 255,
      "g": 0,
      "b": 0
    }
  }
}
```

### 4.2 外部设备控制

```cpp
// 语音指令："打开第一个继电器"
// LLM生成的MCP调用：
{
  "method": "tools/call",
  "params": {
    "name": "self.relay.control",
    "arguments": {
      "relay_id": 1,
      "state": true
    }
  }
}

// 语音指令："读取温度传感器"
// LLM生成的MCP调用：
{
  "method": "tools/call",
  "params": {
    "name": "self.sensor.read_data",
    "arguments": {
      "sensor_type": "temperature"
    }
  }
}
```

## 五、最佳实践

### 5.1 错误处理

```cpp
mcp_server.AddTool("self.device.safe_control", 
    "安全的设备控制", 
    PropertyList({
        Property("action", kPropertyTypeString)
    }), 
    [this](const PropertyList& properties) -> ReturnValue {
        try {
            std::string action = properties["action"].value<std::string>();
            
            // 检查设备状态
            if (!IsDeviceReady()) {
                return "{\"error\": \"Device not ready\", \"code\": 1001}";
            }
            
            // 执行控制操作
            bool result = ExecuteAction(action);
            
            if (result) {
                return "{\"success\": true, \"message\": \"Action completed\"}";
            } else {
                return "{\"error\": \"Action failed\", \"code\": 1002}";
            }
            
        } catch (const std::exception& e) {
            return "{\"error\": \"Exception: " + std::string(e.what()) + "\"}";
        }
    });
```

### 5.2 状态管理

```cpp
class DeviceStateManager {
private:
    std::map<std::string, bool> device_states_;
    std::mutex state_mutex_;
    
public:
    void UpdateDeviceState(const std::string& device, bool state) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        device_states_[device] = state;
        SaveStateToNVS(); // 持久化状态
    }
    
    bool GetDeviceState(const std::string& device) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return device_states_[device];
    }
    
    std::string GetAllStatesJson() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        cJSON* json = cJSON_CreateObject();
        for (const auto& pair : device_states_) {
            cJSON_AddBoolToObject(json, pair.first.c_str(), pair.second);
        }
        char* json_string = cJSON_Print(json);
        std::string result(json_string);
        free(json_string);
        cJSON_Delete(json);
        return result;
    }
};
```

### 5.3 性能优化

1. **异步执行**: 对于耗时操作，使用FreeRTOS任务异步执行
2. **缓存状态**: 缓存设备状态，避免重复查询
3. **超时控制**: 为所有通信操作设置合理超时
4. **内存管理**: 及时释放动态分配的内存

通过以上实现方式，你可以快速为小智ESP32项目添加语音控制LED灯光和外部设备控制功能。这些MCP工具可以被云端大语言模型自动发现和调用，实现真正的语音智能控制。