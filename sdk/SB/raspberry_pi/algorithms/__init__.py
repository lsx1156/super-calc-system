#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
算法库模块
- 模拟信号处理算法
- 数字信号处理算法
- 密码破解算法
"""

from .analog_algorithms import (
    MovingAverageFilter,
    MedianFilter,
    FIRFilter,
    PeakDetector,
    CrossCorrelator,
    MultiChannelProcessor,
)

from .digital_algorithms import (
    EdgeDetector,
    UARTDecoder,
    SPIDecoder,
    I2CDecoder,
    PulseWidthMeter,
    ManchesterDecoder,
    DigitalChannelProcessor,
)

from .crack_algorithms import (
    SHA256,
    MaskGenerator,
    DictionaryAttack,
    TaskScheduler,
    CrackEngine,
)

__all__ = [
    'MovingAverageFilter',
    'MedianFilter',
    'FIRFilter',
    'PeakDetector',
    'CrossCorrelator',
    'MultiChannelProcessor',
    'EdgeDetector',
    'UARTDecoder',
    'SPIDecoder',
    'I2CDecoder',
    'PulseWidthMeter',
    'ManchesterDecoder',
    'DigitalChannelProcessor',
    'SHA256',
    'MaskGenerator',
    'DictionaryAttack',
    'TaskScheduler',
    'CrackEngine',
]
