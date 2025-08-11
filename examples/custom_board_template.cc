// 自定义开发板实现模板 - 语音控制LED和设备控制
// Custom Board Implementation Template - Voice-Controlled LED and Device Control

#include "wifi_board.h"
#include "mcp_server.h"
#include "application.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <led_strip.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>

#define TAG "CUSTOM_BOARD"

// GPIO引脚定义 - 根据你的硬件修改
#define LED_STRIP_GPIO GPIO_NUM_8
#define UART_TX_PIN GPIO_NUM_17
#define UART_RX_PIN GPIO_NUM_16
#define RELAY_CONTROL_PIN GPIO_NUM_18

// LED灯带配置
static const led_strip_config_t led_config = {
    .strip_gpio_num = LED_STRIP_GPIO,
    .max_leds = 4,  // 根据实际LED数量修改
    .led_model = LED_MODEL_WS2812,
    .flags = {
        .invert_out = false
    }
};

static const led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000,
    .flags = {
        .with_dma = false
    }
};

class CustomBoard : public WifiBoard {
private:
    // LED控制相关
    led_strip_handle_t led_strip_ = nullptr;
    bool led_on_ = false;
    uint8_t current_r_ = 255, current_g_ = 255, current_b_ = 255;
    uint8_t brightness_ = 100;
    
    // 串口控制相关
    static const int UART_NUM = UART_NUM_1;
    static const int UART_BUFFER_SIZE = 1024;
    
    // 设备状态管理
    std::map<std::string, bool> device_states_;
    
public:
    CustomBoard() {
        InitializeLed();
        InitializeUart();
        InitializeGpios();
        RegisterMcpTools();
    }
    
    ~CustomBoard() {
        if (led_strip_) {
            led_strip_del(led_strip_);
        }
    }

private:
    // ========== 初始化函数 ==========
    
    void InitializeLed() {
        ESP_LOGI(TAG, "Initializing LED strip on GPIO %d", LED_STRIP_GPIO);
        esp_err_t ret = led_strip_new_rmt_device(&led_config, &rmt_config, &led_strip_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
            return;
        }
        
        // 初始化时关闭所有LED
        led_strip_clear(led_strip_);
        ESP_LOGI(TAG, "LED strip initialized successfully");
    }
    
