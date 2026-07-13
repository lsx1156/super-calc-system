#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
多模式调度系统
支持：采样模式、破译模式、暴力破解模式、硬件测试模式
模式切换流程：停止当前任务 → 复位 → 配置 → 启动 → 状态同步
"""

import time
import struct
import threading
from typing import Optional, Callable
from enum import Enum

from core.config import (
    WorkMode, MODE_NAMES, CMD, DEFAULT_SAMPLE_RATE,
    OVERCLOCK_IN_BRUTEFORCE, OVERCLOCK_TEMP_LIMIT,
)
from core.logger import get_logger
from core.status_manager import status_mgr
from core.usb_comm import usb_mgr
from cluster.adaptive_cluster import cluster_mgr

logger = get_logger("ModeScheduler")


class ModeState(Enum):
    IDLE = "idle"
    SWITCHING = "switching"
    RUNNING = "running"
    STOPPING = "stopping"
    FAULT = "fault"


class ModeScheduler:
    """多模式调度器"""
    
    def __init__(self):
        self.current_mode = WorkMode.STANDBY
        self.target_mode = WorkMode.STANDBY
        self.state = ModeState.IDLE
        self._lock = threading.RLock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
        
        # 模式配置
        self.mode_configs = {
            WorkMode.SAMPLE: {
                "sample_rate": DEFAULT_SAMPLE_RATE,
                "overclock": False,
                "data_aggregation": True,
            },
            WorkMode.CRACK: {
                "sample_rate": DEFAULT_SAMPLE_RATE,
                "overclock": False,
                "crack_mode": "side_channel",
            },
            WorkMode.BRUTEFORCE: {
                "sample_rate": 0,
                "overclock": True,
                "crack_mode": "bruteforce",
            },
            WorkMode.HW_TEST: {
                "sample_rate": 0,
                "overclock": False,
            },
        }
        
        self._callbacks = {
            "mode_changed": [],
            "state_changed": [],
            "mode_started": [],
            "mode_stopped": [],
        }
    
    def register_callback(self, event: str, callback: Callable):
        """注册回调"""
        if event in self._callbacks:
            self._callbacks[event].append(callback)
    
    def _fire_event(self, event: str, *args, **kwargs):
        if event in self._callbacks:
            for cb in self._callbacks[event]:
                try:
                    cb(*args, **kwargs)
                except Exception as e:
                    logger.error(f"回调异常 {event}: {e}")
    
    def init(self):
        """初始化"""
        self._running = True
        self._thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._thread.start()
        logger.info("多模式调度器初始化完成")
    
    def switch_mode(self, target_mode: str) -> bool:
        """切换模式"""
        with self._lock:
            if self.state == ModeState.SWITCHING:
                logger.warning("正在切换模式中，忽略请求")
                return False
            
            if target_mode == self.current_mode and self.state == ModeState.RUNNING:
                logger.info(f"已处于 {MODE_NAMES.get(target_mode, target_mode)}")
                return True
            
            self.target_mode = target_mode
            self._set_state(ModeState.SWITCHING)
            
            # 异步执行切换
            threading.Thread(target=self._do_switch, daemon=True).start()
            
            return True
    
    def _do_switch(self):
        """执行模式切换"""
        try:
            logger.info(f"开始切换模式: {self.current_mode} → {self.target_mode}")
            
            # 1. 停止当前任务
            if self.current_mode != WorkMode.STANDBY:
                self._stop_current_mode()
            
            # 2. 复位
            self._reset_hardware()
            
            # 3. 配置新模式
            if self.target_mode != WorkMode.STANDBY:
                self._configure_mode(self.target_mode)
            
            # 4. 启动新模式
            if self.target_mode != WorkMode.STANDBY:
                self._start_mode(self.target_mode)
            
            # 5. 更新状态
            self.current_mode = self.target_mode
            
            if self.target_mode == WorkMode.STANDBY:
                self._set_state(ModeState.IDLE)
            else:
                self._set_state(ModeState.RUNNING)
            
            status_mgr.update_system(
                work_mode=self.current_mode,
                run_status="Running" if self.state == ModeState.RUNNING else "Stop",
            )
            
            self._fire_event("mode_changed", self.current_mode)
            logger.info(f"模式切换完成: {MODE_NAMES.get(self.current_mode, self.current_mode)}")
            
        except Exception as e:
            logger.error(f"模式切换失败: {e}")
            self._set_state(ModeState.FAULT)
            status_mgr.update_system(fault_message=str(e))
    
    def _stop_current_mode(self):
        """停止当前模式"""
        logger.info(f"停止当前模式: {self.current_mode}")
        
        # 发送停止命令
        usb_mgr.send_to_all(CMD["STOP_SAMPLE"])
        time.sleep(0.5)
        
        # 关闭超频
        usb_mgr.send_to_all(CMD["OVERCLOCK"], struct.pack("<B", 0))
        time.sleep(0.2)
        
        self._fire_event("mode_stopped", self.current_mode)
    
    def _reset_hardware(self):
        """复位硬件"""
        logger.info("复位硬件")
        # 软复位命令
        # usb_mgr.send_to_all(CMD["RESET"])
        time.sleep(0.5)
    
    def _configure_mode(self, mode: str):
        """配置模式"""
        config = self.mode_configs.get(mode, {})
        logger.info(f"配置模式: {mode} -> {config}")
        
        # 设置工作模式
        usb_mgr.send_to_all(CMD["SET_MODE"], struct.pack("<B", self._mode_to_code(mode)))
        time.sleep(0.2)
        
        # 设置采样率
        if config.get("sample_rate", 0) > 0:
            rate = config["sample_rate"]
            params = struct.pack("<I", rate)
            usb_mgr.send_to_all(CMD["SET_RATE"], params)
            status_mgr.update_sample(sample_rate=rate)
        
        # 超频配置
        if config.get("overclock", False) and OVERCLOCK_IN_BRUTEFORCE:
            usb_mgr.send_to_all(CMD["OVERCLOCK"], struct.pack("<B", 1))
            status_mgr.update_hardware(overclock_active=True)
        else:
            usb_mgr.send_to_all(CMD["OVERCLOCK"], struct.pack("<B", 0))
            status_mgr.update_hardware(overclock_active=False)
    
    def _start_mode(self, mode: str):
        """启动模式"""
        logger.info(f"启动模式: {mode}")
        
        if mode == WorkMode.SAMPLE:
            usb_mgr.send_to_all(CMD["START_SAMPLE"])
        elif mode == WorkMode.CRACK:
            usb_mgr.send_to_all(CMD["START_SAMPLE"])
        elif mode == WorkMode.BRUTEFORCE:
            usb_mgr.send_to_all(CMD["START_CRACK"])
        elif mode == WorkMode.HW_TEST:
            usb_mgr.send_to_all(CMD["START_TEST"])
        
        self._fire_event("mode_started", mode)
    
    def _mode_to_code(self, mode: str) -> int:
        """模式名称转代码"""
        mode_map = {
            WorkMode.SAMPLE: 0x00,
            WorkMode.CRACK: 0x01,
            WorkMode.BRUTEFORCE: 0x02,
            WorkMode.HW_TEST: 0x03,
        }
        return mode_map.get(mode, 0x00)
    
    def _set_state(self, state: ModeState):
        """设置状态"""
        old = self.state
        self.state = state
        if old != state:
            self._fire_event("state_changed", state)
    
    def stop_current(self):
        """停止当前任务，回到待机"""
        if self.current_mode != WorkMode.STANDBY:
            self.switch_mode(WorkMode.STANDBY)
    
    def _monitor_loop(self):
        """监控循环"""
        while self._running:
            try:
                # 温度保护
                hw = status_mgr.get_hardware()
                if hw.get("core_temp", 0) >= OVERCLOCK_TEMP_LIMIT:
                    if hw.get("overclock_active", False):
                        logger.warning("温度过高，关闭超频")
                        usb_mgr.send_to_all(CMD["OVERCLOCK"], struct.pack("<B", 0))
                        status_mgr.update_hardware(overclock_active=False)
            
            except Exception as e:
                logger.error(f"模式监控异常: {e}")
            
            time.sleep(2)
    
    def get_current_mode(self) -> str:
        """获取当前模式"""
        return self.current_mode
    
    def get_state(self) -> ModeState:
        """获取当前状态"""
        return self.state
    
    def is_running(self) -> bool:
        """是否运行中"""
        return self.state == ModeState.RUNNING
    
    def stop(self):
        """停止调度器"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        logger.info("模式调度器已停止")


# 全局实例
mode_scheduler = ModeScheduler()
