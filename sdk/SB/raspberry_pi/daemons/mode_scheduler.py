#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模式调度守护进程 (mode_scheduler.py)
- 扫描模式开关、功能开关、启停开关GPIO状态
- 模式切换流程：停止→复位→配置→启动→状态同步
- 十字键参数调整
- 超频控制：暴力破解模式下超频，温度异常降频
"""

import os
import sys
import time
import struct
import threading
import multiprocessing

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.config import WorkMode, CMD
from core.logger import get_logger
from core.status_manager import status_mgr
from core.usb_comm import usb_mgr
from cluster.adaptive_cluster import cluster_mgr

logger = get_logger("ModeSchedulerDaemon")


class ModeSchedulerDaemon:
    """模式调度守护进程"""
    
    def __init__(self):
        self._running = False
        self._mode_scheduler = None
    
    def start(self):
        logger.info("模式调度守护进程启动")
        
        usb_mgr.init()
        cluster_mgr.init()
        
        from modes.mode_scheduler import mode_scheduler
        self._mode_scheduler = mode_scheduler
        self._mode_scheduler.init()
        
        self._running = True
        
        threading.Thread(target=self._gpio_monitor, daemon=True).start()
        threading.Thread(target=self._key_monitor, daemon=True).start()
        
        logger.info("模式调度已就绪")
        
        while self._running:
            time.sleep(1)
    
    def _gpio_monitor(self):
        """GPIO开关监控（预留，实际硬件需要GPIO库）"""
        while self._running:
            try:
                time.sleep(0.1)
            except Exception as e:
                logger.error(f"GPIO监控异常: {e}")
                time.sleep(1)
    
    def _key_monitor(self):
        """按键监控（预留）"""
        while self._running:
            try:
                time.sleep(0.05)
            except Exception as e:
                logger.error(f"按键监控异常: {e}")
                time.sleep(1)
    
    def switch_mode(self, mode: str):
        """切换模式"""
        logger.info(f"切换模式: {mode}")
        if self._mode_scheduler:
            return self._mode_scheduler.switch_mode(mode)
        return False
    
    def stop(self):
        self._running = False
        if self._mode_scheduler:
            self._mode_scheduler.stop()
        cluster_mgr.stop()


def main():
    daemon = ModeSchedulerDaemon()
    daemon.start()


if __name__ == '__main__':
    main()
