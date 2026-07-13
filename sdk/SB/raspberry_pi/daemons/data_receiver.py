#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据接收守护进程 (data_receiver.py)
- USB串口轮询接收，波特率100Mbps
- 数据帧校验（帧头、帧尾、CRC32）
- 环形缓冲区缓存数据
- 数据分通道解析，打时间戳
Zero 2W优化：使用threading.Queue替代multiprocessing.Queue
"""

import os
import sys
import time
import struct
import zlib
import threading
from queue import Queue
from collections import deque
from typing import Optional, Tuple

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.config import PICO2_DEVICES, DATA_QUEUE_SIZE, RUN_ON_ZERO2W
from core.logger import get_logger
from core.usb_comm import usb_mgr

logger = get_logger("DataReceiver")


class DataReceiver:
    """数据接收器（Zero 2W优化版）"""
    
    def __init__(self):
        self._running = False
        
        if RUN_ON_ZERO2W:
            self._data_queue = Queue(maxsize=DATA_QUEUE_SIZE // 2)
            self._web_queue = Queue(maxsize=DATA_QUEUE_SIZE // 4)
        else:
            from multiprocessing import Queue as MPQueue
            self._data_queue = MPQueue(maxsize=DATA_QUEUE_SIZE)
            self._web_queue = MPQueue(maxsize=DATA_QUEUE_SIZE)
        
        self._total_packets = 0
        self._error_packets = 0
        self._lock = threading.Lock()
    
    @property
    def data_queue(self):
        return self._data_queue
    
    @property
    def web_queue(self):
        return self._web_queue
    
    def start(self):
        """启动数据接收"""
        logger.info("数据接收守护进程启动")
        
        usb_mgr.init()
        self._running = True
        
        threading.Thread(target=self._rx_loop, daemon=True).start()
        threading.Thread(target=self._reconnect_loop, daemon=True).start()
        
        logger.info("数据接收已就绪")
        
        while self._running:
            time.sleep(1)
    
    def _rx_loop(self):
        """接收主循环"""
        while self._running:
            try:
                frames = usb_mgr.read_all()
                for pico2_id, data_type, data, device_ts in frames:
                    self._total_packets += 1
                    self._handle_frame(pico2_id, data_type, data, device_ts)
            except Exception as e:
                logger.error(f"接收异常: {e}")
                self._error_packets += 1
                time.sleep(0.01)
            
            time.sleep(0.001)
    
    def _handle_frame(self, pico2_id: int, data_type: int, data: bytes, device_ts: int):
        """处理数据帧"""
        receive_timestamp = time.time()
        
        packet = {
            "pico2_id": pico2_id,
            "data_type": data_type,
            "data": data,
            "device_timestamp": device_ts,
            "receive_timestamp": receive_timestamp,
        }
        
        try:
            if not self._data_queue.full():
                self._data_queue.put(packet, block=False)
        except:
            pass
        
        try:
            if not self._web_queue.full():
                self._web_queue.put(packet, block=False)
        except:
            pass
    
    def _reconnect_loop(self):
        """重连检测循环"""
        while self._running:
            try:
                usb_mgr.reconnect_all()
                usb_mgr.update_status()
            except Exception as e:
                logger.error(f"重连异常: {e}")
            time.sleep(5)
    
    def get_stats(self):
        """获取统计信息"""
        return {
            "total_packets": self._total_packets,
            "error_packets": self._error_packets,
            "queue_size": self._data_queue.qsize(),
        }
    
    def stop(self):
        self._running = False


def main():
    receiver = DataReceiver()
    receiver.start()


if __name__ == '__main__':
    main()