    void InitializeUart() {
        ESP_LOGI(TAG, "Initializing UART on TX:%d, RX:%d", UART_TX_PIN, UART_RX_PIN);
        
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
        };
        
        ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, 
                                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUFFER_SIZE, 
                                           UART_BUFFER_SIZE, 0, NULL, 0));
        ESP_LOGI(TAG, "UART initialized successfully");
    }
    
    void InitializeGpios() {
        // 配置继电器控制引脚
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << RELAY_CONTROL_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(RELAY_CONTROL_PIN, 0); // 初始状态关闭
        
        ESP_LOGI(TAG, "GPIO pins initialized");
    }

    // ========== LED控制函数 ==========
    
    void SetLedColor(uint8_t r, uint8_t g, uint8_t b) {
        if (!led_strip_) return;
        
        // 应用亮度调整
        uint8_t adj_r = (r * brightness_) / 100;
        uint8_t adj_g = (g * brightness_) / 100;
        uint8_t adj_b = (b * brightness_) / 100;
        
        // 设置所有LED为相同颜色
        for (int i = 0; i < led_config.max_leds; i++) {
            led_strip_set_pixel(led_strip_, i, adj_r, adj_g, adj_b);
        }
        led_strip_refresh(led_strip_);
        
        // 保存当前颜色
        current_r_ = r;
        current_g_ = g;
        current_b_ = b;
        
        ESP_LOGI(TAG, "LED color set to R:%d G:%d B:%d (brightness: %d%%)", r, g, b, brightness_);
    }
    
    void SetLedBrightness(uint8_t brightness) {
        brightness_ = std::min(static_cast<uint8_t>(100), brightness);
        
        // 重新应用当前颜色以反映新亮度
        if (led_on_) {
            SetLedColor(current_r_, current_g_, current_b_);
        }
        
        ESP_LOGI(TAG, "LED brightness set to %d%%", brightness_);
    }
    
    void BlinkLed(uint8_t r, uint8_t g, uint8_t b, int times, int interval_ms) {
        // 创建闪烁任务
        struct BlinkParams {
            CustomBoard* board;
            uint8_t r, g, b;
            int times;
            int interval_ms;
            uint8_t orig_r, orig_g, orig_b;
            bool orig_state;
        };
        
        BlinkParams* params = new BlinkParams{
            this, r, g, b, times, interval_ms, 
            current_r_, current_g_, current_b_, led_on_
        };
        
        xTaskCreate([](void* param) {
            BlinkParams* p = static_cast<BlinkParams*>(param);
            
            for (int i = 0; i < p->times; i++) {
                // 点亮
                p->board->SetLedColor(p->r, p->g, p->b);
                vTaskDelay(p->interval_ms / portTICK_PERIOD_MS);
                
                // 熄灭
                p->board->SetLedColor(0, 0, 0);
                vTaskDelay(p->interval_ms / portTICK_PERIOD_MS);
            }
            
            // 恢复原始状态
            if (p->orig_state) {
                p->board->SetLedColor(p->orig_r, p->orig_g, p->orig_b);
                p->board->led_on_ = true;
            }
            
            delete p;
            vTaskDelete(NULL);
        }, "blink_task", 2048, params, 5, NULL);
    }

    // ========== 串口通信函数 ==========
    
    void SendUartCommand(const std::string& command) {
        ESP_LOGI(TAG, "Sending UART command: %s", command.c_str());
        uart_write_bytes(UART_NUM, command.c_str(), command.length());
    }
    
    std::string SendUartCommandWithResponse(const std::string& command, int timeout_ms = 1000) {
        // 清空接收缓冲区
        uart_flush_input(UART_NUM);
        
        // 发送命令
        SendUartCommand(command);
        
        // 等待响应
        char buffer[UART_BUFFER_SIZE];
        int len = uart_read_bytes(UART_NUM, buffer, UART_BUFFER_SIZE - 1, 
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

    // ========== 设备状态管理 ==========
    
    void UpdateDeviceState(const std::string& device, bool state) {
        device_states_[device] = state;
        ESP_LOGI(TAG, "Device '%s' state updated to %s", device.c_str(), state ? "ON" : "OFF");
    }
    
    std::string GetDeviceStatusJson() {
        cJSON* json = cJSON_CreateObject();
        
        // LED状态
        cJSON* led_obj = cJSON_CreateObject();
        cJSON_AddBoolToObject(led_obj, "power", led_on_);
        cJSON_AddNumberToObject(led_obj, "brightness", brightness_);
        cJSON_AddNumberToObject(led_obj, "red", current_r_);
        cJSON_AddNumberToObject(led_obj, "green", current_g_);
        cJSON_AddNumberToObject(led_obj, "blue", current_b_);
        cJSON_AddItemToObject(json, "led", led_obj);
        
        // 外部设备状态
        cJSON* devices_obj = cJSON_CreateObject();
        for (const auto& pair : device_states_) {
            cJSON_AddBoolToObject(devices_obj, pair.first.c_str(), pair.second);
        }
        cJSON_AddItemToObject(json, "external_devices", devices_obj);
        
        char* json_string = cJSON_Print(json);
        std::string result(json_string);
        free(json_string);
        cJSON_Delete(json);
        
        return result;
    }

    // ========== MCP工具注册 ==========
    
    void RegisterMcpTools() {
        auto& mcp_server = McpServer::GetInstance();
        
        // ===== LED控制工具 =====
        
        mcp_server.AddTool("self.light.get_status", 
            "获取LED灯光状态", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                return GetDeviceStatusJson();
            });
        
        mcp_server.AddTool("self.light.turn_on", 
            "打开LED灯光", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                SetLedColor(255, 255, 255); // 默认白色
                led_on_ = true;
                return true;
            });
        
        mcp_server.AddTool("self.light.turn_off", 
            "关闭LED灯光", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                SetLedColor(0, 0, 0);
                led_on_ = false;
                return true;
            });
        
        mcp_server.AddTool("self.light.set_rgb", 
            "设置LED RGB颜色", 
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
                led_on_ = (r > 0 || g > 0 || b > 0);
                return true;
            });
        
        mcp_server.AddTool("self.light.set_brightness", 
            "设置LED亮度", 
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 0, 100)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                int brightness = properties["brightness"].value<int>();
                SetLedBrightness(brightness);
                return true;
            });
        
        mcp_server.AddTool("self.light.blink", 
            "LED闪烁效果", 
            PropertyList({
                Property("r", kPropertyTypeInteger, 0, 255),
                Property("g", kPropertyTypeInteger, 0, 255),
                Property("b", kPropertyTypeInteger, 0, 255),
                Property("times", kPropertyTypeInteger, 1, 10),
                Property("interval", kPropertyTypeInteger, 100, 2000)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                int r = properties["r"].value<int>();
                int g = properties["g"].value<int>();
                int b = properties["b"].value<int>();
                int times = properties["times"].value<int>();
                int interval = properties["interval"].value<int>();
                BlinkLed(r, g, b, times, interval);
                return true;
            });
        
        mcp_server.AddTool("self.light.set_preset", 
            "设置预设灯光模式", 
            PropertyList({
                Property("preset", kPropertyTypeString) // warm, cool, party, sleep, rainbow
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string preset = properties["preset"].value<std::string>();
                
                if (preset == "warm") {
                    SetLedColor(255, 147, 41); // 暖白色
                } else if (preset == "cool") {
                    SetLedColor(255, 255, 255); // 冷白色
                } else if (preset == "party") {
                    // 彩虹效果 - 循环显示不同颜色
                    BlinkLed(255, 0, 0, 2, 300);   // 红色
                    vTaskDelay(600 / portTICK_PERIOD_MS);
                    BlinkLed(0, 255, 0, 2, 300);   // 绿色
                    vTaskDelay(600 / portTICK_PERIOD_MS);
                    BlinkLed(0, 0, 255, 2, 300);   // 蓝色
                } else if (preset == "sleep") {
                    SetLedColor(255, 50, 0); // 暖红色
                    SetLedBrightness(10);    // 低亮度
                } else if (preset == "rainbow") {
                    // 创建彩虹渐变任务
                    xTaskCreate([](void* param) {
                        CustomBoard* board = static_cast<CustomBoard*>(param);
                        for (int i = 0; i < 360; i += 10) {
                            // HSV to RGB转换简化版
                            int r, g, b;
                            if (i < 120) {
                                r = 255 - (i * 255 / 120);
                                g = i * 255 / 120;
                                b = 0;
                            } else if (i < 240) {
                                r = 0;
                                g = 255 - ((i - 120) * 255 / 120);
                                b = (i - 120) * 255 / 120;
                            } else {
                                r = (i - 240) * 255 / 120;
                                g = 0;
                                b = 255 - ((i - 240) * 255 / 120);
                            }
                            board->SetLedColor(r, g, b);
                            vTaskDelay(100 / portTICK_PERIOD_MS);
                        }
                        vTaskDelete(NULL);
                    }, "rainbow_task", 2048, this, 5, NULL);
                } else {
                    return "{\"error\": \"Unknown preset: " + preset + "\"}";
                }
                
                led_on_ = true;
                return true;
            });

        // ===== 串口设备控制工具 =====
        
        mcp_server.AddTool("self.serial.send_command", 
            "通过串口发送命令", 
            PropertyList({
                Property("command", kPropertyTypeString)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string command = properties["command"].value<std::string>();
                SendUartCommand(command + "\n");
                return true;
            });
        
        mcp_server.AddTool("self.relay.control", 
            "控制继电器开关", 
            PropertyList({
                Property("state", kPropertyTypeBoolean)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                bool state = properties["state"].value<bool>();
                gpio_set_level(RELAY_CONTROL_PIN, state ? 1 : 0);
                UpdateDeviceState("relay", state);
                
                // 可选：通过串口通知外部设备
                std::string command = state ? "RELAY:ON" : "RELAY:OFF";
                SendUartCommand(command + "\n");
                
                return true;
            });
        
        mcp_server.AddTool("self.sensor.read_temperature", 
            "读取温度传感器", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string response = SendUartCommandWithResponse("READ:TEMP\n");
                return response;
            });
        
        mcp_server.AddTool("self.sensor.read_humidity", 
            "读取湿度传感器", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string response = SendUartCommandWithResponse("READ:HUMIDITY\n");
                return response;
            });
        
        mcp_server.AddTool("self.motor.control", 
            "控制步进电机", 
            PropertyList({
                Property("direction", kPropertyTypeString), // forward, backward, stop
                Property("steps", kPropertyTypeInteger, 0, 1000),
                Property("speed", kPropertyTypeInteger, 1, 100)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string direction = properties["direction"].value<std::string>();
                int steps = properties["steps"].value<int>();
                int speed = properties["speed"].value<int>();
                
                std::string command = "MOTOR:" + direction + ":" + 
                                    std::to_string(steps) + ":" + 
                                    std::to_string(speed) + "\n";
                SendUartCommand(command);
                UpdateDeviceState("motor", direction != "stop");
                
                return true;
            });
        
        mcp_server.AddTool("self.device.get_all_status", 
            "获取所有设备状态", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                return GetDeviceStatusJson();
            });

        ESP_LOGI(TAG, "All MCP tools registered successfully");
    }

public:
    // ========== 重写基类方法 ==========
    
    Led* GetLed() override {
        // 返回LED控制接口，这里可以返回nullptr或实现简单的LED接口
        return nullptr;
    }
    
    std::string GetDeviceStatusJson() override {
        return GetDeviceStatusJson();
    }
};

// 工厂函数，供main.cc调用
extern "C" Board* CreateBoard() {
    return new CustomBoard();
}