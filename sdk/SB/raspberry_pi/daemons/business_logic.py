#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
业务逻辑进程 (business_logic.py)
- 模拟信号处理：数字滤波、零点校准、增益校准
- 数字信号处理：协议解码、时序分析、电平转换
- 暴力破解：密钥匹配、字典攻击、并行算力调度
- 数据格式化：CSV/二进制封装
Zero 2W优化：减少滤波器数量，禁用FFT，降低内存占用
"""

import os
import sys
import time
import struct
import threading
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.logger import get_logger
from core.status_manager import status_mgr
from core.config import ADC_VREF, WorkMode, MAX_PICO_PER_PICO2, MAX_PICO2_COUNT, DEFAULT_SAMPLE_RATE, RUN_ON_ZERO2W
from algorithms.analog_algorithms import (
    MovingAverageFilter,
    MedianFilter,
    FIRFilter,
    PeakDetector,
)
from algorithms.digital_algorithms import (
    EdgeDetector,
    UARTDecoder,
    PulseWidthMeter,
)

logger = get_logger("BusinessLogic")


class SignalProcessor:
    """信号处理器（Zero 2W优化版）"""
    
    def __init__(self, filter_enabled: bool = True, sample_rate: int = DEFAULT_SAMPLE_RATE):
        self._running = False
        self._input_queue = None
        self._output_queue = None
        self._storage_queue = None
        self._filter_enabled = filter_enabled
        self._sample_rate = sample_rate
        
        if RUN_ON_ZERO2W:
            self._zero_offset = np.zeros(16)
            self._gain_calibration = np.ones(16)
        else:
            self._zero_offset = np.zeros(32)
            self._gain_calibration = np.ones(32)
        
        self._ma_filters = []
        self._med_filters = []
        self._fir_filters = []
        self._peak_detectors = []
        self._edge_detectors = []
        self._uart_decoders = []
        self._pulse_meters = []
        
        if filter_enabled:
            num_ch = 32 if RUN_ON_ZERO2W else 64
            self._init_filters(num_ch)
    
    def _init_filters(self, num_channels: int):
        """初始化滤波器（按需创建，控制内存）"""
        self._ma_filters = [MovingAverageFilter(window_size=8) for _ in range(num_channels)]
        self._med_filters = [MedianFilter(window_size=5) for _ in range(num_channels)]
        self._fir_filters = [
            FIRFilter.design_lowpass(1000, self._sample_rate, 21)
            for _ in range(num_channels)
        ]
        self._peak_detectors = [PeakDetector(min_height=0.1, min_distance=10) for _ in range(num_channels)]
        self._edge_detectors = [EdgeDetector(debounce_samples=3) for _ in range(num_channels)]
        self._uart_decoders = [UARTDecoder(sample_rate=self._sample_rate) for _ in range(num_channels)]
        self._pulse_meters = [PulseWidthMeter(sample_rate=self._sample_rate) for _ in range(num_channels)]
        
        total_mem = num_channels * (8 * 8 + 5 * 8 + 21 * 16 + 64 + 32 + 256 + 64)
        logger.info(f"滤波器初始化完成: {num_channels} 通道, 预估内存 {total_mem/1024:.1f} KB")
    
    def set_queues(self, input_q, output_q, storage_q):
        self._input_queue = input_q
        self._output_queue = output_q
        self._storage_queue = storage_q
    
    def start(self):
        logger.info("业务逻辑进程启动")
        self._running = True
        
        threading.Thread(target=self._process_loop, daemon=True).start()
        
        while self._running:
            time.sleep(1)
    
    def _process_loop(self):
        """处理循环"""
        while self._running:
            try:
                if self._input_queue is None or self._input_queue.empty():
                    time.sleep(0.001)
                    continue
                
                packet = self._input_queue.get(timeout=0.1)
                result = self._process_packet(packet)
                
                if result and self._output_queue and not self._output_queue.full():
                    self._output_queue.put(result)
                
                if result and self._storage_queue and not self._storage_queue.full():
                    self._storage_queue.put(result)
                
            except Exception as e:
                logger.error(f"处理异常: {e}")
    
    def _process_packet(self, packet):
        """处理单个数据包"""
        data_type = packet.get("data_type", 0)
        
        if data_type == 0x10:
            return self._process_aggregated(packet)
        elif data_type == 0x01:
            return self._process_analog(packet)
        elif data_type == 0x02:
            return self._process_digital(packet)
        
        return packet
    
    def _process_aggregated(self, packet):
        """处理聚合数据"""
        data = packet.get("data", b"")
        if len(data) < 3:
            return packet
        
        node_count = data[1]
        offset = 3
        analog_vals = []
        digital_vals = []
        
        for i in range(node_count):
            if offset + 3 > len(data):
                break
            
            node_id = data[offset]
            data_len = data[offset+1] | (data[offset+2] << 8)
            offset += 3
            
            if offset + data_len > len(data):
                break
            
            node_data = data[offset:offset+data_len]
            offset += data_len
            
            if len(node_data) > 0 and node_data[0] == 0x01:
                for ch in range(4):
                    if 1 + ch*2 + 1 < len(node_data):
                        val = struct.unpack_from("<H", node_data, 1+ch*2)[0]
                        voltage = val * ADC_VREF / 4095.0
                        analog_vals.append(voltage)
        
        result = dict(packet)
        result["analog_values"] = analog_vals
        result["digital_values"] = digital_vals
        
        if self._filter_enabled and len(analog_vals) > 0:
            result["filtered_ma"] = self._apply_moving_average(analog_vals)
            result["filtered_median"] = self._apply_median(analog_vals)
            result["filtered_fir"] = self._apply_fir(analog_vals)
            result["peaks"] = self._detect_peaks(result["filtered_fir"])
        
        if not RUN_ON_ZERO2W and len(analog_vals) > 8:
            self._compute_fft(result["filtered_fir"] if self._filter_enabled else analog_vals, result)
        
        status_mgr.update_sample(
            analog_channels=len(analog_vals),
            total_samples=status_mgr.get_sample()["total_samples"] + 1,
        )
        
        return result
    
    def _process_analog(self, packet):
        """处理模拟数据"""
        return packet
    
    def _process_digital(self, packet):
        """处理数字数据"""
        data = packet.get("data", b"")
        if not data:
            return packet
        
        result = dict(packet)
        
        digital_data = list(data)
        result["digital_samples"] = digital_data
        
        if self._filter_enabled:
            edges = []
            uart_bytes = []
            pulse_info = []
            
            max_ch = 8 if RUN_ON_ZERO2W else min(8, len(digital_data))
            
            for ch_idx in range(max_ch):
                sample = digital_data[ch_idx]
                
                if ch_idx < len(self._edge_detectors):
                    edge, level = self._edge_detectors[ch_idx].process(sample)
                    if edge > 0:
                        edges.append({"channel": ch_idx, "edge": edge})
                
                if ch_idx < len(self._uart_decoders):
                    byte = self._uart_decoders[ch_idx].process(sample)
                    if byte is not None:
                        uart_bytes.append({"channel": ch_idx, "byte": byte})
                
                if ch_idx < len(self._pulse_meters):
                    info = self._pulse_meters[ch_idx].process(sample)
                    if info:
                        pulse_info.append({"channel": ch_idx, **info})
            
            result["edges"] = edges
            result["uart_decoded"] = uart_bytes
            result["pulse_info"] = pulse_info
        
        return result
    
    def _apply_moving_average(self, values):
        """滑动平均滤波"""
        result = []
        for i, v in enumerate(values):
            if i < len(self._ma_filters):
                result.append(self._ma_filters[i].process(v))
            else:
                result.append(v)
        return result
    
    def _apply_median(self, values):
        """中值滤波"""
        result = []
        for i, v in enumerate(values):
            if i < len(self._med_filters):
                result.append(self._med_filters[i].process(v))
            else:
                result.append(v)
        return result
    
    def _apply_fir(self, values):
        """FIR低通滤波"""
        result = []
        for i, v in enumerate(values):
            if i < len(self._fir_filters):
                result.append(self._fir_filters[i].process(v))
            else:
                result.append(v)
        return result
    
    def _detect_peaks(self, values):
        """峰值检测"""
        peaks = []
        for i, v in enumerate(values):
            if i < len(self._peak_detectors):
                is_peak, val = self._peak_detectors[i].process(v)
                if is_peak:
                    peaks.append({"channel": i, "value": val})
        return peaks
    
    def _compute_fft(self, values, result):
        """计算FFT（仅非Zero 2W）"""
        try:
            if len(values) < 16:
                return
            
            samples = np.array(values[-64:])
            fft_vals = np.fft.fft(samples)
            mag = np.abs(fft_vals)
            
            n = len(mag) // 2
            result["fft_freq"] = list(range(n))
            result["fft_mag"] = mag[:n].tolist()
            
            if n > 0:
                peak_idx = np.argmax(mag[:n])
                result["peak_freq"] = float(peak_idx)
        except:
            pass
    
    def stop(self):
        self._running = False


def main():
    processor = SignalProcessor()
    processor.start()


if __name__ == '__main__':
    main()