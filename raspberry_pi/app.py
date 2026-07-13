#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 主程序
Flask Web服务 + 多进程架构
"""

import os
import sys
import time
import json
import threading
import multiprocessing
from multiprocessing import Queue, Process
from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import logging

# 添加当前目录到路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from config import (
    VERSION, WEB_HOST, WEB_PORT, WEB_SECRET_KEY,
    DATA_QUEUE_SIZE, STATUS_QUEUE_SIZE
)
from data_receiver import create_data_receiver
from business_logic import create_business_logic
from storage_manager import create_storage_manager, setup_logging

# 设置日志
setup_logging()
logger = logging.getLogger("Main")

# Flask应用初始化
app = Flask(__name__)
app.config['SECRET_KEY'] = WEB_SECRET_KEY
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# 全局数据队列
data_queue = Queue(maxsize=DATA_QUEUE_SIZE)
status_queue = Queue(maxsize=STATUS_QUEUE_SIZE)
result_queue = Queue(maxsize=DATA_QUEUE_SIZE)
storage_queue = Queue(maxsize=DATA_QUEUE_SIZE)

# 全局状态变量
device_status = {
    "version": VERSION,
    "work_mode": "IDLE",
    "run_status": "Stop",
    "core_temp": 25,
    "battery": 100,
    "storage": 0,
    "pico_freq": 133,
    "pico2_freq": 150,
    "fault_info": "None"
}

sample_data = {
    "analog": [0] * 64,
    "digital": [0] * 128,
    "crack_progress": 0,
    "crack_result": ""
}

# 子模块实例
data_receiver = None
business_logic = None
storage_manager = None


# ==================== 页面路由 ====================

@app.route('/')
def index():
    """主页"""
    return render_template('index.html', status=device_status)


@app.route('/api/status')
def get_status():
    """获取设备状态"""
    return jsonify(device_status)


@app.route('/api/data')
def get_data():
    """获取采样数据"""
    return jsonify(sample_data)


@app.route('/api/config')
def get_config():
    """获取配置"""
    from config import *
    config_data = {
        "version": VERSION,
        "adc_channels": ADC_CHANNELS,
        "adc_resolution": ADC_RESOLUTION,
        "adc_sample_rate_max": ADC_SAMPLE_RATE_MAX,
        "digital_channels": DIGITAL_CHANNELS,
        "digital_sample_rate_max": DIGITAL_SAMPLE_RATE_MAX,
        "pico_default_freq": PICO_DEFAULT_FREQ,
        "pico_overclock_freq": PICO_OVERCLOCK_FREQ,
        "pico2_default_freq": PICO2_DEFAULT_FREQ,
        "pico2_overclock_freq": PICO2_OVERCLOCK_FREQ
    }
    return jsonify(config_data)


# ==================== WebSocket事件 ====================

@socketio.on('connect')
def handle_connect():
    """客户端连接"""
    logger.info(f"客户端连接: {request.sid}")
    emit('status_update', device_status)


@socketio.on('disconnect')
def handle_disconnect():
    """客户端断开"""
    logger.info(f"客户端断开: {request.sid}")


@socketio.on('start_sample')
def handle_start_sample():
    """启动采样"""
    logger.info("启动采样")
    
    # 发送命令到Pico2
    if data_receiver:
        data_receiver.send_command("/dev/ttyACM0", 0x01)  # 启动采样
    
    device_status["run_status"] = "Running"
    device_status["work_mode"] = "Sample"
    emit('status_update', device_status)


@socketio.on('stop_sample')
def handle_stop_sample():
    """停止采样"""
    logger.info("停止采样")
    
    # 发送命令到Pico2
    if data_receiver:
        data_receiver.send_command("/dev/ttyACM0", 0x02)  # 停止采样
    
    device_status["run_status"] = "Stop"
    emit('status_update', device_status)


@socketio.on('start_crack')
def handle_start_crack(data):
    """启动破解"""
    target_hash = data.get('target_hash', '')
    key_length = data.get('key_length', 16)
    
    logger.info(f"启动破解: 目标={target_hash}, 密钥长度={key_length}")
    
    if business_logic:
        business_logic.start_crack(target_hash, key_length)
    
    device_status["run_status"] = "Running"
    device_status["work_mode"] = "Decode"
    emit('status_update', device_status)


@socketio.on('stop_crack')
def handle_stop_crack():
    """停止破解"""
    logger.info("停止破解")
    
    if business_logic:
        business_logic.stop_crack()
    
    device_status["run_status"] = "Stop"
    emit('status_update', device_status)


@socketio.on('set_sample_rate')
def handle_set_sample_rate(data):
    """设置采样率"""
    rate = data.get('rate', 125000)
    logger.info(f"设置采样率: {rate}")
    
    # 发送命令到Pico2
    if data_receiver:
        params = struct.pack("<I", rate)
        data_receiver.send_command("/dev/ttyACM0", 0x03, params)


@socketio.on('set_overclock')
def handle_set_overclock(data):
    """设置超频"""
    mode = data.get('mode', 0)
    logger.info(f"设置超频模式: {mode}")
    
    # 发送命令到Pico2
    if data_receiver:
        params = struct.pack("<B", mode)
        data_receiver.send_command("/dev/ttyACM0", 0x05, params)
    
    # 更新状态
    if mode == 1:  # 超频模式
        device_status["pico_freq"] = 200
        device_status["pico2_freq"] = 240
    else:
        device_status["pico_freq"] = 133
        device_status["pico2_freq"] = 150
    
    emit('status_update', device_status)


# ==================== 后台任务 ====================

def update_status():
    """状态更新线程"""
    global device_status, sample_data
    
    while True:
        try:
            # 从状态队列读取
            if not status_queue.empty():
                new_status = status_queue.get()
                device_status.update(new_status)
                socketio.emit('status_update', device_status)
            
            # 从结果队列读取
            if not result_queue.empty():
                new_data = result_queue.get()
                
                if new_data.get("type") == "analog":
                    waveform = new_data.get("waveform", [])
                    if len(waveform) >= 64:
                        sample_data["analog"] = waveform[:64]
                
                elif new_data.get("type") == "crack":
                    sample_data["crack_progress"] = new_data.get("progress", 0)
                    sample_data["crack_result"] = new_data.get("result", "")
                
                socketio.emit('data_update', sample_data)
            
            # 获取破解进度
            if business_logic and device_status["work_mode"] == "Decode":
                crack_progress = business_logic.get_crack_progress()
                sample_data["crack_progress"] = crack_progress.get("progress", 0)
                sample_data["crack_result"] = crack_progress.get("result", "")
                
                if crack_progress.get("progress") % 10 == 0:
                    socketio.emit('data_update', sample_data)
            
            # 获取存储统计
            if storage_manager:
                storage_stats = storage_manager.get_statistics()
                device_status["storage"] = int(storage_stats.get("storage_usage", 0))
            
            time.sleep(0.1)
            
        except Exception as e:
            logger.error(f"状态更新异常: {e}")
            time.sleep(1)


# ==================== 主函数 ====================

def main():
    """主函数"""
    logger.info(f"超采集算系统 {VERSION} 启动")
    
    # 创建子模块
    global data_receiver, business_logic, storage_manager
    
    data_receiver = create_data_receiver(data_queue, status_queue)
    business_logic = create_business_logic(data_queue, result_queue)
    storage_manager = create_storage_manager(storage_queue)
    
    # 启动子模块
    data_receiver.start()
    business_logic.start()
    storage_manager.start()
    
    # 启动状态更新线程
    status_thread = threading.Thread(target=update_status, daemon=True)
    status_thread.start()
    
    # 启动Flask服务
    logger.info(f"Web服务启动: http://{WEB_HOST}:{WEB_PORT}")
    socketio.run(app, host=WEB_HOST, port=WEB_PORT, debug=False, allow_unsafe_werkzeug=True)


if __name__ == '__main__':
    main()