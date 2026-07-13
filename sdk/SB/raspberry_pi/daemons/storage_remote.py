#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
存储与远程进程 (storage_remote.py)
- 数据异步写入eMMC，支持断点续传
- 系统日志分级记录，自动轮转
- 邮件/短信预警推送
"""

import os
import sys
import time
import json
import csv
import threading
import multiprocessing
from datetime import datetime
from typing import Optional, Any

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.config import (
    STORAGE_PATH, DEFAULT_STORAGE_FORMAT,
    MAX_LOG_SIZE, LOG_ROTATION_COUNT, WRITE_BUFFER_SIZE, FLUSH_INTERVAL,
    ALERT_LOW_BATTERY,
)
from core.logger import get_logger

logger = get_logger("StorageRemote")


class StorageManager:
    """存储管理器"""
    
    def __init__(self):
        self._running = False
        self._queue: Optional[multiprocessing.Queue] = None
        self._buffer = []
        self._buffer_size = 0
        self._current_file = None
        self._last_flush = 0
        self._lock = threading.Lock()
        
        os.makedirs(STORAGE_PATH, exist_ok=True)
    
    def set_queue(self, queue):
        self._queue = queue
    
    def start(self):
        logger.info("存储管理进程启动")
        self._running = True
        
        threading.Thread(target=self._write_loop, daemon=True).start()
        threading.Thread(target=self._flush_loop, daemon=True).start()
        
        while self._running:
            time.sleep(1)
    
    def _write_loop(self):
        """写入循环"""
        while self._running:
            try:
                if self._queue is None or self._queue.empty():
                    time.sleep(0.01)
                    continue
                
                packet = self._queue.get(timeout=0.1)
                self._buffer.append(packet)
                self._buffer_size += 1
                
                if self._buffer_size >= WRITE_BUFFER_SIZE:
                    self._flush_buffer()
                    
            except Exception as e:
                logger.error(f"写入异常: {e}")
                time.sleep(0.1)
    
    def _flush_loop(self):
        """定时刷新循环"""
        while self._running:
            try:
                now = time.time()
                if now - self._last_flush > FLUSH_INTERVAL:
                    self._flush_buffer()
                    self._last_flush = now
            except Exception as e:
                logger.error(f"刷新异常: {e}")
            time.sleep(FLUSH_INTERVAL)
    
    def _flush_buffer(self):
        """刷新缓冲区到磁盘"""
        if not self._buffer:
            return
        
        with self._lock:
            try:
                today = datetime.now().strftime("%Y%m%d")
                filename = f"data_{today}.csv"
                filepath = os.path.join(STORAGE_PATH, filename)
                
                file_exists = os.path.exists(filepath)
                
                with open(filepath, 'a', newline='', encoding='utf-8') as f:
                    writer = csv.writer(f)
                    
                    if not file_exists:
                        writer.writerow(["timestamp", "pico2_id", "data_type", "data"])
                    
                    for packet in self._buffer:
                        writer.writerow([
                            packet.get("timestamp", time.time()),
                            packet.get("pico2_id", 0),
                            packet.get("data_type", 0),
                            packet.get("data", b"").hex() if isinstance(packet.get("data"), bytes) else str(packet.get("data")),
                        ])
                
                logger.debug(f"写入 {len(self._buffer)} 条记录到 {filename}")
                self._buffer.clear()
                self._buffer_size = 0
                
            except Exception as e:
                logger.error(f"写入文件失败: {e}")
    
    def save_config(self, config: dict, name: str = "config.json"):
        """保存配置"""
        try:
            filepath = os.path.join(STORAGE_PATH, name)
            with open(filepath, 'w', encoding='utf-8') as f:
                json.dump(config, f, indent=2, ensure_ascii=False)
            return True
        except Exception as e:
            logger.error(f"保存配置失败: {e}")
            return False
    
    def load_config(self, name: str = "config.json"):
        """加载配置"""
        try:
            filepath = os.path.join(STORAGE_PATH, name)
            if os.path.exists(filepath):
                with open(filepath, 'r', encoding='utf-8') as f:
                    return json.load(f)
        except Exception as e:
            logger.error(f"加载配置失败: {e}")
        return None
    
    def stop(self):
        self._running = False
        self._flush_buffer()


class AlertManager:
    """预警管理器"""
    
    def __init__(self):
        self._low_battery_sent = False
    
    def check_battery(self, level: float):
        if level <= ALERT_LOW_BATTERY and not self._low_battery_sent:
            logger.warning(f"低电量预警: {level}%")
            self._send_alert(f"低电量警告: {level}%")
            self._low_battery_sent = True
        elif level > ALERT_LOW_BATTERY + 10:
            self._low_battery_sent = False
    
    def _send_alert(self, message: str):
        """发送预警（预留接口）"""
        logger.info(f"[预警] {message}")
        # TODO: 实现邮件/短信推送


def main():
    storage = StorageManager()
    storage.start()


if __name__ == '__main__':
    main()
