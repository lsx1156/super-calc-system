#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 存储管理模块
负责数据落盘、日志记录、存储管理
"""

import os
import csv
import json
import threading
import queue
import time
import logging
from datetime import datetime
from typing import Dict, Any, Optional
from config import (
    STORAGE_PATH, MAX_STORAGE_SIZE, DATA_FORMAT,
    LOG_DIR, LOG_FILE, LOG_LEVEL, LOG_FORMAT
)

logger = logging.getLogger("StorageManager")


class StorageManager:
    """存储管理器"""
    
    def __init__(self, data_queue: queue.Queue):
        self.data_queue = data_queue
        self.running = False
        
        # 创建存储目录
        self._ensure_directories()
        
        # 存储统计
        self.total_size = 0
        self.file_count = 0
        self.write_count = 0
        
        # 当前文件
        self.current_file = None
        self.current_csv_writer = None
        
        # 处理线程
        self.storage_thread = None
    
    def _ensure_directories(self):
        """确保目录存在"""
        os.makedirs(STORAGE_PATH, exist_ok=True)
        os.makedirs(LOG_DIR, exist_ok=True)
    
    def start(self):
        """启动存储管理"""
        self.running = True
        
        # 创建新数据文件
        self._create_new_file()
        
        # 启动存储线程
        self.storage_thread = threading.Thread(
            target=self._storage_loop,
            daemon=True
        )
        self.storage_thread.start()
        
        logger.info("存储管理器启动")
    
    def stop(self):
        """停止存储管理"""
        self.running = False
        
        # 关闭当前文件
        if self.current_file:
            self.current_file.close()
        
        if self.storage_thread:
            self.storage_thread.join(timeout=2)
        
        logger.info("存储管理器停止")
    
    def _create_new_file(self):
        """创建新数据文件"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        if DATA_FORMAT == "csv":
            filename = os.path.join(STORAGE_PATH, f"data_{timestamp}.csv")
            self.current_file = open(filename, 'w', newline='')
            self.current_csv_writer = csv.writer(self.current_file)
            # 写入CSV头
            self.current_csv_writer.writerow([
                "timestamp", "type", "node_id", "channel", "value", "voltage"
            ])
        else:
            filename = os.path.join(STORAGE_PATH, f"data_{timestamp}.bin")
            self.current_file = open(filename, 'wb')
        
        self.file_count += 1
        logger.info(f"创建数据文件: {filename}")
    
    def _storage_loop(self):
        """存储循环"""
        while self.running:
            try:
                # 从队列获取数据
                data_item = self.data_queue.get(timeout=0.5)
                
                # 检查存储空间
                if self.total_size >= MAX_STORAGE_SIZE:
                    logger.warning("存储空间已满")
                    continue
                
                # 写入数据
                self._write_data(data_item)
                
                # 检查文件大小，超过限制则创建新文件
                if self.current_file:
                    file_size = os.fstat(self.current_file.fileno()).st_size
                    if file_size > 100 * 1024 * 1024:  # 100MB
                        self.current_file.close()
                        self._create_new_file()
                
            except queue.Empty:
                continue
            except Exception as e:
                logger.error(f"存储异常: {e}")
    
    def _write_data(self, data_item: Dict[str, Any]):
        """写入数据"""
        try:
            timestamp = data_item.get("timestamp", time.time())
            node_id = data_item.get("node_id", 0)
            data = data_item.get("data", b"")
            
            if DATA_FORMAT == "csv":
                # CSV格式写入
                data_type = data[0] if data else 0
                
                if data_type == 0x01:  # 模拟数据
                    # 解析并写入每个采样点
                    for i in range(1, len(data), 2):
                        if i + 1 < len(data):
                            sample_value = (data[i] << 8) | data[i+1]
                            channel = (sample_value >> 12) & 0x0F
                            value = sample_value & 0x0FFF
                            voltage = value * 3.3 / 4095
                            
                            self.current_csv_writer.writerow([
                                timestamp, "analog", node_id, channel, value, voltage
                            ])
                
                elif data_type == 0x02:  # 数字数据
                    # 解析并写入每个位状态
                    for i in range(1, len(data)):
                        byte_value = data[i]
                        for j in range(8):
                            channel = (i - 1) * 8 + j
                            state = (byte_value >> j) & 0x01
                            
                            self.current_csv_writer.writerow([
                                timestamp, "digital", node_id, channel, state, ""
                            ])
                
                self.current_file.flush()
            
            else:
                # 二进制格式写入
                # 写入时间戳（8字节）
                self.current_file.write(struct.pack("<Q", int(timestamp * 1000000)))
                # 写入节点ID（1字节）
                self.current_file.write(struct.pack("<B", node_id))
                # 写入数据长度（2字节）
                self.current_file.write(struct.pack("<H", len(data)))
                # 写入数据
                self.current_file.write(data)
                self.current_file.flush()
            
            self.write_count += 1
            self.total_size += len(data) + 11  # 数据 + 头部
            
        except Exception as e:
            logger.error(f"写入数据异常: {e}")
    
    def get_statistics(self) -> Dict[str, Any]:
        """获取存储统计"""
        return {
            "total_size": self.total_size,
            "file_count": self.file_count,
            "write_count": self.write_count,
            "storage_usage": (self.total_size / MAX_STORAGE_SIZE) * 100
        }
    
    def clear_old_data(self, days: int = 30):
        """清除旧数据"""
        cutoff_time = datetime.now() - timedelta(days=days)
        
        for filename in os.listdir(STORAGE_PATH):
            filepath = os.path.join(STORAGE_PATH, filename)
            
            # 检查文件修改时间
            mtime = datetime.fromtimestamp(os.path.getmtime(filepath))
            if mtime < cutoff_time:
                try:
                    os.remove(filepath)
                    logger.info(f"删除旧数据文件: {filename}")
                except Exception as e:
                    logger.error(f"删除文件失败: {e}")


class LogManager:
    """日志管理器"""
    
    def __init__(self):
        self._setup_logging()
    
    def _setup_logging(self):
        """设置日志"""
        # 创建日志目录
        os.makedirs(LOG_DIR, exist_ok=True)
        
        # 配置日志
        logging.basicConfig(
            level=getattr(logging, LOG_LEVEL),
            format=LOG_FORMAT,
            handlers=[
                logging.FileHandler(LOG_FILE),
                logging.StreamHandler()
            ]
        )
    
    def get_log_files(self) -> list:
        """获取日志文件列表"""
        return [f for f in os.listdir(LOG_DIR) if f.endswith('.log')]
    
    def get_log_content(self, filename: str, lines: int = 100) -> str:
        """获取日志内容"""
        filepath = os.path.join(LOG_DIR, filename)
        try:
            with open(filepath, 'r') as f:
                content = f.readlines()
                return ''.join(content[-lines:])
        except Exception as e:
            return f"读取日志失败: {e}"


def create_storage_manager(data_queue: queue.Queue) -> StorageManager:
    """创建存储管理器"""
    return StorageManager(data_queue)


def setup_logging():
    """设置日志"""
    LogManager()