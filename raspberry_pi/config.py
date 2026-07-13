#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 配置模块
"""

import os

# 系统版本
VERSION = "V0.1"

# 工作目录
WORK_DIR = "/home/pi/super_calc"
DATA_DIR = os.path.join(WORK_DIR, "data")
LOG_DIR = os.path.join(WORK_DIR, "logs")
CONFIG_DIR = os.path.join(WORK_DIR, "config")

# USB配置
USB_DEVICES = [
    "/dev/ttyACM0",  # Pico2 #0
    "/dev/ttyACM1",  # Pico2 #1
]
USB_BAUDRATE = 100000000  # 100Mbps (USB CDC)
USB_TIMEOUT = 1.0  # 秒

# 数据队列配置
DATA_QUEUE_SIZE = 100
STATUS_QUEUE_SIZE = 10
COMMAND_QUEUE_SIZE = 10

# 采样配置
ADC_CHANNELS = 64
ADC_RESOLUTION = 12
ADC_SAMPLE_RATE_MAX = 125000  # 125KSPS

DIGITAL_CHANNELS = 128
DIGITAL_SAMPLE_RATE_MAX = 100000000  # 100MSPS

# 破解配置
DEFAULT_KEY_LENGTH = 16
MAX_KEY_LENGTH = 32
CRACK_TIMEOUT = 300  # 秒

# 超频配置
PICO_DEFAULT_FREQ = 133  # MHz
PICO_OVERCLOCK_FREQ = 200  # MHz
PICO2_DEFAULT_FREQ = 150  # MHz
PICO2_OVERCLOCK_FREQ = 240  # MHz

# Web服务配置
WEB_HOST = "0.0.0.0"
WEB_PORT = 5000
WEB_SECRET_KEY = "super_calc_system_v0.1"

# 存储配置
STORAGE_PATH = "/home/pi/super_calc/data"
MAX_STORAGE_SIZE = 64 * 1024 * 1024 * 1024  # 64GB
DATA_FORMAT = "csv"  # csv 或 binary

# OLED显示配置
OLED_I2C_ADDR = 0x3C
OLED_WIDTH = 128
OLED_HEIGHT = 64

# GPIO配置
GPIO_BUTTON_MODE = 17      # 模式按钮
GPIO_BUTTON_FUNC = 18      # 功能按钮
GPIO_BUTTON_START = 19     # 启停按钮
GPIO_BUTTON_UP = 20        # 上键
GPIO_BUTTON_DOWN = 21      # 下键
GPIO_BUTTON_LEFT = 22      # 左键
GPIO_BUTTON_RIGHT = 23     # 右键
GPIO_LED_GREEN = 24        # 绿色LED
GPIO_LED_BLUE = 25         # 蓝色LED
GPIO_LED_RED = 26          # 红色LED

# 温度阈值
TEMP_WARNING = 60  # ℃
TEMP_CRITICAL = 70  # ℃

# 电池阈值
BATTERY_WARNING = 30  # %
BATTERY_CRITICAL = 20  # %

# FRAM配置
FRAM_I2C_ADDR = 0x50
FRAM_SIZE = 4096  # 4KB

# 日志配置
LOG_LEVEL = "INFO"
LOG_FORMAT = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
LOG_FILE = os.path.join(LOG_DIR, "super_calc.log")

# 多进程配置
PROCESS_PRIORITY = {
    "system_daemon": -20,     # 最高优先级
    "data_receiver": -10,
    "mode_scheduler": -5,
    "fault_handler": -5,
    "business_logic": 0,
    "hmi": 0,
    "storage_remote": 10,     # 最低优先级
}

# CPU核心绑定
CPU_AFFINITY = {
    "system_daemon": 0,
    "data_receiver": 1,
    "mode_scheduler": 1,
    "fault_handler": 1,
    "business_logic": 2,
    "hmi": 2,
    "storage_remote": 3,
}