#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数字信号处理算法库 (digital_algorithms.py)
- 边沿检测 (Edge Detection)
- UART协议解码器
- SPI协议解码器
- I2C协议解码器
- 脉冲宽度测量
- 曼彻斯特编解码

内存设计原则：
- 所有解码器使用固定大小的环形缓冲区
- 单通道状态机 < 512字节
- 避免动态分配，复用状态变量
"""

from collections import deque
from typing import List, Tuple, Optional, Dict
import time


class EdgeDetector:
    """
    边沿检测器
    - 上升沿 / 下降沿 / 双边沿
    - 去抖动 (Debounce)
    - 内存：O(1) 固定大小
    """
    
    EDGE_RISING = 1
    EDGE_FALLING = 2
    EDGE_BOTH = 3
    
    def __init__(self, edge_type: int = 3, debounce_samples: int = 3):
        self.edge_type = edge_type
        self.debounce_samples = debounce_samples
        self._last_val = 0
        self._stable_val = 0
        self._debounce_count = 0
    
    def reset(self):
        self._last_val = 0
        self._stable_val = 0
        self._debounce_count = 0
    
    def process(self, value: int) -> Tuple[int, int]:
        """
        输入一个采样值，返回 (edge_type, new_level)
        edge_type: 0=无变化, 1=上升沿, 2=下降沿
        """
        edge = 0
        
        if value != self._last_val:
            self._debounce_count = 0
            self._last_val = value
        else:
            if self._debounce_count < self.debounce_samples:
                self._debounce_count += 1
            else:
                if value != self._stable_val:
                    if value == 1 and self._stable_val == 0:
                        if self.edge_type & self.EDGE_RISING:
                            edge = 1
                    elif value == 0 and self._stable_val == 1:
                        if self.edge_type & self.EDGE_FALLING:
                            edge = 2
                    self._stable_val = value
        
        return (edge, self._stable_val)
    
    @property
    def memory_usage(self) -> int:
        return 32


class UARTDecoder:
    """
    UART协议解码器
    - 支持：8N1, 8E1, 8O1, 7N1 等常用配置
    - 状态机实现，逐bit解码
    - 内存：固定缓冲区 + 状态变量
    
    默认配置：9600 8N1 (8数据位, 无校验, 1停止位)
    """
    
    # 校验类型
    PARITY_NONE = 0
    PARITY_EVEN = 1
    PARITY_ODD = 2
    
    # 状态
    STATE_IDLE = 0      # 等待起始位
    STATE_START = 1     # 起始位确认
    STATE_DATA = 2      # 接收数据位
    STATE_PARITY = 3    # 校验位
    STATE_STOP = 4      # 停止位
    
    def __init__(self, baudrate: int = 9600, sample_rate: int = 100000,
                 data_bits: int = 8, parity: int = 0, stop_bits: int = 1):
        self.baudrate = baudrate
        self.sample_rate = sample_rate
        self.data_bits = data_bits
        self.parity = parity
        self.stop_bits = stop_bits
        
        # 每个bit的采样点数
        self.samples_per_bit = sample_rate // baudrate
        if self.samples_per_bit < 3:
            self.samples_per_bit = 3
        
        # 解码状态
        self._state = self.STATE_IDLE
        self._bit_count = 0
        self._sample_count = 0
        self._current_byte = 0
        self._high_count = 0  # 当前bit内高电平计数（中间采样）
        
        # 输出缓冲
        self._output: deque = deque(maxlen=256)
        self._errors = 0
    
    def reset(self):
        self._state = self.STATE_IDLE
        self._bit_count = 0
        self._sample_count = 0
        self._current_byte = 0
        self._high_count = 0
        self._output.clear()
        self._errors = 0
    
    def process(self, sample: int) -> Optional[int]:
        """
        输入一个bit采样值(0或1)
        解码完成一个字节返回该字节，否则返回None
        """
        if self._state == self.STATE_IDLE:
            if sample == 0:
                self._state = self.STATE_START
                self._sample_count = 1
                self._high_count = 0
            return None
        
        self._sample_count += 1
        if sample == 1:
            self._high_count += 1
        
        mid_point = self.samples_per_bit // 2
        
        if self._state == self.STATE_START:
            if self._sample_count >= self.samples_per_bit:
                mid_val = 1 if self._high_count > self.samples_per_bit // 2 else 0
                if mid_val == 0:
                    self._state = self.STATE_DATA
                    self._bit_count = 0
                    self._current_byte = 0
                    self._sample_count = 0
                    self._high_count = 0
                else:
                    self._state = self.STATE_IDLE
                    self._errors += 1
                return None
        
        elif self._state == self.STATE_DATA:
            if self._sample_count >= self.samples_per_bit:
                bit_val = 1 if self._high_count > self.samples_per_bit // 2 else 0
                self._current_byte = (self._current_byte >> 1) | (bit_val << (self.data_bits - 1))
                self._bit_count += 1
                self._sample_count = 0
                self._high_count = 0
                
                if self._bit_count >= self.data_bits:
                    if self.parity == self.PARITY_NONE:
                        self._state = self.STATE_STOP
                        self._bit_count = 0
                    else:
                        self._state = self.STATE_PARITY
                return None
        
        elif self._state == self.STATE_PARITY:
            if self._sample_count >= self.samples_per_bit:
                parity_bit = 1 if self._high_count > self.samples_per_bit // 2 else 0
                # 计算数据中的1的个数
                ones = bin(self._current_byte).count('1')
                expected = ones % 2
                
                if self.parity == self.PARITY_EVEN and parity_bit != expected:
                    self._errors += 1
                elif self.parity == self.PARITY_ODD and parity_bit == expected:
                    self._errors += 1
                
                self._state = self.STATE_STOP
                self._bit_count = 0
                self._sample_count = 0
                self._high_count = 0
                return None
        
        elif self._state == self.STATE_STOP:
            if self._sample_count >= self.samples_per_bit:
                stop_val = 1 if self._high_count > self.samples_per_bit // 2 else 0
                
                self._bit_count += 1
                self._sample_count = 0
                self._high_count = 0
                
                if self._bit_count >= self.stop_bits:
                    self._state = self.STATE_IDLE
                    if stop_val == 1:
                        result = self._current_byte
                        self._output.append(result)
                        return result
                
                return None
        
        return None
    
    def process_batch(self, samples: List[int]) -> List[int]:
        """批量处理，返回解码出的字节列表"""
        result = []
        for s in samples:
            byte = self.process(s)
            if byte is not None:
                result.append(byte)
        return result
    
    def read_byte(self) -> Optional[int]:
        """从缓冲区读取一个字节"""
        if self._output:
            return self._output.popleft()
        return None
    
    def available(self) -> int:
        return len(self._output)
    
    @property
    def error_count(self) -> int:
        return self._errors
    
    @property
    def memory_usage(self) -> int:
        return 256 + 64  # 缓冲+状态


class SPIDecoder:
    """
    SPI协议解码器
    - 支持：模式0/1/2/3 (CPOL, CPHA)
    - 支持：MSB/LSB 优先
    - 内存：固定大小状态机
    """
    
    MODE_0 = 0  # CPOL=0, CPHA=0
    MODE_1 = 1  # CPOL=0, CPHA=1
    MODE_2 = 2  # CPOL=1, CPHA=0
    MODE_3 = 3  # CPOL=1, CPHA=1
    
    def __init__(self, mode: int = 0, bits_per_word: int = 8, msb_first: bool = True):
        self.mode = mode
        self.bits_per_word = bits_per_word
        self.msb_first = msb_first
        
        self._cpol = (mode >> 1) & 1
        self._cpha = mode & 1
        
        self._last_clk = 0
        self._last_cs = 1
        self._bit_count = 0
        self._current_word = 0
        self._active = False
        
        self._output: deque = deque(maxlen=256)
        self._mosi_buffer: deque = deque(maxlen=256)
        self._miso_buffer: deque = deque(maxlen=256)
    
    def reset(self):
        self._last_clk = 0
        self._last_cs = 1
        self._bit_count = 0
        self._current_word = 0
        self._active = False
        self._output.clear()
        self._mosi_buffer.clear()
        self._miso_buffer.clear()
    
    def process(self, sclk: int, mosi: int, miso: int, cs: int) -> Optional[Tuple[int, int]]:
        """
        输入 SPI 信号采样
        返回 (mosi_byte, miso_byte) 或 None
        """
        result = None
        
        if cs == 0 and self._last_cs == 1:
            self._active = True
            self._bit_count = 0
            self._current_word = 0
        
        if cs == 1 and self._last_cs == 0:
            self._active = False
        
        if self._active:
            rising = (sclk == 1 and self._last_clk == 0)
            falling = (sclk == 0 and self._last_clk == 1)
            
            sample_edge = rising if (self._cpha == 0) else falling
            shift_edge = falling if (self._cpha == 0) else rising
            
            if sample_edge:
                if self.msb_first:
                    self._current_word = (self._current_word << 1) | mosi
                else:
                    self._current_word = (self._current_word >> 1) | (mosi << (self.bits_per_word - 1))
                
                self._bit_count += 1
                
                if self._bit_count >= self.bits_per_word:
                    mosi_byte = self._current_word
                    miso_byte = 0
                    self._mosi_buffer.append(mosi_byte)
                    self._miso_buffer.append(miso_byte)
                    result = (mosi_byte, miso_byte)
                    self._bit_count = 0
                    self._current_word = 0
        
        self._last_clk = sclk
        self._last_cs = cs
        
        return result
    
    @property
    def memory_usage(self) -> int:
        return 512 + 128


class I2CDecoder:
    """
    I2C协议解码器
    - 起始/停止条件检测
    - 7位/10位地址
    - ACK/NACK检测
    - 内存：轻量状态机
    """
    
    STATE_IDLE = 0
    STATE_ADDR = 1
    STATE_DATA = 2
    STATE_ACK = 3
    
    def __init__(self, address_bits: int = 7):
        self.address_bits = address_bits
        
        self._last_sda = 1
        self._last_scl = 1
        self._state = self.STATE_IDLE
        self._bit_count = 0
        self._current_byte = 0
        self._read_mode = False
        
        self._output: deque = deque(maxlen=256)
    
    def reset(self):
        self._last_sda = 1
        self._last_scl = 1
        self._state = self.STATE_IDLE
        self._bit_count = 0
        self._current_byte = 0
        self._output.clear()
    
    def process(self, sda: int, scl: int) -> Optional[Dict]:
        """
        输入SDA/SCL采样，返回事件字典或None
        事件类型: 'start', 'stop', 'addr', 'data', 'ack', 'nack'
        """
        result = None
        
        # 起始条件：SCL高时SDA下降沿
        if scl == 1 and self._last_scl == 1 and sda == 0 and self._last_sda == 1:
            self._state = self.STATE_ADDR
            self._bit_count = 0
            self._current_byte = 0
            result = {'type': 'start'}
        
        # 停止条件：SCL高时SDA上升沿
        elif scl == 1 and self._last_scl == 1 and sda == 1 and self._last_sda == 0:
            self._state = self.STATE_IDLE
            result = {'type': 'stop'}
        
        # SCL上升沿采样数据
        elif scl == 1 and self._last_scl == 0:
            if self._state in (self.STATE_ADDR, self.STATE_DATA):
                self._current_byte = (self._current_byte << 1) | sda
                self._bit_count += 1
                
                total_bits = self.address_bits + 1 if self._state == self.STATE_ADDR else 8
                
                if self._bit_count >= total_bits:
                    if self._state == self.STATE_ADDR:
                        addr = self._current_byte >> 1
                        rw = self._current_byte & 1
                        self._read_mode = (rw == 1)
                        result = {'type': 'addr', 'addr': addr, 'read': self._read_mode}
                    else:
                        result = {'type': 'data', 'value': self._current_byte}
                    
                    self._state = self.STATE_ACK
                    self._bit_count = 0
            
            elif self._state == self.STATE_ACK:
                ack = (sda == 0)
                result = {'type': 'ack' if ack else 'nack'}
                self._state = self.STATE_DATA
                self._bit_count = 0
                self._current_byte = 0
        
        self._last_sda = sda
        self._last_scl = scl
        
        return result
    
    @property
    def memory_usage(self) -> int:
        return 256 + 64


class PulseWidthMeter:
    """
    脉冲宽度测量器
    - 测量高电平/低电平持续时间
    - 频率测量
    - 占空比计算
    - 内存：O(1)
    """
    
    def __init__(self, sample_rate: int = 100000):
        self.sample_rate = sample_rate
        self._last_val = 0
        self._high_samples = 0
        self._low_samples = 0
        self._period_samples = 0
        self._last_period = 0
        self._last_high_width = 0
        self._last_low_width = 0
    
    def reset(self):
        self._last_val = 0
        self._high_samples = 0
        self._low_samples = 0
        self._period_samples = 0
        self._last_period = 0
        self._last_high_width = 0
        self._last_low_width = 0
    
    def process(self, value: int) -> Optional[Dict]:
        """
        输入采样值，有新周期时返回测量结果
        """
        result = None
        
        if value == 1:
            if self._last_val == 0:
                self._last_low_width = self._low_samples
                self._high_samples = 1
                self._period_samples += 1
            else:
                self._high_samples += 1
                self._period_samples += 1
        else:
            if self._last_val == 1:
                self._last_high_width = self._high_samples
                self._low_samples = 1
                self._last_period = self._period_samples
                self._period_samples = 1
                
                period_sec = self._last_period / self.sample_rate
                freq = 1.0 / period_sec if period_sec > 0 else 0
                duty = (self._last_high_width / self._last_period * 100) if self._last_period > 0 else 0
                
                result = {
                    'frequency_hz': freq,
                    'period_s': period_sec,
                    'high_width_s': self._last_high_width / self.sample_rate,
                    'low_width_s': self._last_low_width / self.sample_rate,
                    'duty_percent': duty,
                }
            else:
                self._low_samples += 1
                self._period_samples += 1
        
        self._last_val = value
        return result
    
    @property
    def memory_usage(self) -> int:
        return 64


class ManchesterDecoder:
    """
    曼彻斯特码解码器
    - 每个bit中间有跳变
    - 0 = 上升沿, 1 = 下降沿（或相反，可配置）
    - 内存：状态机 + 小缓冲
    """
    
    def __init__(self, bit_rate: int = 1000, sample_rate: int = 100000,
                 zero_is_rising: bool = True):
        self.bit_rate = bit_rate
        self.sample_rate = sample_rate
        self.zero_is_rising = zero_is_rising
        
        self.samples_per_bit = sample_rate // bit_rate
        
        self._last_val = 0
        self._samples_since_edge = 0
        self._current_byte = 0
        self._bit_count = 0
        
        self._output: deque = deque(maxlen=256)
    
    def reset(self):
        self._last_val = 0
        self._samples_since_edge = 0
        self._current_byte = 0
        self._bit_count = 0
        self._output.clear()
    
    def process(self, value: int) -> Optional[int]:
        result = None
        
        edge = 0
        if value != self._last_val:
            if value == 1:
                edge = 1
            else:
                edge = 2
        
        if edge > 0:
            half_bit = self.samples_per_bit // 2
            
            if self._samples_since_edge > half_bit + half_bit // 2:
                bit_val = 0
                if self.zero_is_rising:
                    bit_val = 0 if edge == 1 else 1
                else:
                    bit_val = 1 if edge == 1 else 0
                
                self._current_byte = (self._current_byte << 1) | bit_val
                self._bit_count += 1
                
                if self._bit_count >= 8:
                    self._output.append(self._current_byte)
                    result = self._current_byte
                    self._bit_count = 0
                    self._current_byte = 0
            
            self._samples_since_edge = 0
        else:
            self._samples_since_edge += 1
        
        self._last_val = value
        return result
    
    @property
    def memory_usage(self) -> int:
        return 256 + 64


class DigitalChannelProcessor:
    """
    数字通道处理器
    - 单通道集成：边沿检测 + 脉宽测量 + UART解码
    - 内存可控，按需启用功能
    """
    
    def __init__(self, channel_id: int = 0, sample_rate: int = 100000):
        self.channel_id = channel_id
        self.sample_rate = sample_rate
        
        self.edge_detector = EdgeDetector()
        self.pulse_meter = PulseWidthMeter(sample_rate)
        self.uart = UARTDecoder(sample_rate=sample_rate)
        
        self._edge_count = 0
        self._rising_count = 0
        self._falling_count = 0
    
    def reset(self):
        self.edge_detector.reset()
        self.pulse_meter.reset()
        self.uart.reset()
        self._edge_count = 0
        self._rising_count = 0
        self._falling_count = 0
    
    def process(self, value: int) -> Dict:
        edge, level = self.edge_detector.process(value)
        
        if edge == 1:
            self._edge_count += 1
            self._rising_count += 1
        elif edge == 2:
            self._edge_count += 1
            self._falling_count += 1
        
        pulse_info = self.pulse_meter.process(value)
        uart_byte = self.uart.process(value)
        
        return {
            'channel': self.channel_id,
            'level': level,
            'edge': edge,
            'edge_count': self._edge_count,
            'rising_count': self._rising_count,
            'falling_count': self._falling_count,
            'pulse': pulse_info,
            'uart_byte': uart_byte,
        }
    
    def get_memory_usage(self) -> dict:
        return {
            'edge_detect': self.edge_detector.memory_usage,
            'pulse_meter': self.pulse_meter.memory_usage,
            'uart': self.uart.memory_usage,
            'total': (self.edge_detector.memory_usage + 
                     self.pulse_meter.memory_usage + 
                     self.uart.memory_usage),
        }
