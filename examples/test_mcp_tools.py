#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
小智ESP32项目MCP工具测试脚本
Test script for xiaozhi-esp32 MCP tools

使用方法:
python3 test_mcp_tools.py --host <ESP32_IP> --port 8080

或者通过WebSocket连接测试:
python3 test_mcp_tools.py --websocket ws://esp32.local:8080/ws
"""

import json
import argparse
import asyncio
import websockets
import time
from typing import Dict, Any, Optional

class McpTester:
    def __init__(self, websocket_url: Optional[str] = None, host: str = "192.168.1.100", port: int = 8080):
        self.websocket_url = websocket_url
        self.host = host
        self.port = port
        self.session_id = f"test_session_{int(time.time())}"
        self.request_id = 1
        
    def create_mcp_message(self, method: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """创建MCP消息"""
        message = {
            "session_id": self.session_id,
            "type": "mcp",
            "payload": {
                "jsonrpc": "2.0",
                "method": method,
                "params": params,
                "id": self.request_id
            }
        }
        self.request_id += 1
        return message
    
    async def send_websocket_message(self, message: Dict[str, Any]) -> Dict[str, Any]:
        """通过WebSocket发送消息"""
        if not self.websocket_url:
            raise ValueError("WebSocket URL not provided")
            
        async with websockets.connect(self.websocket_url) as websocket:
            # 发送消息
            await websocket.send(json.dumps(message))
            print(f"发送: {json.dumps(message, indent=2, ensure_ascii=False)}")
            
            # 接收响应
            response = await websocket.recv()
            response_data = json.loads(response)
            print(f"接收: {json.dumps(response_data, indent=2, ensure_ascii=False)}")
            
            return response_data
    
    async def test_initialize(self) -> bool:
        """测试MCP初始化"""
        print("\n=== 测试MCP初始化 ===")
        
        message = self.create_mcp_message("initialize", {
            "capabilities": {
                "vision": {
                    "url": "http://test.example.com/vision",
                    "token": "test_token"
                }
            }
        })
        
        try:
            response = await self.send_websocket_message(message)
            
            if "result" in response.get("payload", {}):
                print("✅ MCP初始化成功")
                result = response["payload"]["result"]
                print(f"协议版本: {result.get('protocolVersion', 'Unknown')}")
                print(f"服务器信息: {result.get('serverInfo', {})}")
                return True
            else:
                print("❌ MCP初始化失败")
                return False
                
        except Exception as e:
            print(f"❌ MCP初始化异常: {e}")
            return False
    
    async def test_tools_list(self) -> bool:
        """测试获取工具列表"""
        print("\n=== 测试获取工具列表 ===")
        
        message = self.create_mcp_message("tools/list", {
            "cursor": ""
        })
        
        try:
            response = await self.send_websocket_message(message)
            
            if "result" in response.get("payload", {}):
                print("✅ 获取工具列表成功")
                tools = response["payload"]["result"].get("tools", [])
                print(f"发现 {len(tools)} 个工具:")
                
                for tool in tools:
                    print(f"  - {tool.get('name', 'Unknown')}: {tool.get('description', 'No description')}")
                
                return True
            else:
                print("❌ 获取工具列表失败")
                return False
                
        except Exception as e:
            print(f"❌ 获取工具列表异常: {e}")
            return False
    
    async def test_led_controls(self) -> bool:
        """测试LED控制功能"""
        print("\n=== 测试LED控制功能 ===")
        
        test_cases = [
            # 打开灯
            {
                "name": "打开LED灯",
                "tool": "self.light.turn_on",
                "params": {}
            },
            # 设置颜色
            {
                "name": "设置红色",
                "tool": "self.light.set_rgb",
                "params": {"r": 255, "g": 0, "b": 0}
            },
            # 设置亮度
            {
                "name": "设置亮度50%",
                "tool": "self.light.set_brightness",
                "params": {"brightness": 50}
            },
            # 闪烁效果
            {
                "name": "蓝色闪烁3次",
                "tool": "self.light.blink",
                "params": {"r": 0, "g": 0, "b": 255, "times": 3, "interval": 500}
            },
            # 预设模式
            {
                "name": "暖光模式",
                "tool": "self.light.set_preset",
                "params": {"preset": "warm"}
            },
            # 关闭灯
            {
                "name": "关闭LED灯",
                "tool": "self.light.turn_off",
                "params": {}
            }
        ]
        
        success_count = 0
        
        for test_case in test_cases:
            print(f"\n测试: {test_case['name']}")
            
            message = self.create_mcp_message("tools/call", {
                "name": test_case["tool"],
                "arguments": test_case["params"]
            })
            
            try:
                response = await self.send_websocket_message(message)
                
                payload = response.get("payload", {})
                if "result" in payload:
                    print(f"✅ {test_case['name']} 成功")
                    success_count += 1
                elif "error" in payload:
                    print(f"❌ {test_case['name']} 失败: {payload['error']}")
                else:
                    print(f"❌ {test_case['name']} 响应格式错误")
                
                # 等待一秒再执行下一个测试
                await asyncio.sleep(1)
                
            except Exception as e:
                print(f"❌ {test_case['name']} 异常: {e}")
        
        print(f"\nLED控制测试完成: {success_count}/{len(test_cases)} 成功")
        return success_count == len(test_cases)
    
    async def test_device_controls(self) -> bool:
        """测试设备控制功能"""
        print("\n=== 测试设备控制功能 ===")
        
        test_cases = [
            # 继电器控制
            {
                "name": "打开继电器",
                "tool": "self.relay.control",
                "params": {"state": True}
            },
            {
                "name": "关闭继电器",
                "tool": "self.relay.control",
                "params": {"state": False}
            },
            # 传感器读取
            {
                "name": "读取温度",
                "tool": "self.sensor.read_temperature",
                "params": {}
            },
            {
                "name": "读取湿度",
                "tool": "self.sensor.read_humidity",
                "params": {}
            },
            # 电机控制
            {
                "name": "电机前进50步",
                "tool": "self.motor.control",
                "params": {"direction": "forward", "steps": 50, "speed": 30}
            },
            {
                "name": "停止电机",
                "tool": "self.motor.control",
                "params": {"direction": "stop", "steps": 0, "speed": 0}
            }
        ]
        
        success_count = 0
        
        for test_case in test_cases:
            print(f"\n测试: {test_case['name']}")
            
            message = self.create_mcp_message("tools/call", {
                "name": test_case["tool"],
                "arguments": test_case["params"]
            })
            
            try:
                response = await self.send_websocket_message(message)
                
                payload = response.get("payload", {})
                if "result" in payload:
                    print(f"✅ {test_case['name']} 成功")
                    success_count += 1
                elif "error" in payload:
                    print(f"❌ {test_case['name']} 失败: {payload['error']}")
                else:
                    print(f"❌ {test_case['name']} 响应格式错误")
                
                # 等待一秒再执行下一个测试
                await asyncio.sleep(1)
                
            except Exception as e:
                print(f"❌ {test_case['name']} 异常: {e}")
        
        print(f"\n设备控制测试完成: {success_count}/{len(test_cases)} 成功")
        return success_count == len(test_cases)
    
    async def test_device_status(self) -> bool:
        """测试设备状态查询"""
        print("\n=== 测试设备状态查询 ===")
        
        message = self.create_mcp_message("tools/call", {
            "name": "self.device.get_all_status",
            "arguments": {}
        })
        
        try:
            response = await self.send_websocket_message(message)
            
            payload = response.get("payload", {})
            if "result" in payload:
                print("✅ 设备状态查询成功")
                result = payload["result"]
                
                if isinstance(result, dict):
                    print("设备状态信息:")
                    print(json.dumps(result, indent=2, ensure_ascii=False))
                else:
                    # 如果返回的是JSON字符串，解析它
                    try:
                        status_data = json.loads(result)
                        print("设备状态信息:")
                        print(json.dumps(status_data, indent=2, ensure_ascii=False))
                    except:
                        print(f"状态数据: {result}")
                
                return True
            else:
                print("❌ 设备状态查询失败")
                return False
                
        except Exception as e:
            print(f"❌ 设备状态查询异常: {e}")
            return False
    
    async def run_all_tests(self) -> None:
        """运行所有测试"""
        print("开始小智ESP32 MCP工具测试")
        print(f"WebSocket连接: {self.websocket_url}")
        print(f"会话ID: {self.session_id}")
        
        tests = [
            ("MCP初始化", self.test_initialize),
            ("工具列表", self.test_tools_list),
            ("LED控制", self.test_led_controls),
            ("设备控制", self.test_device_controls),
            ("设备状态", self.test_device_status)
        ]
        
        results = {}
        
        for test_name, test_func in tests:
            try:
                result = await test_func()
                results[test_name] = result
            except Exception as e:
                print(f"❌ {test_name} 测试异常: {e}")
                results[test_name] = False
        
        # 输出测试结果摘要
        print("\n" + "="*50)
        print("测试结果摘要:")
        print("="*50)
        
        total_tests = len(results)
        passed_tests = sum(1 for result in results.values() if result)
        
        for test_name, result in results.items():
            status = "✅ 通过" if result else "❌ 失败"
            print(f"{test_name}: {status}")
        
        print(f"\n总体结果: {passed_tests}/{total_tests} 测试通过")
        
        if passed_tests == total_tests:
            print("🎉 所有测试均通过！")
        else:
            print("⚠️  部分测试失败，请检查设备连接和配置")


def main():
    parser = argparse.ArgumentParser(description="小智ESP32 MCP工具测试脚本")
    parser.add_argument("--websocket", type=str, help="WebSocket连接URL，例如: ws://192.168.1.100:8080/ws")
    parser.add_argument("--host", type=str, default="192.168.1.100", help="ESP32设备IP地址")
    parser.add_argument("--port", type=int, default=8080, help="WebSocket端口")
    
    args = parser.parse_args()
    
    # 构建WebSocket URL
    if args.websocket:
        websocket_url = args.websocket
    else:
        websocket_url = f"ws://{args.host}:{args.port}/ws"
    
    # 创建测试器并运行测试
    tester = McpTester(websocket_url=websocket_url, host=args.host, port=args.port)
    
    try:
        asyncio.run(tester.run_all_tests())
    except KeyboardInterrupt:
        print("\n用户中断测试")
    except Exception as e:
        print(f"测试运行异常: {e}")


if __name__ == "__main__":
    main()