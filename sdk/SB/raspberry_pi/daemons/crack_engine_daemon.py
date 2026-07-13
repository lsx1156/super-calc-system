#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
破解引擎守护进程 (crack_engine_daemon.py)
- 密码破解任务调度
- 分布式任务分片
- 破解进度监控
- 内存控制与负载均衡
"""

import os
import sys
import time
import hashlib
import threading
from typing import Optional, Callable

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.logger import get_logger
from core.status_manager import status_mgr
from core.config import WorkMode
from algorithms.crack_algorithms import (
    MaskGenerator,
    TaskScheduler,
    CrackEngine,
)

logger = get_logger("CrackEngine")


class CrackEngineDaemon:
    """破解引擎守护进程"""
    
    def __init__(self):
        self._running = False
        self._active = False
        self._lock = threading.RLock()
        self._thread: Optional[threading.Thread] = None
        self._monitor_thread: Optional[threading.Thread] = None
        
        self._engine: Optional[CrackEngine] = None
        self._scheduler: Optional[TaskScheduler] = None
        self._target_hash: Optional[str] = None
        self._mask: Optional[str] = None
        self._found_password: Optional[str] = None
        self._callbacks = {
            "crack_found": [],
            "progress_update": [],
            "crack_finished": [],
        }
    
    def register_callback(self, event: str, callback: Callable):
        """注册回调"""
        if event in self._callbacks:
            self._callbacks[event].append(callback)
    
    def _fire_event(self, event: str, *args, **kwargs):
        for cb in self._callbacks.get(event, []):
            try:
                cb(*args, **kwargs)
            except Exception as e:
                logger.error(f"回调异常 {event}: {e}")
    
    def init(self):
        """初始化"""
        self._running = True
        self._monitor_thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self._monitor_thread.start()
        logger.info("破解引擎初始化完成")
    
    def start_bruteforce(self, target_hash: str, mask: str = "?l?l?l?l?l?l?l?l",
                         algorithm: str = "md5", num_nodes: int = 1) -> bool:
        """启动暴力破解（掩码攻击）"""
        with self._lock:
            if self._active:
                logger.warning("破解任务已在运行")
                return False
            
            self._target_hash = target_hash
            self._mask = mask
            self._found_password = None
            self._active = True
            
            self._engine = CrackEngine(algorithm=algorithm)
            self._engine.start_mask(target_hash, mask)
            
            gen = MaskGenerator(mask)
            total = gen.total_count()
            
            self._scheduler = TaskScheduler(total_nodes=num_nodes)
            self._scheduler.setup_bruteforce(int(total))
            
            status_mgr.update_crack(
                crack_active=True,
                target_hash=target_hash,
                crack_mode="bruteforce",
                total_count=int(total),
                processed_count=0,
                found_password="",
            )
            
            threading.Thread(target=self._crack_worker, daemon=True).start()
            
            logger.info(f"启动暴力破解: hash={target_hash[:16]}..., mask={mask}")
            return True
    
    def _crack_worker(self):
        """破解工作线程"""
        try:
            batch_size = 5000
            last_report = 0
            
            while self._active and self._engine and self._engine._running:
                found = self._engine.step(batch_size)
                processed = self._engine._attempts
                
                if time.time() - last_report > 0.5:
                    total = self._scheduler._total_keys if self._scheduler else 0
                    progress = processed / total if total > 0 else 0
                    status_mgr.update_crack(
                        processed_count=processed,
                        progress=progress,
                    )
                    self._fire_event("progress_update", processed, total)
                    last_report = time.time()
                
                if found:
                    with self._lock:
                        self._found_password = self._engine._result
                        self._active = False
                    
                    status_mgr.update_crack(
                        crack_active=False,
                        processed_count=processed,
                        found_password=self._found_password,
                    )
                    
                    logger.info(f"破解成功: {self._found_password}")
                    self._fire_event("crack_found", self._found_password)
                    self._fire_event("crack_finished", True, self._found_password)
                    return
                
                if not self._engine._running:
                    break
            
            with self._lock:
                self._active = False
            
            total = self._scheduler._total_keys if self._scheduler else 0
            status_mgr.update_crack(
                crack_active=False,
                processed_count=self._engine._attempts if self._engine else 0,
                found_password="",
            )
            
            logger.info("破解完成，未找到匹配")
            self._fire_event("crack_finished", False, None)
            
        except Exception as e:
            logger.error(f"破解异常: {e}")
            with self._lock:
                self._active = False
            status_mgr.update_crack(crack_active=False, error_message=str(e))
    
    def stop_crack(self):
        """停止破解"""
        with self._lock:
            self._active = False
            if self._engine:
                self._engine._running = False
            status_mgr.update_crack(crack_active=False)
            logger.info("破解任务已停止")
    
    def get_progress(self) -> dict:
        """获取破解进度"""
        crack = status_mgr.get_crack()
        return {
            "active": crack.get("crack_active", False),
            "target_hash": crack.get("target_hash", ""),
            "total": crack.get("total_count", 0),
            "processed": crack.get("processed_count", 0),
            "found": crack.get("found_password", ""),
            "progress": crack.get("progress", 0.0),
        }
    
    def is_active(self) -> bool:
        """是否运行中"""
        return self._active
    
    def _monitor_loop(self):
        """监控循环"""
        while self._running:
            try:
                time.sleep(1)
            except Exception as e:
                logger.error(f"监控异常: {e}")
    
    def stop(self):
        """停止引擎"""
        self.stop_crack()
        self._running = False
        if self._monitor_thread:
            self._monitor_thread.join(timeout=2)
        logger.info("破解引擎已停止")


crack_engine = CrackEngineDaemon()
