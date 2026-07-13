#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
人机交互任务 (hmi_task.py)
- 100ms周期扫描按键状态，防抖处理
- OLED页面循环刷新
- LED状态机控制
- 按键长按事件处理
"""

import os
import sys
import time
import threading

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.logger import get_logger
from core.status_manager import status_mgr

logger = get_logger("HMI")


class HMIManager:
    """人机交互管理器"""
    
    def __init__(self):
        self._running = False
        self._current_page = 0
        self._total_pages = 4
        self._led_state = {"green": False, "blue": False, "red": False}
        self._key_state = {
            "left": False, "right": False, "up": False, "down": False, "ok": False
        }
        self._key_debounce = {}
    
    def start(self):
        logger.info("人机交互任务启动")
        self._running = True
        
        threading.Thread(target=self._scan_loop, daemon=True).start()
        threading.Thread(target=self._oled_loop, daemon=True).start()
        threading.Thread(target=self._led_loop, daemon=True).start()
        
        while self._running:
            time.sleep(1)
    
    def _scan_loop(self):
        """按键扫描循环"""
        while self._running:
            try:
                time.sleep(0.1)
            except Exception as e:
                logger.error(f"按键扫描异常: {e}")
                time.sleep(0.1)
    
    def _oled_loop(self):
        """OLED刷新循环"""
        while self._running:
            try:
                self._update_oled()
                time.sleep(0.1)
            except Exception as e:
                logger.error(f"OLED刷新异常: {e}")
                time.sleep(0.5)
    
    def _update_oled(self):
        """更新OLED显示（预留接口）"""
        pass
    
    def _led_loop(self):
        """LED状态机循环"""
        while self._running:
            try:
                self._update_leds()
                time.sleep(0.5)
            except Exception as e:
                logger.error(f"LED异常: {e}")
                time.sleep(1)
    
    def _update_leds(self):
        """更新LED状态"""
        system = status_mgr.get_system()
        mode = system.get("work_mode", "standby")
        run_status = system.get("run_status", "Stop")
        
        # 绿色LED：电源/电量（默认常亮）
        self._led_state["green"] = True
        
        # 蓝色LED：运行/算力
        if run_status == "Running":
            if mode == "bruteforce":
                self._led_state["blue"] = not self._led_state["blue"]  # 极速闪
            elif mode == "crack":
                self._led_state["blue"] = not self._led_state["blue"]  # 快闪
            else:
                self._led_state["blue"] = True  # 常亮
        else:
            self._led_state["blue"] = False
        
        # 红色LED：故障
        hw = status_mgr.get_hardware()
        if hw.get("core_temp", 0) > 70:
            self._led_state["red"] = True
        else:
            self._led_state["red"] = False
    
    def next_page(self):
        self._current_page = (self._current_page + 1) % self._total_pages
    
    def prev_page(self):
        self._current_page = (self._current_page - 1) % self._total_pages
    
    def stop(self):
        self._running = False


def main():
    hmi = HMIManager()
    hmi.start()


if __name__ == '__main__':
    main()
