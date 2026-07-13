#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模拟信号处理算法库 (analog_algorithms.py)
- 滑动平均滤波 (Moving Average)
- 中值滤波 (Median Filter)
- FIR数字滤波 (有限长单位冲激响应)
- 峰值检测 (Peak Detection)
- 互相关分析 (Cross-correlation)

内存设计原则：
- 所有缓冲区使用固定大小，避免动态分配
- 采用环形缓冲区(cyclic buffer)复用内存
- 单通道内存占用 < 2KB
"""

import math
from collections import deque
from typing import List, Tuple, Optional


class MovingAverageFilter:
    """
    滑动平均滤波器
    - 内存：O(N)，N为窗口大小
    - 计算：O(1) per sample（增量计算）
    - 适用：去除高斯白噪声
    """
    
    def __init__(self, window_size: int = 8):
        self.window_size = window_size
        self._buffer = deque(maxlen=window_size)
        self._sum = 0.0
    
    def reset(self):
        self._buffer.clear()
        self._sum = 0.0
    
    def process(self, value: float) -> float:
        """增量式滑动平均，O(1)复杂度"""
        if len(self._buffer) == self.window_size:
            self._sum -= self._buffer[0]
        
        self._buffer.append(value)
        self._sum += value
        
        return self._sum / len(self._buffer)
    
    def process_batch(self, values: List[float]) -> List[float]:
        return [self.process(v) for v in values]
    
    @property
    def memory_usage(self) -> int:
        return self.window_size * 8  # float8字节


class MedianFilter:
    """
    中值滤波器
    - 内存：O(N)，N为窗口大小
    - 计算：O(N log N) per sample
    - 适用：去除脉冲噪声、椒盐噪声
    - 窗口建议：3-7（太大计算慢）
    """
    
    def __init__(self, window_size: int = 5):
        self.window_size = window_size
        self._buffer = deque(maxlen=window_size)
    
    def reset(self):
        self._buffer.clear()
    
    def process(self, value: float) -> float:
        self._buffer.append(value)
        
        n = len(self._buffer)
        if n < 3:
            return value
        
        sorted_vals = sorted(self._buffer)
        return sorted_vals[n // 2]
    
    def process_batch(self, values: List[float]) -> List[float]:
        return [self.process(v) for v in values]
    
    @property
    def memory_usage(self) -> int:
        return self.window_size * 8


class FIRFilter:
    """
    FIR数字滤波器（有限长单位冲激响应）
    - 内存：O(N)，N为抽头数
    - 计算：O(N) per sample
    - 特点：线性相位，稳定，可设计为低通/高通/带通/带阻
    
    预设计常用系数（可直接用，不用scipy）
    """
    
    def __init__(self, coefficients: List[float]):
        self.coeffs = coefficients
        self._tap_count = len(coefficients)
        self._buffer = [0.0] * self._tap_count  # 固定数组，环形
        self._index = 0  # 写入位置
    
    def reset(self):
        for i in range(self._tap_count):
            self._buffer[i] = 0.0
        self._index = 0
    
    def process(self, value: float) -> float:
        """FIR滤波，环形缓冲区实现"""
        self._buffer[self._index] = value
        self._index = (self._index + 1) % self._tap_count
        
        result = 0.0
        for i in range(self._tap_count):
            idx = (self._index + i) % self._tap_count
            result += self.coeffs[i] * self._buffer[idx]
        
        return result
    
    def process_batch(self, values: List[float]) -> List[float]:
        return [self.process(v) for v in values]
    
    @property
    def memory_usage(self) -> int:
        return self._tap_count * 8 * 2  # 系数+缓冲区
    
    @staticmethod
    def design_lowpass(cutoff_freq: float, sample_rate: float, num_taps: int = 31) -> 'FIRFilter':
        """
        设计低通FIR滤波器（窗函数法，汉宁窗）
        cutoff_freq: 截止频率(Hz)
        sample_rate: 采样率(Hz)
        num_taps: 抽头数（奇数）
        """
        if num_taps % 2 == 0:
            num_taps += 1
        
        nyquist = sample_rate / 2.0
        wc = 2.0 * math.pi * cutoff_freq / sample_rate  # 归一化截止频率
        M = num_taps - 1
        
        coeffs = []
        for n in range(num_taps):
            if n == M / 2:
                h = wc / math.pi
            else:
                h = math.sin(wc * (n - M / 2)) / (math.pi * (n - M / 2))
            
            w = 0.5 - 0.5 * math.cos(2.0 * math.pi * n / M)  # 汉宁窗
            coeffs.append(h * w)
        
        # 归一化：直流增益为1
        total = sum(coeffs)
        if abs(total) > 1e-10:
            coeffs = [c / total for c in coeffs]
        
        return FIRFilter(coeffs)
    
    @staticmethod
    def design_highpass(cutoff_freq: float, sample_rate: float, num_taps: int = 31) -> 'FIRFilter':
        """设计高通FIR滤波器（频谱反转法）"""
        lowpass = FIRFilter.design_lowpass(cutoff_freq, sample_rate, num_taps)
        coeffs = [-c for c in lowpass.coeffs]
        mid = len(coeffs) // 2
        coeffs[mid] += 1.0
        return FIRFilter(coeffs)


class PeakDetector:
    """
    峰值检测器
    - 内存：O(1) 固定大小
    - 检测：过零检测+斜率变化
    - 可调：最小峰值高度、最小峰间距
    """
    
    def __init__(self, min_height: float = 0.1, min_distance: int = 5):
        self.min_height = min_height
        self.min_distance = min_distance
        self._last_val = 0.0
        self._last_peak_pos = -1000
        self._sample_idx = 0
        self._rising = False
    
    def reset(self):
        self._last_val = 0.0
        self._last_peak_pos = -1000
        self._sample_idx = 0
        self._rising = False
    
    def process(self, value: float) -> Tuple[bool, float]:
        """
        返回 (is_peak, peak_value)
        检测到峰值返回 (True, value)，否则 (False, 0)
        """
        is_peak = False
        
        if value > self._last_val:
            self._rising = True
        elif value < self._last_val and self._rising:
            # 上升转下降 = 可能峰值
            if (self._last_val >= self.min_height and
                self._sample_idx - self._last_peak_pos >= self.min_distance):
                is_peak = True
                self._last_peak_pos = self._sample_idx
            self._rising = False
        
        self._last_val = value
        self._sample_idx += 1
        
        return (is_peak, self._last_val if is_peak else 0.0)
    
    def find_peaks(self, values: List[float]) -> List[Tuple[int, float]]:
        """批量找峰值，返回 (index, value) 列表"""
        peaks = []
        self.reset()
        
        for i, v in enumerate(values):
            is_peak, val = self.process(v)
            if is_peak:
                peaks.append((i, val))
        
        return peaks
    
    @property
    def memory_usage(self) -> int:
        return 64  # 固定几个变量


class CrossCorrelator:
    """
    互相关分析器
    - 用途：信号时延估计、模板匹配
    - 内存：O(N) 固定缓冲区
    - 计算：使用增量式，避免全量计算
    """
    
    def __init__(self, template: List[float]):
        self.template = template
        self._tpl_len = len(template)
        self._buffer = deque(maxlen=self._tpl_len)
        self._tpl_sum_sq = sum(t * t for t in template)
    
    def reset(self):
        self._buffer.clear()
    
    def process(self, value: float) -> float:
        """
        输入新样本，返回与模板的归一化互相关值
        范围：-1 ~ 1，越接近1相似度越高
        """
        self._buffer.append(value)
        
        if len(self._buffer) < self._tpl_len:
            return 0.0
        
        sig_sum = 0.0
        sig_sum_sq = 0.0
        cross_sum = 0.0
        
        for i in range(self._tpl_len):
            s = self._buffer[i]
            t = self.template[i]
            sig_sum += s
            sig_sum_sq += s * s
            cross_sum += s * t
        
        sig_mean = sig_sum / self._tpl_len
        tpl_mean = sum(self.template) / self._tpl_len
        
        numerator = cross_sum - self._tpl_len * sig_mean * tpl_mean
        denominator = math.sqrt(
            (sig_sum_sq - self._tpl_len * sig_mean * sig_mean) *
            (self._tpl_sum_sq - self._tpl_len * tpl_mean * tpl_mean)
        )
        
        if denominator < 1e-10:
            return 0.0
        
        return numerator / denominator
    
    @property
    def memory_usage(self) -> int:
        return self._tpl_len * 8 * 2


class MultiChannelProcessor:
    """
    多通道信号处理器
    - 统一管理多通道的滤波/检测
    - 每个通道独立状态，内存按通道预分配
    - 支持 1-32 通道
    """
    
    def __init__(self, num_channels: int = 8, 
                 ma_window: int = 8,
                 med_window: int = 5,
                 fir_taps: int = 21,
                 fir_cutoff: float = 1000.0,
                 sample_rate: float = 50000.0):
        
        self.num_channels = num_channels
        
        self.ma_filters = [MovingAverageFilter(ma_window) for _ in range(num_channels)]
        self.med_filters = [MedianFilter(med_window) for _ in range(num_channels)]
        self.fir_filters = [
            FIRFilter.design_lowpass(fir_cutoff, sample_rate, fir_taps)
            for _ in range(num_channels)
        ]
        self.peak_detectors = [PeakDetector() for _ in range(num_channels)]
    
    def process_ma(self, channel_values: List[float]) -> List[float]:
        """滑动平均滤波"""
        return [self.ma_filters[i].process(v) 
                for i, v in enumerate(channel_values[:self.num_channels])]
    
    def process_median(self, channel_values: List[float]) -> List[float]:
        """中值滤波"""
        return [self.med_filters[i].process(v) 
                for i, v in enumerate(channel_values[:self.num_channels])]
    
    def process_fir(self, channel_values: List[float]) -> List[float]:
        """FIR低通滤波"""
        return [self.fir_filters[i].process(v) 
                for i, v in enumerate(channel_values[:self.num_channels])]
    
    def detect_peaks(self, channel_values: List[float]) -> List[Tuple[bool, float]]:
        """峰值检测"""
        return [self.peak_detectors[i].process(v) 
                for i, v in enumerate(channel_values[:self.num_channels])]
    
    def get_memory_usage(self) -> dict:
        ma = sum(f.memory_usage for f in self.ma_filters)
        med = sum(f.memory_usage for f in self.med_filters)
        fir = sum(f.memory_usage for f in self.fir_filters)
        peak = sum(f.memory_usage for f in self.peak_detectors)
        
        return {
            "moving_average": ma,
            "median": med,
            "fir": fir,
            "peak_detect": peak,
            "total": ma + med + fir + peak,
        }
