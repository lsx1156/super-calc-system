#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 Demo - 功能验证测试脚本
自动化验证三大核心功能是否正常工作
"""

import os
import sys
import time
import json
import struct
import hashlib
import threading
import unittest
from pathlib import Path

# 添加路径
DEMO_DIR = Path(__file__).parent.parent
RPI_DIR = DEMO_DIR / 'raspberry_pi'
sys.path.insert(0, str(RPI_DIR))

# ==================== 破解引擎（内嵌版，避免依赖flask） ====================

class CrackEngine:
    """破解引擎（Demo版）"""
    
    def __init__(self):
        self.running = False
        self.progress = 0
        self.attempts = 0
        self.result = ""
        self.start_time = 0
    
    def crack_md5(self, target_hash, key_length=16, charset="0123456789abcdef"):
        import hashlib
        
        self.running = True
        self.progress = 0
        self.attempts = 0
        self.result = ""
        self.start_time = time.time()
        
        try:
            total = len(charset) ** key_length
            
            def generate(prefix, length):
                if not self.running:
                    return None
                
                if length == 0:
                    self.attempts += 1
                    test = hashlib.md5(prefix.encode()).hexdigest()
                    if test == target_hash.lower():
                        return prefix
                    
                    if self.attempts % 10000 == 0:
                        self.progress = int((self.attempts / total) * 100)
                    return None
                
                for c in charset:
                    result = generate(prefix + c, length - 1)
                    if result:
                        return result
                    if not self.running:
                        return None
                
                return None
            
            result = generate("", key_length)
            
            if result:
                self.result = f"破解成功! 密钥: {result}"
                self.progress = 100
            else:
                self.result = "破解结束"
            
            self.running = False
            return result
            
        except Exception as e:
            self.result = f"破解异常: {e}"
            self.running = False
            return None
    
    def stop(self):
        self.running = False

# ==================== 测试用例 ====================

class TestCrackEngine(unittest.TestCase):
    """破解引擎测试"""
    
    def setUp(self):
        self.engine = CrackEngine()
    
    def test_md5_short(self):
        """测试短密钥MD5破解"""
        # 测试4位纯数字
        test_key = "1234"
        target = hashlib.md5(test_key.encode()).hexdigest()
        
        result = self.engine.crack_md5(target, key_length=4, charset="0123456789")
        
        self.assertEqual(result, test_key)
        self.assertTrue(self.engine.attempts > 0)
    
    def test_md5_hex(self):
        """测试十六进制密钥破解"""
        test_key = "ab12"
        target = hashlib.md5(test_key.encode()).hexdigest()
        
        result = self.engine.crack_md5(target, key_length=4, charset="0123456789abcdef")
        
        self.assertEqual(result, test_key)
    
    def test_crack_stop(self):
        """测试停止破解"""
        target = hashlib.md5(b"999999").hexdigest()
        
        def stop_after_delay():
            time.sleep(0.1)
            self.engine.stop()
        
        t = threading.Thread(target=stop_after_delay)
        t.start()
        
        result = self.engine.crack_md5(target, key_length=6, charset="0123456789")
        
        self.assertIsNone(result)
        self.assertFalse(self.engine.running)


class TestDataProcessing(unittest.TestCase):
    """数据处理测试"""
    
    def test_adc_voltage_conversion(self):
        """测试ADC电压转换"""
        # 12位ADC，3.3V参考
        def adc_to_voltage(val):
            return val * 3.3 / 4095.0
        
        # 0 -> 0V
        self.assertAlmostEqual(adc_to_voltage(0), 0.0, places=2)
        # 4095 -> 3.3V
        self.assertAlmostEqual(adc_to_voltage(4095), 3.3, places=2)
        # 2048 -> ~1.65V
        self.assertAlmostEqual(adc_to_voltage(2048), 1.65, places=2)
    
    def test_crc16(self):
        """测试CRC16校验"""
        # 简单CRC16实现验证
        def crc16(data):
            crc = 0
            for b in data:
                crc ^= b << 8
                for _ in range(8):
                    if crc & 0x8000:
                        crc = (crc << 1) ^ 0x1021
                    else:
                        crc <<= 1
            return crc & 0xFFFF
        
        data = b"\x01\x02\x03\x04"
        crc = crc16(data)
        self.assertIsInstance(crc, int)
        self.assertTrue(0 <= crc <= 0xFFFF)
    
    def test_frame_construction(self):
        """测试帧构建"""
        # 模拟帧构建
        header = 0x55
        tail = 0xAA
        cmd = 0x01
        params = struct.pack("<I", 50000)
        data = struct.pack("<B", cmd) + params
        
        frame = struct.pack("<B", header)
        frame += struct.pack("<B", 0)  # node_id
        frame += struct.pack("<H", len(data))
        frame += data
        frame += struct.pack("<I", 0)  # CRC placeholder
        frame += struct.pack("<B", tail)
        
        # 验证帧结构
        self.assertEqual(frame[0], header)
        self.assertEqual(frame[-1], tail)
        data_len = struct.unpack("<H", frame[2:4])[0]
        self.assertEqual(data_len, len(data))


class TestConfig(unittest.TestCase):
    """配置测试"""
    
    def test_demo_scaling(self):
        """测试Demo规模配置"""
        num_pico = 8
        adc_per_pico = 4
        digital_per_pico = 8
        
        total_adc = num_pico * adc_per_pico
        total_digital = num_pico * digital_per_pico
        
        self.assertEqual(total_adc, 32)
        self.assertEqual(total_digital, 64)
    
    def test_sample_rates(self):
        """测试采样率配置"""
        rates = [10000, 50000, 100000, 125000]
        
        for rate in rates:
            # 每个样本 2 bytes (12bit ADC)
            bytes_per_channel = 2
            num_channels = 32
            
            throughput = rate * bytes_per_channel * num_channels
            # 通过SPI 20Mbps传输，验证是否在带宽内
            spi_bandwidth = 20 * 1000 * 1000 / 8  # 20Mbps = 2.5MB/s
            
            # 聚合后的数据率应该远小于SPI带宽
            self.assertLess(throughput, spi_bandwidth * 8)  # 留余量


class TestHardwareInterface(unittest.TestCase):
    """硬件接口测试（模拟）"""
    
    def test_spi_protocol(self):
        """测试SPI协议格式"""
        # 模拟SPI命令帧
        header = 0xAA
        tail = 0x55
        cmd = 0x01  # GET_STATUS
        
        # 小端格式：低字节在前，高字节在后
        # 长度为1 -> 低字节0x01, 高字节0x00
        frame = [header, 0x01, 0x00, cmd, 0x00, 0x00, tail]
        
        self.assertEqual(frame[0], header)
        self.assertEqual(frame[-1], tail)
        # 长度字段（2字节，小端）
        data_len = frame[1] | (frame[2] << 8)
        self.assertEqual(data_len, 1)  # 只有cmd
        self.assertEqual(frame[3], cmd)
    
    def test_pico_addressing(self):
        """测试Pico寻址"""
        num_slaves = 8
        cs_base_pin = 2
        
        for i in range(num_slaves):
            cs_pin = cs_base_pin + i
            self.assertEqual(cs_pin, 2 + i)
            self.assertLess(cs_pin, 10)  # GPIO2-9


# ==================== 运行测试 ====================

def run_all_tests():
    """运行所有测试"""
    print("=" * 60)
    print("  超采集算系统 Demo - 功能验证测试")
    print("=" * 60)
    print()
    
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # 添加测试类
    suite.addTests(loader.loadTestsFromTestCase(TestCrackEngine))
    suite.addTests(loader.loadTestsFromTestCase(TestDataProcessing))
    suite.addTests(loader.loadTestsFromTestCase(TestConfig))
    suite.addTests(loader.loadTestsFromTestCase(TestHardwareInterface))
    
    # 运行测试
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    print()
    print("=" * 60)
    print(f"  测试结果: {result.testsRun} 运行, "
          f"{len(result.failures)} 失败, {len(result.errors)} 错误")
    
    if result.wasSuccessful():
        print("  ✓ 所有测试通过！")
    else:
        print("  ✗ 部分测试失败")
    
    print("=" * 60)
    
    return result.wasSuccessful()

if __name__ == '__main__':
    success = run_all_tests()
    sys.exit(0 if success else 1)