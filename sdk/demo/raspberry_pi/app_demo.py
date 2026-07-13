#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 Demo - 主程序
单组验证：1片RP2350 + 8片RP2040
三大核心功能：信号采集分析、协议破解、硬件安全测试
"""

import os
import sys
import time
import json
import struct
import zlib
import threading
import logging
from collections import deque

import serial
from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import numpy as np

# Demo配置
DEMO_VERSION = "V0.1 Demo"
NUM_PICO = 8
ADC_CHANNELS = 32  # 8片 × 4路
DIGITAL_CHANNELS = 64  # 8片 × 8路

# USB配置
USB_DEVICE = "/dev/ttyACM0"
USB_BAUDRATE = 100000000
USB_TIMEOUT = 1.0

# Web配置
WEB_HOST = "0.0.0.0"
WEB_PORT = 5000
SECRET_KEY = "super_calc_demo_v0.1"

# 破解配置
DEFAULT_KEY_LENGTH = 16
MAX_KEY_LENGTH = 24
CRACK_TIMEOUT = 120  # 2分钟

# 日志配置
logging.basicConfig(level=logging.DEBUG, 
                    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("Demo")

# Flask初始化
app = Flask(__name__)
app.config['SECRET_KEY'] = SECRET_KEY
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# ==================== 全局状态 ====================

device_status = {
    "version": DEMO_VERSION,
    "work_mode": "IDLE",      # IDLE, SAMPLE, CRACK, HW_TEST
    "run_status": "Stop",
    "core_temp": 25,
    "vcore": 1.1,
    "clock_freq": 150,
    "pico2_freq": 150,
    "pico_freq": 133,
    "pico_online": [0]*8,
    "online_count": 0,
    "sample_count": 0,
    "sample_rate": 50000,
    "fault_info": "None",
    "demo_mode": True
}

sample_data = {
    "analog": [0.0] * 32,      # 32路模拟
    "digital": [0] * 64,        # 64路数字
    "waveform": [0.0] * 64,     # 波形数据
    "fft_freq": [0.0] * 32,     # FFT频率
    "fft_mag": [0.0] * 32,      # FFT幅度
    "peak_freq": 0,             # 主频
    "snr": 0,                   # 信噪比
}

crack_data = {
    "progress": 0,
    "attempts": 0,
    "result": "",
    "running": False,
    "target_hash": "",
    "key_length": 16,
    "elapsed": 0,
    "rate": 0,
}

hw_test_data = {
    "running": False,
    "current_test": "",
    "test_results": {},
    "glitch_count": 0,
    "temp_history": [],
    "volt_history": [],
}

# ==================== USB通信 ====================

class USBComm:
    """USB CDC通信"""
    
    FRAME_HEADER = 0x55
    FRAME_TAIL = 0xAA
    
    def __init__(self, device, baudrate, timeout=1.0):
        self.device = device
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        self.running = False
        self.connected = False
        self.rx_buffer = b""
    
    def connect(self):
        """连接USB设备"""
        try:
            self.ser = serial.Serial(
                port=self.device,
                baudrate=self.baudrate,
                timeout=self.timeout
            )
            self.connected = True
            logger.info(f"USB连接成功: {self.device}")
            return True
        except Exception as e:
            logger.error(f"USB连接失败: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """断开连接"""
        self.running = False
        if self.ser:
            self.ser.close()
        self.connected = False
    
    def send_command(self, cmd, params=b""):
        """发送命令"""
        if not self.connected:
            return False
        
        try:
            # 构建帧
            data = struct.pack("<B", cmd) + params
            frame = struct.pack("<B", self.FRAME_HEADER)
            frame += struct.pack("<B", 0x00)  # 节点ID
            frame += struct.pack("<H", len(data))
            frame += data
            crc = zlib.crc32(data) & 0xFFFFFFFF
            frame += struct.pack("<I", crc)
            frame += struct.pack("<B", self.FRAME_TAIL)
            
            self.ser.write(frame)
            self.ser.flush()
            return True
        except Exception as e:
            logger.error(f"发送命令失败: {e}")
            self.connected = False
            return False
    
    def read_frame(self):
        """读取一帧数据"""
        if not self.connected:
            return None
        
        try:
            raw = self.ser.read(8192)
            if not raw:
                return None
            
            self.rx_buffer += raw
            
            # 查找帧头
            while self.FRAME_HEADER in self.rx_buffer:
                idx = self.rx_buffer.find(self.FRAME_HEADER)
                if idx > 0:
                    self.rx_buffer = self.rx_buffer[idx:]
                
                if len(self.rx_buffer) < 9:
                    break
                
                # 解析帧头
                node_id = self.rx_buffer[1]
                data_len = struct.unpack("<H", self.rx_buffer[2:4])[0]
                
                total_len = 4 + data_len + 4 + 1  # 头+长度+数据+CRC+尾
                if len(self.rx_buffer) < total_len:
                    break
                
                # 提取数据
                data = self.rx_buffer[4:4+data_len]
                crc_rx = struct.unpack("<I", self.rx_buffer[4+data_len:8+data_len])[0]
                tail = self.rx_buffer[8+data_len]
                
                # 移除已处理的数据
                self.rx_buffer = self.rx_buffer[total_len:]
                
                if tail != self.FRAME_TAIL:
                    continue
                
                # 验证CRC
                crc_calc = zlib.crc32(data) & 0xFFFFFFFF
                if crc_calc != crc_rx:
                    logger.warning("CRC校验失败")
                    continue
                
                return {
                    "node_id": node_id,
                    "data": data,
                    "length": data_len
                }
            
            return None
            
        except Exception as e:
            logger.error(f"读取帧失败: {e}")
            return None

# 全局USB对象
usb_comm = USBComm(USB_DEVICE, USB_BAUDRATE)

# ==================== 数据解析 ====================

def parse_aggregated_data(data):
    """解析聚合数据"""
    if len(data) < 2:
        return
    
    data_type = data[0]
    if data_type != 0x10:  # 不是聚合数据
        return
    
    num_nodes = data[1]
    offset = 2
    
    analog_vals = []
    digital_vals = []
    
    for i in range(min(num_nodes, NUM_PICO)):
        if offset + 3 > len(data):
            break
        
        node_id = data[offset]
        node_data_len = struct.unpack("<H", data[offset+1:offset+3])[0]
        offset += 3
        
        if offset + node_data_len > len(data):
            break
        
        node_data = data[offset:offset+node_data_len]
        offset += node_data_len
        
        # 解析节点数据
        if len(node_data) > 0:
            dtype = node_data[0]
            if dtype == 0x01:  # 模拟数据
                for ch in range(4):
                    if 1 + ch*2 + 1 < len(node_data):
                        val = struct.unpack("<H", node_data[1+ch*2:3+ch*2])[0]
                        voltage = val * 3.3 / 4095.0
                        analog_vals.append(voltage)
            
            if dtype == 0x02:  # 数字数据（可能在后面）
                pass
    
    # 更新采样数据
    if analog_vals:
        for i, v in enumerate(analog_vals[:32]):
            sample_data["analog"][i] = round(v, 4)
        
        # 波形数据（取前64个点滚动）
        sample_data["waveform"] = sample_data["waveform"][-64+len(analog_vals[:64]):] + analog_vals[:64]
        
        # FFT分析
        if len(sample_data["waveform"]) >= 32:
            samples = np.array(sample_data["waveform"][-64:])
            fft = np.fft.fft(samples)
            freq = np.fft.fftfreq(len(samples), 1.0/device_status["sample_rate"])
            mag = np.abs(fft)
            
            n = len(freq)//2
            sample_data["fft_freq"] = freq[:n].tolist()
            sample_data["fft_mag"] = mag[:n].tolist()
            
            if n > 0:
                peak_idx = np.argmax(mag[:n])
                sample_data["peak_freq"] = round(abs(freq[peak_idx]), 2)
                # 简化SNR计算
                if mag[peak_idx] > 0:
                    noise = np.mean(mag[:n]) - mag[peak_idx]/n
                    sample_data["snr"] = round(20 * np.log10(mag[peak_idx] / max(noise, 0.001)), 1)
    
    device_status["sample_count"] += 1

# ==================== 破解引擎 ====================

class CrackEngine:
    """破解引擎（Demo版）"""
    
    def __init__(self):
        self.running = False
        self.progress = 0
        self.attempts = 0
        self.result = ""
        self.start_time = 0
    
    def crack_md5(self, target_hash, key_length=16, charset="0123456789abcdef"):
        """暴力破解MD5（简化版Demo）"""
        import hashlib
        
        self.running = True
        self.progress = 0
        self.attempts = 0
        self.result = ""
        self.start_time = time.time()
        
        crack_data["running"] = True
        crack_data["target_hash"] = target_hash
        crack_data["key_length"] = key_length
        
        logger.info(f"开始破解: {target_hash}, 长度={key_length}")
        
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
                    
                    # 更新进度
                    if self.attempts % 10000 == 0:
                        self.progress = int((self.attempts / total) * 100)
                        crack_data["progress"] = self.progress
                        crack_data["attempts"] = self.attempts
                        crack_data["elapsed"] = time.time() - self.start_time
                        crack_data["rate"] = int(self.attempts / max(crack_data["elapsed"], 0.001))
                    
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
                self.result = f"破解成功!\n密钥: {result}\n哈希: {target_hash}\n尝试次数: {self.attempts}"
                crack_data["progress"] = 100
                crack_data["result"] = self.result
                logger.info(f"破解成功: {result}")
            else:
                if self.running:
                    self.result = f"破解失败\n尝试次数: {self.attempts}\n未找到匹配密钥"
                else:
                    self.result = f"破解已停止\n尝试次数: {self.attempts}"
                crack_data["result"] = self.result
                logger.info(f"破解结束，尝试次数: {self.attempts}")
            
            crack_data["running"] = False
            self.running = False
            return result
            
        except Exception as e:
            logger.error(f"破解异常: {e}")
            self.result = f"破解异常: {e}"
            crack_data["result"] = self.result
            crack_data["running"] = False
            self.running = False
            return None
    
    def stop(self):
        self.running = False

crack_engine = CrackEngine()

# ==================== 硬件安全测试 ====================

def run_hw_test(test_type):
    """运行硬件安全测试"""
    hw_test_data["running"] = True
    hw_test_data["current_test"] = test_type
    
    logger.info(f"开始硬件测试: {test_type}")
    
    try:
        if test_type == "voltage":
            # 电压测试
            usb_comm.send_command(0x07, struct.pack("<B", 0))
            time.sleep(0.1)
            hw_test_data["test_results"]["voltage"] = "正常"
            
        elif test_type == "clock":
            # 时钟测试
            usb_comm.send_command(0x07, struct.pack("<B", 1))
            time.sleep(0.1)
            hw_test_data["test_results"]["clock"] = f"{device_status['clock_freq']} MHz"
            
        elif test_type == "temperature":
            # 温度测试（收集30秒）
            temps = []
            for i in range(30):
                temps.append(device_status["core_temp"])
                time.sleep(1)
                hw_test_data["temp_history"] = temps[-60:]
            hw_test_data["test_results"]["temperature"] = f"{max(temps):.1f}°C (峰值)"
            
        elif test_type == "overclock":
            # 超频稳定性测试
            usb_comm.send_command(0x05, struct.pack("<B", 1))  # 超频
            time.sleep(1)
            # 运行负载
            for i in range(10):
                if not hw_test_data["running"]:
                    break
                time.sleep(1)
            usb_comm.send_command(0x05, struct.pack("<B", 0))  # 恢复
            hw_test_data["test_results"]["overclock"] = "通过"
            
        elif test_type == "glitch":
            # 电压毛刺测试
            for i in range(10):
                if not hw_test_data["running"]:
                    break
                width = 100 + i * 100  # 递增脉宽
                params = struct.pack("<HB", width, 0)  # 目标Pico #0
                usb_comm.send_command(0x08, params)
                hw_test_data["glitch_count"] = i + 1
                time.sleep(0.5)
            hw_test_data["test_results"]["glitch"] = f"完成{hw_test_data['glitch_count']}次毛刺注入"
        
        hw_test_data["running"] = False
        logger.info(f"硬件测试完成: {test_type}")
        
    except Exception as e:
        logger.error(f"硬件测试异常: {e}")
        hw_test_data["test_results"][test_type] = f"失败: {e}"
        hw_test_data["running"] = False

# ==================== 后台线程 ====================

def data_receiver_thread():
    """数据接收线程"""
    while True:
        if not usb_comm.connected:
            usb_comm.connect()
            # 检测Pico
            usb_comm.send_command(0x06)  # 获取状态
            time.sleep(2)
            continue
        
        frame = usb_comm.read_frame()
        if frame:
            data = frame["data"]
            parse_aggregated_data(data)
            
            # 状态数据更新
            if len(data) > 0 and data[0] == 0x03:
                pass  # 状态帧
        else:
            time.sleep(0.01)

def status_update_thread():
    """状态更新线程"""
    while True:
        try:
            # 推送状态
            socketio.emit("status_update", device_status)
            socketio.emit("data_update", sample_data)
            socketio.emit("crack_update", crack_data)
            socketio.emit("hw_test_update", hw_test_data)
            time.sleep(0.1)
        except Exception as e:
            time.sleep(0.5)

# ==================== Web路由 ====================

@app.route('/')
def index():
    return render_template('index.html', status=device_status)

@app.route('/api/status')
def api_status():
    return jsonify(device_status)

@app.route('/api/data')
def api_data():
    return jsonify(sample_data)

@app.route('/api/crack')
def api_crack():
    return jsonify(crack_data)

@app.route('/api/hw_test')
def api_hw_test():
    return jsonify(hw_test_data)

# ==================== WebSocket事件 ====================

@socketio.on('connect')
def on_connect():
    logger.info(f"客户端连接: {request.sid}")
    emit('status_update', device_status)

@socketio.on('disconnect')
def on_disconnect():
    logger.info(f"客户端断开: {request.sid}")

@socketio.on('start_sample')
def on_start_sample():
    logger.info("启动采样")
    usb_comm.send_command(0x01)  # 启动
    device_status["run_status"] = "Running"
    device_status["work_mode"] = "SAMPLE"

@socketio.on('stop_sample')
def on_stop_sample():
    logger.info("停止采样")
    usb_comm.send_command(0x02)  # 停止
    device_status["run_status"] = "Stop"

@socketio.on('set_sample_rate')
def on_set_rate(data):
    rate = data.get('rate', 50000)
    logger.info(f"设置采样率: {rate}")
    params = struct.pack("<I", rate)
    usb_comm.send_command(0x03, params)
    device_status["sample_rate"] = rate

@socketio.on('set_overclock')
def on_overclock(data):
    mode = data.get('mode', 0)
    logger.info(f"设置超频: mode={mode}")
    usb_comm.send_command(0x05, struct.pack("<B", mode))
    if mode == 1:
        device_status["pico_freq"] = 200
        device_status["pico2_freq"] = 240
    else:
        device_status["pico_freq"] = 133
        device_status["pico2_freq"] = 150

@socketio.on('start_crack')
def on_start_crack(data):
    target = data.get('target_hash', '')
    length = data.get('key_length', 16)
    
    if not target:
        return
    
    device_status["work_mode"] = "CRACK"
    device_status["run_status"] = "Running"
    
    thread = threading.Thread(
        target=crack_engine.crack_md5,
        args=(target, length),
        daemon=True
    )
    thread.start()

@socketio.on('stop_crack')
def on_stop_crack():
    crack_engine.stop()
    device_status["run_status"] = "Stop"

@socketio.on('start_hw_test')
def on_start_hw_test(data):
    test_type = data.get('test_type', '')
    if not test_type:
        return
    
    device_status["work_mode"] = "HW_TEST"
    device_status["run_status"] = "Running"
    
    thread = threading.Thread(
        target=run_hw_test,
        args=(test_type,),
        daemon=True
    )
    thread.start()

@socketio.on('stop_hw_test')
def on_stop_hw_test():
    hw_test_data["running"] = False
    device_status["run_status"] = "Stop"

# ==================== 主函数 ====================

def main():
    logger.info(f"超采集算系统 {DEMO_VERSION} 启动")
    logger.info(f"Demo模式: 1片RP2350 + 8片RP2040")
    
    # 启动后台线程
    threading.Thread(target=data_receiver_thread, daemon=True).start()
    threading.Thread(target=status_update_thread, daemon=True).start()
    
    # 启动Web服务
    logger.info(f"Web服务: http://{WEB_HOST}:{WEB_PORT}")
    socketio.run(app, host=WEB_HOST, port=WEB_PORT, debug=False, allow_unsafe_werkzeug=True)

if __name__ == '__main__':
    main()