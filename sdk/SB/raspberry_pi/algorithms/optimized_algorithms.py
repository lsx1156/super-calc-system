#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
向量化优化算法库 (optimized_algorithms.py)
- 使用numpy向量化批量处理，提升浮点计算速度10-100倍
- 多通道并行处理
- 内存可控，固定缓冲区大小
"""

import numpy as np
from typing import List, Tuple, Optional
from collections import deque


class VectorizedMovingAverage:
    """向量化滑动平均滤波 - 批量处理提升10-50倍速度"""
    
    def __init__(self, num_channels: int = 1, window_size: int = 8):
        self.num_channels = num_channels
        self.window_size = window_size
        self._kernel = np.ones(window_size) / window_size
        self._overlap = np.zeros((num_channels, window_size - 1), dtype=np.float64)
    
    def reset(self):
        self._overlap.fill(0)
    
    def process_batch(self, data: np.ndarray) -> np.ndarray:
        """
        批量处理
        data: (num_channels, n_samples) 或 (n_samples,)
        """
        if data.ndim == 1:
            data = data.reshape(1, -1)
        
        n = data.shape[1]
        result = np.zeros_like(data)
        
        for ch in range(min(self.num_channels, data.shape[0])):
            combined = np.concatenate([self._overlap[ch], data[ch]])
            filtered = np.convolve(combined, self._kernel, mode='valid')
            result[ch] = filtered[:n]
            
            if self.window_size - 1 > 0:
                self._overlap[ch] = data[ch, -(self.window_size - 1):] if n >= self.window_size - 1 else np.concatenate([self._overlap[ch, n:], data[ch]])
        
        return result[0] if self.num_channels == 1 and result.shape[0] == 1 else result


class VectorizedFIR:
    """向量化FIR滤波 - 使用numpy卷积加速"""
    
    def __init__(self, coefficients: np.ndarray, num_channels: int = 1):
        self.num_channels = num_channels
        self.coeffs = np.array(coefficients, dtype=np.float64)
        self._order = len(self.coeffs) - 1
        self._overlap = np.zeros((num_channels, self._order), dtype=np.float64)
    
    def reset(self):
        self._overlap.fill(0)
    
    @classmethod
    def design_lowpass(cls, cutoff: float, sample_rate: float, numtaps: int = 31,
                       num_channels: int = 1) -> 'VectorizedFIR':
        """设计低通FIR滤波器"""
        nyq = sample_rate / 2.0
        normalized_cutoff = cutoff / nyq
        
        if numtaps % 2 == 0:
            numtaps += 1
        
        taps = np.arange(numtaps) - (numtaps - 1) / 2.0
        
        h = np.sinc(2 * normalized_cutoff * taps) * (2 * normalized_cutoff)
        
        window = np.hamming(numtaps)
        h = h * window
        
        h = h / np.sum(h)
        
        return cls(h, num_channels)
    
    def process_batch(self, data: np.ndarray) -> np.ndarray:
        """批量FIR滤波"""
        if data.ndim == 1:
            data = data.reshape(1, -1)
        
        n = data.shape[1]
        result = np.zeros_like(data)
        
        for ch in range(min(self.num_channels, data.shape[0])):
            combined = np.concatenate([self._overlap[ch], data[ch]])
            filtered = np.convolve(combined, self.coeffs, mode='valid')
            result[ch] = filtered[:n]
            
            if self._order > 0:
                self._overlap[ch] = data[ch, -self._order:] if n >= self._order else np.concatenate([self._overlap[ch, n:], data[ch]])
        
        return result[0] if self.num_channels == 1 and result.shape[0] == 1 else result


class VectorizedPeakDetector:
    """向量化峰值检测 - 批量处理"""
    
    def __init__(self, min_height: float = 0.1, min_distance: int = 10):
        self.min_height = min_height
        self.min_distance = min_distance
    
    def detect_batch(self, data: np.ndarray) -> List[np.ndarray]:
        """
        批量峰值检测
        返回每个通道的峰值索引数组
        """
        if data.ndim == 1:
            data = data.reshape(1, -1)
        
        results = []
        for ch in range(data.shape[0]):
            peaks = self._detect_single(data[ch])
            results.append(peaks)
        
        return results
    
    def _detect_single(self, x: np.ndarray) -> np.ndarray:
        """单通道峰值检测"""
        if len(x) < 3:
            return np.array([], dtype=int)
        
        dx = np.diff(x)
        peaks = np.where((dx[:-1] > 0) & (dx[1:] < 0))[0] + 1
        
        if self.min_height > 0:
            peaks = peaks[x[peaks] >= self.min_height]
        
        if self.min_distance > 1 and len(peaks) > 1:
            keep = np.ones(len(peaks), dtype=bool)
            for i in range(1, len(peaks)):
                if peaks[i] - peaks[i - 1] < self.min_distance:
                    if x[peaks[i]] > x[peaks[i - 1]]:
                        keep[i - 1] = False
                    else:
                        keep[i] = False
            peaks = peaks[keep]
        
        return peaks


class SignalFeatureExtractor:
    """信号特征提取器 - 用于自动识别"""
    
    @staticmethod
    def extract_features(signal: np.ndarray, sample_rate: float = 1.0) -> dict:
        """
        提取信号统计特征
        返回特征字典
        """
        if len(signal) == 0:
            return {}
        
        sig = np.array(signal, dtype=np.float64)
        
        mean = np.mean(sig)
        std = np.std(sig)
        rms = np.sqrt(np.mean(sig ** 2))
        peak = np.max(np.abs(sig))
        crest_factor = peak / rms if rms > 1e-10 else 0
        
        sig_ac = sig - mean
        rms_ac = np.sqrt(np.mean(sig_ac ** 2))
        peak_ac = np.max(np.abs(sig_ac))
        crest_factor_ac = peak_ac / rms_ac if rms_ac > 1e-10 else 0
        
        skewness = np.mean(((sig - mean) / std) ** 3) if std > 1e-10 else 0
        kurtosis = np.mean(((sig - mean) / std) ** 4) if std > 1e-10 else 0
        
        # 频域特征
        n = len(sig)
        fft_vals = np.fft.rfft(sig - mean)
        fft_mag = np.abs(fft_vals)
        freqs = np.fft.rfftfreq(n, 1.0 / sample_rate) if sample_rate > 0 else np.arange(len(fft_mag))
        
        if len(fft_mag) > 1:
            dominant_freq_idx = np.argmax(fft_mag[1:]) + 1
            dominant_freq = freqs[dominant_freq_idx] if sample_rate > 0 else dominant_freq_idx
            spectral_centroid = np.sum(freqs * fft_mag) / np.sum(fft_mag) if np.sum(fft_mag) > 1e-10 else 0
        else:
            dominant_freq = 0
            spectral_centroid = 0
        
        # 过零率
        zero_crossings = np.sum(np.diff(np.signbit(sig))) / len(sig)
        
        # 周期估计（自相关）
        if len(sig) > 32 and std > 1e-10:
            sig_norm = (sig - mean) / std
            corr = np.correlate(sig_norm, sig_norm, mode='full')
            corr = corr[len(corr) // 2:]
            peaks = np.where((corr[1:-1] > corr[:-2]) & (corr[1:-1] > corr[2:]))[0] + 1
            period = peaks[0] if len(peaks) > 0 and peaks[0] > 5 else 0
        else:
            period = 0
        
        return {
            'mean': float(mean),
            'std': float(std),
            'rms': float(rms),
            'peak': float(peak),
            'crest_factor': float(crest_factor),
            'crest_factor_ac': float(crest_factor_ac),
            'rms_ac': float(rms_ac),
            'peak_ac': float(peak_ac),
            'skewness': float(skewness),
            'kurtosis': float(kurtosis),
            'dominant_freq': float(dominant_freq),
            'spectral_centroid': float(spectral_centroid),
            'zero_crossing_rate': float(zero_crossings),
            'period_samples': int(period),
            'snr_estimate': float(20 * np.log10(rms / (std + 1e-10))) if std > 1e-10 else 0,
        }


class SignalAutoIdentifier:
    """
    模拟信号自动识别器
    基于统计特征自动识别信号类型：
    - 直流信号 (DC)
    - 正弦波 (Sine) - 峭度≈1.5, 波峰因数≈1.414
    - 方波 (Square) - 峭度≈1.0, 波峰因数≈1.0
    - 三角波 (Triangle) - 峭度≈1.8, 波峰因数≈1.732
    - 噪声 (Noise) - 峭度≈3.0(高斯), 无明显周期
    - 脉冲 (Pulse) - 高波峰因数
    """
    
    SIGNAL_TYPES = ['dc', 'sine', 'square', 'triangle', 'noise', 'pulse', 'unknown']
    
    def __init__(self, sample_rate: float = 10000.0, threshold: float = 0.35):
        self.sample_rate = sample_rate
        self.threshold = threshold
        self.extractor = SignalFeatureExtractor()
    
    def identify(self, signal: np.ndarray) -> Tuple[str, float, dict]:
        """
        识别信号类型
        返回: (信号类型, 置信度, 特征字典)
        """
        features = self.extractor.extract_features(signal, self.sample_rate)
        
        std = features.get('std', 0)
        mean = features.get('mean', 0)
        peak = features.get('peak', 0)
        
        if std < 0.001:
            return 'dc', 0.95, features
        
        kurtosis = features.get('kurtosis', 0)
        crest_factor = features.get('crest_factor_ac', features.get('crest_factor', 0))
        period = features.get('period_samples', 0)
        zero_crossing = features.get('zero_crossing_rate', 0)
        
        scores = {}
        
        sine_kurt = max(0, 1.0 - abs(kurtosis - 1.5) / 1.5)
        sine_crest = max(0, 1.0 - abs(crest_factor - 1.414) / 0.8)
        scores['sine'] = 0.5 * sine_kurt + 0.5 * sine_crest
        
        square_kurt = max(0, 1.0 - abs(kurtosis - 1.0) / 1.5)
        square_crest = max(0, 1.0 - abs(crest_factor - 1.0) / 0.8)
        scores['square'] = 0.5 * square_kurt + 0.5 * square_crest
        
        triangle_kurt = max(0, 1.0 - abs(kurtosis - 1.8) / 1.5)
        triangle_crest = max(0, 1.0 - abs(crest_factor - 1.732) / 0.8)
        scores['triangle'] = 0.5 * triangle_kurt + 0.5 * triangle_crest
        
        noise_period = 1.0 if period == 0 else max(0, 1.0 - (period / 200.0))
        noise_kurt = max(0, 1.0 - abs(kurtosis - 3.0) / 3.0)
        scores['noise'] = 0.6 * noise_period + 0.4 * noise_kurt
        
        pulse_crest = min(1.0, max(0, (crest_factor - 2.0) / 3.0))
        scores['pulse'] = pulse_crest
        
        scores['dc'] = max(0, 1.0 - std / (peak * 0.5 + 0.01))
        
        best_type = max(scores, key=scores.get)
        best_score = scores[best_type]
        
        sorted_scores = sorted(scores.values(), reverse=True)
        if len(sorted_scores) >= 2 and sorted_scores[1] > 0:
            margin = (sorted_scores[0] - sorted_scores[1]) / max(sorted_scores[0], 0.01)
            confidence = min(0.95, 0.3 + 0.7 * margin)
        else:
            confidence = best_score * 0.8
        
        if best_score < 0.3 or confidence < self.threshold:
            best_type = 'unknown'
            confidence = max(0.1, 1.0 - best_score)
        
        return best_type, min(0.99, confidence), features
    
    def identify_batch(self, signals: List[np.ndarray]) -> List[Tuple[str, float, dict]]:
        """批量识别多通道信号"""
        return [self.identify(sig) for sig in signals]


class HashAutoIdentifier:
    """
    哈希类型自动识别器
    根据哈希值长度和格式自动识别哈希算法
    """
    
    HASH_TYPES = {
        32: 'md5',
        40: 'sha1',
        56: 'sha224',
        64: 'sha256',
        96: 'sha384',
        128: 'sha512',
    }
    
    @staticmethod
    def identify(hash_str: str) -> Tuple[str, float]:
        """
        识别哈希类型
        返回: (哈希类型, 置信度)
        """
        if not hash_str or not isinstance(hash_str, str):
            return 'unknown', 0.0
        
        h = hash_str.strip().lower()
        
        if '$' in h:
            if h.startswith('$2'):
                return 'bcrypt', 0.9
            elif h.startswith('$pbkdf2'):
                return 'pbkdf2', 0.9
            elif h.startswith('$argon2'):
                return 'argon2', 0.9
            elif h.startswith('$5$'):
                return 'sha256_crypt', 0.9
            elif h.startswith('$6$'):
                return 'sha512_crypt', 0.9
        
        hex_len = len(h)
        
        if all(c in '0123456789abcdef' for c in h):
            if hex_len in HashAutoIdentifier.HASH_TYPES:
                return HashAutoIdentifier.HASH_TYPES[hex_len], 0.85
        
        if all(c in '0123456789abcdefghijklmnopqrstuv' for c in h.lower()):
            if hex_len == 40:
                return 'sha1', 0.5
        
        return 'unknown', 0.1


class ProtocolAutoIdentifier:
    """
    数字协议自动识别器
    自动识别UART/SPI/I2C等串行协议
    """
    
    PROTOCOL_TYPES = ['uart', 'spi', 'i2c', 'gpio', 'unknown']
    
    def __init__(self, sample_rate: float = 1000000.0):
        self.sample_rate = sample_rate
    
    def estimate_baudrate(self, signal: np.ndarray) -> Tuple[float, float]:
        """
        估计波特率
        返回: (波特率, 置信度)
        """
        if len(signal) < 10:
            return 0, 0
        
        transitions = np.where(np.diff(signal))[0]
        
        if len(transitions) < 2:
            return 0, 0
        
        intervals = np.diff(transitions)
        
        if len(intervals) == 0:
            return 0, 0
        
        min_interval = np.min(intervals)
        median_interval = np.median(intervals)
        
        if min_interval < 2:
            return 0, 0
        
        baudrate = self.sample_rate / min_interval
        
        confidence = min(1.0, 1.0 - abs(min_interval - median_interval / 8) / (median_interval / 8)) if median_interval > 0 else 0.5
        
        return baudrate, max(0.1, confidence)
    
    def identify(self, scl_signal: Optional[np.ndarray] = None,
                 sda_signal: Optional[np.ndarray] = None,
                 mosi_signal: Optional[np.ndarray] = None,
                 miso_signal: Optional[np.ndarray] = None,
                 rx_signal: Optional[np.ndarray] = None) -> Tuple[str, float, dict]:
        """
        识别协议类型
        返回: (协议类型, 置信度, 详细信息)
        """
        info = {}
        scores = {}
        
        has_clock = scl_signal is not None and len(scl_signal) > 0
        has_data = sda_signal is not None and len(sda_signal) > 0
        has_mosi = mosi_signal is not None and len(mosi_signal) > 0
        has_miso = miso_signal is not None and len(miso_signal) > 0
        has_single = rx_signal is not None and len(rx_signal) > 0
        
        if has_clock and has_data and not has_mosi and not has_miso:
            scl = np.array(scl_signal)
            sda = np.array(sda_signal)
            
            scl_edges = np.sum(np.diff(scl) != 0)
            sda_edges = np.sum(np.diff(sda) != 0)
            
            scl_freq = 0
            if scl_edges > 2:
                transitions = np.where(np.diff(scl))[0]
                if len(transitions) > 1:
                    avg_period = np.mean(np.diff(transitions)) * 2
                    scl_freq = self.sample_rate / avg_period if avg_period > 0 else 0
            
            i2c_score = min(1.0, scl_edges / 20.0) * min(1.0, sda_edges / 10.0)
            scores['i2c'] = i2c_score
            info['i2c_clock_freq'] = scl_freq
        
        if has_clock and has_mosi:
            spi_score = 0.7
            if has_miso:
                spi_score += 0.2
            scores['spi'] = min(1.0, spi_score)
        
        if has_single and not has_clock:
            rx = np.array(rx_signal)
            
            transitions = np.sum(np.diff(rx) != 0)
            baudrate, baud_conf = self.estimate_baudrate(rx)
            
            if baudrate > 0:
                uart_score = baud_conf * min(1.0, transitions / 20.0)
                scores['uart'] = uart_score
                info['estimated_baudrate'] = baudrate
        
        if not scores:
            scores['gpio'] = 0.5
            scores['unknown'] = 0.5
        else:
            scores['gpio'] = 0.1
            scores['unknown'] = 0.05
        
        best = max(scores, key=scores.get)
        best_score = scores[best]
        
        total = sum(scores.values())
        confidence = best_score / total if total > 0 else 0
        
        if confidence < 0.5:
            best = 'unknown'
        
        return best, confidence, info
