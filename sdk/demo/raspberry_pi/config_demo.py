#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - Demo版本配置
单组验证：1片RP2350 + 8片RP2040
验证目标：信号采集分析、协议破解、硬件安全测试
适配树莓派Zero 2W（512MB RAM，单核USB）
"""

import os

# Demo版本标识
DEMO_VERSION = "V0.1 Demo"
IS_DEMO = True

# 运行平台（Zero 2W适配）
RUN_ON_ZERO2W = True

# ========== 系统规模配置 ==========
# Demo版本：1片Pico2 + 8片Pico（Zero 2W限制）
NUM_PICO2 = 1          # Pico2数量：1片（Zero 2W仅1个USB端口）
NUM_PICO = 8           # Pico数量：8片（Zero 2W内存限制）

# ADC通道配置（每片Pico 4路）
ADC_CHANNELS_PER_PICO = 4
ADC_TOTAL_CHANNELS = NUM_PICO * ADC_CHANNELS_PER_PICO  # 32路

# 数字通道配置（每片Pico 8路）
DIGITAL_CHANNELS_PER_PICO = 8
DIGITAL_TOTAL_CHANNELS = NUM_PICO * DIGITAL_CHANNELS_PER_PICO  # 64路

# ========== 硬件接口配置 ==========
# USB设备（Zero 2W只有1路）
USB_DEVICES = [
    "/dev/ttyACM0",  # Pico2 #0
]
USB_BAUDRATE = 100000000  # 100Mbps
USB_TIMEOUT = 1.0

# SPI配置（Pico2 → 8片Pico）
SPI_NUM_SLAVES = NUM_PICO  # 8个从设备
SPI_CS_BASE_PIN = 2       # CS起始引脚（GPIO2-9）
SPI_BAUDRATE = 20000000   # 20Mbps

# ========== 采样配置 ==========
# 模拟采样
ADC_SAMPLE_RATE_DEFAULT = 50000   # 默认50KSPS（Demo降频稳定）
ADC_SAMPLE_RATE_MAX = 100000      # Zero 2W限制最大采样率为100KSPS
ADC_RESOLUTION = 12               # 12bit

# 数字捕获
DIGITAL_SAMPLE_RATE_DEFAULT = 50000000  # 默认50MSPS（Demo降频）
DIGITAL_SAMPLE_RATE_MAX = 100000000     # 最大100MSPS

# ========== 破解配置（Zero 2W性能限制）==========
DEFAULT_KEY_LENGTH = 8        # Zero 2W缩短默认密钥长度
MAX_KEY_LENGTH = 12           # Zero 2W限制到12位，防止内存溢出
CRACK_TIMEOUT = 60            # Zero 2W超时1分钟
CRACK_CHARSET_DEFAULT = "0123456789abcdef"  # 十六进制字符集

# ========== 超频配置（Zero 2W限制）==========
PICO_DEFAULT_FREQ = 133    # MHz
PICO_OVERCLOCK_FREQ = 200  # MHz（仅MODE_BRUTEFORCE使用）
PICO2_DEFAULT_FREQ = 150   # MHz
PICO2_OVERCLOCK_FREQ = 240 # MHz（仅MODE_BRUTEFORCE使用）
OVERCLOCK_TIME_LIMIT = 300 # Zero 2W超频时间限制5分钟

# ========== 内存优化配置（Zero 2W）==========
DATA_QUEUE_SIZE = 2048     # 减少队列大小
QUEUE_MAXSIZE = 250        # 减少队列上限
WRITE_BUFFER_SIZE = 256 * 1024  # 256KB缓冲

# ========== Web服务配置（Zero 2W优化）==========
WEB_HOST = "0.0.0.0"
WEB_PORT = 5000
WEB_SECRET_KEY = "super_calc_demo_v0.1"
MAX_WEB_SOCKETS = 2        # Zero 2W限制WebSocket连接数

# ========== 存储配置（Zero 2W优化）==========
STORAGE_PATH = "/home/pi/super_calc_demo/data"
MAX_STORAGE_SIZE = 2 * 1024 * 1024 * 1024  # 2GB（Zero 2W减少）
DATA_FORMAT = "csv"

# ========== 日志配置 ==========
LOG_LEVEL = "INFO"  # Zero 2W使用INFO级别减少日志输出
LOG_DIR = "/home/pi/super_calc_demo/logs"
LOG_FILE = os.path.join(LOG_DIR, "super_calc_demo.log")

# ========== Demo验证功能开关（Zero 2W精简）==========
ENABLE_ANALOG_SAMPLE = True     # 模拟采样验证
ENABLE_DIGITAL_CAPTURE = True   # 数字捕获验证
ENABLE_CRACK_ENGINE = True      # 破解引擎验证
ENABLE_HARDWARE_TEST = True     # 硬件安全测试
ENABLE_FFT_ANALYSIS = False     # Zero 2W禁用FFT分析（内存和算力限制）
ENABLE_DATA_STORAGE = True      # 数据存储

# ========== 硬件安全测试项（Zero 2W精简）==========
HW_TEST_ITEMS = [
    "voltage_monitor",      # 电压监控
    "current_monitor",      # 电流监控
    "temperature_test",     # 温度测试
    "overclock_stability",  # 超频稳定性（简化）
]

# ========== Demo测试模式 ==========
# 可以设置为True，使用模拟数据快速验证Web和逻辑（无需硬件）
SIMULATION_MODE = False

# ========== Zero 2W温度保护（散热较差）==========
TEMP_WARNING = 55.0       # Zero 2W降低警告温度
TEMP_SHUTDOWN = 70.0      # Zero 2W降低关机温度