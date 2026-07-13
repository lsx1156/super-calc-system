#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 数据接收模块
负责双路USB数据接收、帧校验、缓存
"""

import serial
import threading
import queue
import time
import struct
import zlib
import logging
from typing import Optional, Tuple
from config import (
    USB_DEVICES, USB_BAUDRATE, USB_TIMEOUT,
    DATA_QUEUE_SIZE, USB_FRAME_HEADER, USB_FRAME_TAIL
)

logger = logging.getLogger("DataReceiver")

# 帧结构常量
FRAME_HEADER = 0x55
FRAME_TAIL = 0xAA
MAX_DATA_LENGTH = 8192


class USBFrame:
    """USB帧结构"""
    
    HEADER = 0x55
    TAIL = 0xAA
    
    def __init__(self, node_id: int = 0, data: bytes = b"", length: int = 0):
        self.node_id = node_id
        self.data = data
        self.length = length
        self.crc32 = 0
    
    @staticmethod
    def parse(raw_data: bytes) -> Optional['USBFrame']:
        """解析原始数据为帧"""
        if len(raw_data) < 8:  # 最小帧长度
            return None
        
        # 检查帧头和帧尾
        if raw_data[0] != FRAME_HEADER or raw_data[-1] != FRAME_TAIL:
            logger.warning(f"帧头/帧尾错误: {raw_data[0]:02X}, {raw_data[-1]:02X}")
            return None
        
        try:
            # 解析帧结构
            node_id = raw_data[1]
            length = struct.unpack("<H", raw_data[2:4])[0]
            
            if length > MAX_DATA_LENGTH:
                logger.warning(f"数据长度超限: {length}")
                return None
            
            # 提取数据
            data_start = 4
            data_end = data_start + length
            data = raw_data[data_start:data_end]
            
            # 提取CRC32
            crc32 = struct.unpack("<I", raw_data[data_end:data_end+4])[0]
            
            # 验证CRC32
            calculated_crc = zlib.crc32(data) & 0xFFFFFFFF
            if calculated_crc != crc32:
                logger.warning(f"CRC32校验失败: 计算={calculated_crc:08X}, 接收={crc32:08X}")
                return None
            
            frame = USBFrame(node_id, data, length)
            frame.crc32 = crc32
            return frame
            
        except Exception as e:
            logger.error(f"帧解析异常: {e}")
            return None
    
    def to_bytes(self) -> bytes:
        """将帧转换为字节"""
        self.crc32 = zlib.crc32(self.data) & 0xFFFFFFFF
        frame_bytes = struct.pack("<B", FRAME_HEADER)
        frame_bytes += struct.pack("<B", self.node_id)
        frame_bytes += struct.pack("<H", self.length)
        frame_bytes += self.data
        frame_bytes += struct.pack("<I", self.crc32)
        frame_bytes += struct.pack("<B", FRAME_TAIL)
        return frame_bytes


class DataReceiver:
    """数据接收器"""
    
    def __init__(self, data_queue: queue.Queue, status_queue: queue.Queue):
        self.data_queue = data_queue
        self.status_queue = status_queue
        self.running = False
        self.serial_ports = {}
        self.receiver_threads = {}
        self.receive_count = 0
        self.error_count = 0
    
    def start(self):
        """启动数据接收"""
        self.running = True
        
        # 打开所有USB设备
        for device in USB_DEVICES:
            try:
                port = serial.Serial(
                    port=device,
                    baudrate=USB_BAUDRATE,
                    timeout=USB_TIMEOUT
                )
                self.serial_ports[device] = port
                logger.info(f"打开USB设备: {device}")
                
                # 启动接收线程
                thread = threading.Thread(
                    target=self._receive_loop,
                    args=(device, port),
                    daemon=True
                )
                thread.start()
                self.receiver_threads[device] = thread
                
            except Exception as e:
                logger.error(f"打开USB设备失败 {device}: {e}")
    
    def stop(self):
        """停止数据接收"""
        self.running = False
        
        # 关闭所有串口
        for device, port in self.serial_ports.items():
            try:
                port.close()
                logger.info(f"关闭USB设备: {device}")
            except Exception as e:
                logger.error(f"关闭USB设备失败 {device}: {e}")
    
    def _receive_loop(self, device: str, port: serial.Serial):
        """接收循环"""
        buffer = b""
        
        while self.running:
            try:
                # 读取数据
                raw_data = port.read(8192)
                if not raw_data:
                    continue
                
                buffer += raw_data
                
                # 查找帧头
                while FRAME_HEADER in buffer:
                    header_pos = buffer.find(FRAME_HEADER)
                    buffer = buffer[header_pos:]
                    
                    # 查找帧尾
                    tail_pos = buffer.find(FRAME_TAIL)
                    if tail_pos == -1:
                        # 帧尾未找到，等待更多数据
                        break
                    
                    # 提取完整帧
                    frame_data = buffer[:tail_pos + 1]
                    buffer = buffer[tail_pos + 1:]
                    
                    # 解析帧
                    frame = USBFrame.parse(frame_data)
                    if frame:
                        # 放入数据队列
                        self.data_queue.put({
                            "device": device,
                            "node_id": frame.node_id,
                            "data": frame.data,
                            "length": frame.length,
                            "timestamp": time.time()
                        })
                        self.receive_count += 1
                        
                        # 更新状态
                        if self.receive_count % 100 == 0:
                            self.status_queue.put({
                                "receiver": {
                                    "count": self.receive_count,
                                    "errors": self.error_count,
                                    "buffer_size": len(buffer)
                                }
                            })
                    else:
                        self.error_count += 1
                
            except Exception as e:
                logger.error(f"接收异常 {device}: {e}")
                self.error_count += 1
                time.sleep(0.1)
    
    def send_command(self, device: str, cmd: int, params: bytes = b"") -> bool:
        """发送命令到Pico2"""
        if device not in self.serial_ports:
            return False
        
        try:
            # 构建命令帧
            data = struct.pack("<B", cmd) + params
            frame = USBFrame(node_id=0xFF, data=data, length=len(data))
            frame_bytes = frame.to_bytes()
            
            # 发送
            port = self.serial_ports[device]
            port.write(frame_bytes)
            port.flush()
            
            logger.debug(f"发送命令到 {device}: cmd={cmd:02X}")
            return True
            
        except Exception as e:
            logger.error(f"发送命令失败 {device}: {e}")
            return False
    
    def get_statistics(self) -> dict:
        """获取统计信息"""
        return {
            "receive_count": self.receive_count,
            "error_count": self.error_count,
            "devices_connected": len(self.serial_ports)
        }


def create_data_receiver(data_queue: queue.Queue, status_queue: queue.Queue) -> DataReceiver:
    """创建数据接收器"""
    return DataReceiver(data_queue, status_queue)