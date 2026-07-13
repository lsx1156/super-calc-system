#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 业务逻辑模块
负责数据解析、协议破解、FFT分析
"""

import numpy as np
import threading
import queue
import time
import logging
import hashlib
import json
from typing import Optional, List, Dict, Any
from collections import deque
from config import (
    DATA_QUEUE_SIZE, DEFAULT_KEY_LENGTH, MAX_KEY_LENGTH,
    CRACK_TIMEOUT, ADC_CHANNELS, ADC_RESOLUTION
)

logger = logging.getLogger("BusinessLogic")


class DataParser:
    """数据解析器"""
    
    def __init__(self):
        self.analog_buffer = deque(maxlen=1024)
        self.digital_buffer = deque(maxlen=1024)
    
    def parse_analog_data(self, data: bytes) -> Dict[str, Any]:
        """解析模拟采样数据"""
        try:
            # 解析数据类型
            data_type = data[0]
            if data_type != 0x01:  # 不是模拟数据
                return {}
            
            # 解析通道数据
            # 每个采样点2字节（12bit）
            sample_count = (len(data) - 1) // 2
            samples = []
            
            for i in range(sample_count):
                sample_bytes = data[1 + i*2:1 + i*2 + 2]
                sample_value = struct.unpack("<H", sample_bytes)[0]
                # 12bit分辨率，高4位为通道号
                channel = (sample_value >> 12) & 0x0F
                value = sample_value & 0x0FFF
                
                samples.append({
                    "channel": channel,
                    "value": value,
                    "voltage": value * 3.3 / 4095  # 转换为电压
                })
            
            # 存入缓冲区
            self.analog_buffer.extend(samples)
            
            return {
                "type": "analog",
                "samples": samples,
                "count": sample_count,
                "timestamp": time.time()
            }
            
        except Exception as e:
            logger.error(f"解析模拟数据异常: {e}")
            return {}
    
    def parse_digital_data(self, data: bytes) -> Dict[str, Any]:
        """解析数字捕获数据"""
        try:
            # 解析数据类型
            data_type = data[0]
            if data_type != 0x02:  # 不是数字数据
                return {}
            
            # 解析位数据
            # 每字节包含8个通道的状态
            bit_count = (len(data) - 1) * 8
            states = []
            
            for i in range(len(data) - 1):
                byte_value = data[i + 1]
                for j in range(8):
                    channel = i * 8 + j
                    state = (byte_value >> j) & 0x01
                    states.append({
                        "channel": channel,
                        "state": state
                    })
            
            # 存入缓冲区
            self.digital_buffer.extend(states)
            
            return {
                "type": "digital",
                "states": states,
                "count": bit_count,
                "timestamp": time.time()
            }
            
        except Exception as e:
            logger.error(f"解析数字数据异常: {e}")
            return {}
    
    def get_analog_waveform(self, channel: int = None) -> List[float]:
        """获取模拟波形数据"""
        if channel is None:
            # 返回所有通道的平均值
            return [s["voltage"] for s in list(self.analog_buffer)[-64:]]
        else:
            # 返回指定通道的数据
            return [s["voltage"] for s in list(self.analog_buffer) 
                    if s["channel"] == channel][-64:]


class FFTAnalyzer:
    """FFT分析器"""
    
    def __init__(self):
        self.sample_rate = 125000  # 125KSPS
    
    def analyze(self, data: List[float]) -> Dict[str, Any]:
        """执行FFT分析"""
        try:
            # 转换为numpy数组
            samples = np.array(data)
            
            # 执行FFT
            n = len(samples)
            fft_result = np.fft.fft(samples)
            fft_freq = np.fft.fftfreq(n, 1/self.sample_rate)
            
            # 计算幅度
            fft_magnitude = np.abs(fft_result)
            
            # 只取正频率部分
            positive_freq = fft_freq[:n//2]
            positive_magnitude = fft_magnitude[:n//2]
            
            # 找主频率
            peak_idx = np.argmax(positive_magnitude)
            peak_freq = positive_freq[peak_idx]
            peak_magnitude = positive_magnitude[peak_idx]
            
            return {
                "frequencies": positive_freq.tolist(),
                "magnitudes": positive_magnitude.tolist(),
                "peak_frequency": peak_freq,
                "peak_magnitude": peak_magnitude,
                "sample_count": n
            }
            
        except Exception as e:
            logger.error(f"FFT分析异常: {e}")
            return {}


class CrackEngine:
    """破解引擎"""
    
    def __init__(self):
        self.running = False
        self.progress = 0
        self.result = ""
        self.key_length = DEFAULT_KEY_LENGTH
        self.attempts = 0
        self.start_time = 0
    
    def crack_hash(self, target_hash: str, key_length: int = 16, 
                   charset: str = "0123456789abcdef") -> Optional[str]:
        """暴力破解哈希"""
        self.running = True
        self.progress = 0
        self.key_length = key_length
        self.attempts = 0
        self.start_time = time.time()
        
        logger.info(f"开始破解: 目标={target_hash}, 密钥长度={key_length}")
        
        # 生成所有可能的密钥
        total_combinations = len(charset) ** key_length
        
        try:
            # 递归生成密钥
            def generate_keys(prefix: str, length: int):
                if length == 0:
                    # 检查密钥
                    self.attempts += 1
                    test_hash = hashlib.md5(prefix.encode()).hexdigest()
                    
                    if test_hash == target_hash:
                        return prefix
                    
                    # 更新进度
                    self.progress = int((self.attempts / total_combinations) * 100)
                    
                    # 检查超时
                    if time.time() - self.start_time > CRACK_TIMEOUT:
                        return None
                    
                    return None
                
                for char in charset:
                    if not self.running:
                        return None
                    
                    result = generate_keys(prefix + char, length - 1)
                    if result:
                        return result
                
                return None
            
            # 开始破解
            result = generate_keys("", key_length)
            
            if result:
                self.result = f"破解成功: 密钥={result}"
                logger.info(f"破解成功: {result}, 尝试次数={self.attempts}")
                return result
            else:
                self.result = f"破解失败: 尝试次数={self.attempts}"
                logger.warning(f"破解失败: 尝试次数={self.attempts}")
                return None
            
        except Exception as e:
            logger.error(f"破解异常: {e}")
            self.result = f"破解异常: {e}"
            return None
    
    def stop(self):
        """停止破解"""
        self.running = False
    
    def get_progress(self) -> Dict[str, Any]:
        """获取破解进度"""
        elapsed = time.time() - self.start_time if self.start_time > 0 else 0
        rate = self.attempts / elapsed if elapsed > 0 else 0
        
        return {
            "progress": self.progress,
            "attempts": self.attempts,
            "elapsed_time": elapsed,
            "rate": rate,
            "result": self.result,
            "running": self.running
        }


class BusinessLogic:
    """业务逻辑处理器"""
    
    def __init__(self, data_queue: queue.Queue, result_queue: queue.Queue):
        self.data_queue = data_queue
        self.result_queue = result_queue
        self.running = False
        
        # 子模块
        self.parser = DataParser()
        self.fft_analyzer = FFTAnalyzer()
        self.crack_engine = CrackEngine()
        
        # 处理线程
        self.process_thread = None
    
    def start(self):
        """启动业务逻辑处理"""
        self.running = True
        
        # 启动处理线程
        self.process_thread = threading.Thread(
            target=self._process_loop,
            daemon=True
        )
        self.process_thread.start()
        
        logger.info("业务逻辑处理器启动")
    
    def stop(self):
        """停止业务逻辑处理"""
        self.running = False
        self.crack_engine.stop()
        
        if self.process_thread:
            self.process_thread.join(timeout=2)
        
        logger.info("业务逻辑处理器停止")
    
    def _process_loop(self):
        """处理循环"""
        while self.running:
            try:
                # 从队列获取数据
                data_item = self.data_queue.get(timeout=0.1)
                
                # 解析数据
                if data_item.get("data"):
                    raw_data = data_item["data"]
                    data_type = raw_data[0] if raw_data else 0
                    
                    if data_type == 0x01:  # 模拟数据
                        parsed = self.parser.parse_analog_data(raw_data)
                        
                        # FFT分析
                        waveform = self.parser.get_analog_waveform()
                        if len(waveform) >= 64:
                            fft_result = self.fft_analyzer.analyze(waveform)
                            parsed["fft"] = fft_result
                        
                        # 发送结果
                        self.result_queue.put({
                            "type": "analog",
                            "data": parsed,
                            "waveform": waveform
                        })
                    
                    elif data_type == 0x02:  # 数字数据
                        parsed = self.parser.parse_digital_data(raw_data)
                        self.result_queue.put({
                            "type": "digital",
                            "data": parsed
                        })
                
            except queue.Empty:
                continue
            except Exception as e:
                logger.error(f"处理异常: {e}")
    
    def start_crack(self, target_hash: str, key_length: int = 16) -> bool:
        """启动破解"""
        thread = threading.Thread(
            target=self.crack_engine.crack_hash,
            args=(target_hash, key_length),
            daemon=True
        )
        thread.start()
        return True
    
    def stop_crack(self):
        """停止破解"""
        self.crack_engine.stop()
    
    def get_crack_progress(self) -> Dict[str, Any]:
        """获取破解进度"""
        return self.crack_engine.get_progress()
    
    def get_statistics(self) -> Dict[str, Any]:
        """获取统计信息"""
        return {
            "analog_buffer_size": len(self.parser.analog_buffer),
            "digital_buffer_size": len(self.parser.digital_buffer),
            "crack_progress": self.crack_engine.get_progress()
        }


def create_business_logic(data_queue: queue.Queue, result_queue: queue.Queue) -> BusinessLogic:
    """创建业务逻辑处理器"""
    return BusinessLogic(data_queue, result_queue)