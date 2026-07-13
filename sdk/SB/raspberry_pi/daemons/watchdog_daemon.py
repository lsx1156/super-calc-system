#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
系统守护进程 (watchdog_daemon.py)
- 监控所有业务进程运行状态，崩溃自动重启
- 驱动硬件看门狗 /dev/watchdog
- 监控系统资源（CPU、内存、存储）
- 掉电中断处理，数据落盘与配置备份
- 开机自启，优先级最高
"""

import os
import sys
import time
import signal
import subprocess
import threading
from typing import Dict, List, Optional

# 添加路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.config import (
    WATCHDOG_DEVICE, WATCHDOG_TIMEOUT, WATCHDOG_FEED_INTERVAL,
    PROCESS_MONITOR_INTERVAL, MAX_RESTART_COUNT, RESTART_COOLDOWN,
    LOG_DIR,
)
from core.logger import get_logger

logger = get_logger("WatchdogDaemon")


class ProcessInfo:
    """被监控的进程信息"""
    
    def __init__(self, name: str, cmd: List[str], critical: bool = True):
        self.name = name
        self.cmd = cmd
        self.critical = critical
        self.process: Optional[subprocess.Popen] = None
        self.restart_count = 0
        self.last_restart = 0
        self.running = False


class WatchdogDaemon:
    """系统守护进程"""
    
    def __init__(self):
        self.processes: Dict[str, ProcessInfo] = {}
        self._running = False
        self._watchdog_fd = None
        self._lock = threading.Lock()
        self._power_fail = False
        
        signal.signal(signal.SIGTERM, self._handle_signal)
        signal.signal(signal.SIGINT, self._handle_signal)
    
    def register_process(self, name: str, cmd: List[str], critical: bool = True):
        """注册要监控的进程"""
        self.processes[name] = ProcessInfo(name, cmd, critical)
        logger.info(f"注册监控进程: {name} {'[关键]' if critical else ''}")
    
    def start(self):
        """启动守护进程"""
        logger.info("系统守护进程启动")
        
        self._init_watchdog()
        self._start_all_processes()
        
        self._running = True
        
        threading.Thread(target=self._watchdog_thread, daemon=True).start()
        threading.Thread(target=self._monitor_thread, daemon=True).start()
        threading.Thread(target=self._resource_monitor, daemon=True).start()
        threading.Thread(target=self._powerfail_monitor, daemon=True).start()
        
        logger.info("守护进程进入运行状态")
        
        while self._running:
            time.sleep(1)
    
    def _init_watchdog(self):
        """初始化硬件看门狗"""
        try:
            if os.path.exists(WATCHDOG_DEVICE):
                self._watchdog_fd = os.open(WATCHDOG_DEVICE, os.O_WRONLY)
                logger.info(f"硬件看门狗已启用: {WATCHDOG_DEVICE}")
            else:
                logger.warning(f"看门狗设备不存在: {WATCHDOG_DEVICE}，使用软件看门狗")
        except Exception as e:
            logger.error(f"看门狗初始化失败: {e}")
    
    def _start_all_processes(self):
        """启动所有注册的进程"""
        for name, proc_info in self.processes.items():
            self._start_process(name)
    
    def _start_process(self, name: str) -> bool:
        """启动单个进程"""
        if name not in self.processes:
            return False
        
        proc_info = self.processes[name]
        
        now = time.time()
        if now - proc_info.last_restart < RESTART_COOLDOWN:
            if proc_info.restart_count >= MAX_RESTART_COUNT:
                logger.error(f"进程 {name} 重启次数过多，不再重启")
                if proc_info.critical:
                    self._emergency_shutdown(f"关键进程 {name} 无法恢复")
                return False
        
        try:
            logger.info(f"启动进程: {name} -> {' '.join(proc_info.cmd)}")
            proc_info.process = subprocess.Popen(
                proc_info.cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            proc_info.running = True
            proc_info.last_restart = now
            
            if now - proc_info.last_restart > RESTART_COOLDOWN * 2:
                proc_info.restart_count = 0
            
            logger.info(f"进程 {name} 启动成功，PID={proc_info.process.pid}")
            return True
            
        except Exception as e:
            logger.error(f"启动进程 {name} 失败: {e}")
            proc_info.restart_count += 1
            return False
    
    def _stop_process(self, name: str):
        """停止进程"""
        if name not in self.processes:
            return
        
        proc_info = self.processes[name]
        if proc_info.process and proc_info.process.poll() is None:
            try:
                logger.info(f"停止进程: {name}")
                proc_info.process.terminate()
                proc_info.process.wait(timeout=5)
            except:
                try:
                    proc_info.process.kill()
                except:
                    pass
            proc_info.running = False
    
    def _watchdog_thread(self):
        """看门狗喂食线程"""
        while self._running:
            try:
                if self._watchdog_fd:
                    os.write(self._watchdog_fd, b"1")
                    os.fsync(self._watchdog_fd)
                time.sleep(WATCHDOG_FEED_INTERVAL)
            except Exception as e:
                logger.error(f"看门狗喂食失败: {e}")
                time.sleep(1)
    
    def _monitor_thread(self):
        """进程监控线程"""
        while self._running:
            try:
                for name, proc_info in self.processes.items():
                    if not proc_info.running:
                        continue
                    
                    if proc_info.process is None or proc_info.process.poll() is not None:
                        exit_code = proc_info.process.returncode if proc_info.process else -1
                        logger.warning(f"进程 {name} 已退出，退出码={exit_code}")
                        proc_info.running = False
                        proc_info.restart_count += 1
                        
                        if proc_info.critical:
                            logger.info(f"自动重启关键进程: {name}")
                            self._start_process(name)
                
                time.sleep(PROCESS_MONITOR_INTERVAL)
                
            except Exception as e:
                logger.error(f"进程监控异常: {e}")
                time.sleep(1)
    
    def _resource_monitor(self):
        """系统资源监控"""
        while self._running:
            try:
                try:
                    import psutil
                    cpu_percent = psutil.cpu_percent(interval=1)
                    mem = psutil.virtual_memory()
                    disk = psutil.disk_usage('/')
                    
                    logger.debug(
                        f"资源: CPU={cpu_percent:.1f}% "
                        f"MEM={mem.percent:.1f}% "
                        f"DISK={disk.percent:.1f}%"
                    )
                    
                    if mem.percent > 95:
                        logger.critical("内存使用率超过95%，触发应急保护")
                        self._emergency_shutdown("内存耗尽")
                        
                except ImportError:
                    pass
                
                time.sleep(5)
                
            except Exception as e:
                logger.error(f"资源监控异常: {e}")
                time.sleep(5)
    
    def _powerfail_monitor(self):
        """掉电检测线程"""
        while self._running:
            try:
                time.sleep(0.1)
            except Exception as e:
                logger.error(f"掉电检测异常: {e}")
                time.sleep(1)
    
    def _emergency_shutdown(self, reason: str):
        """紧急关机流程"""
        logger.critical(f"紧急关机: {reason}")
        self._power_fail = True
        
        # 停止所有非关键进程
        for name, proc_info in self.processes.items():
            if not proc_info.critical:
                self._stop_process(name)
        
        # 触发数据落盘
        logger.info("执行应急数据落盘")
        time.sleep(2)
    
    def _handle_signal(self, signum, frame):
        """信号处理"""
        logger.info(f"收到信号 {signum}，正在关闭...")
        self._running = False
        
        # 停止所有进程
        for name in list(self.processes.keys()):
            self._stop_process(name)
        
        # 关闭看门狗
        if self._watchdog_fd:
            try:
                os.close(self._watchdog_fd)
            except:
                pass
        
        logger.info("守护进程已退出")
        sys.exit(0)


def main():
    daemon = WatchdogDaemon()
    
    # 注册业务进程
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    python = sys.executable
    
    daemon.register_process("data_receiver", 
        [python, os.path.join(base_dir, "daemons", "data_receiver.py")], critical=True)
    daemon.register_process("mode_scheduler", 
        [python, os.path.join(base_dir, "daemons", "mode_scheduler.py")], critical=True)
    daemon.register_process("business_logic", 
        [python, os.path.join(base_dir, "daemons", "business_logic.py")], critical=True)
    daemon.register_process("storage", 
        [python, os.path.join(base_dir, "daemons", "storage_remote.py")], critical=False)
    daemon.register_process("web_service", 
        [python, os.path.join(base_dir, "services", "web_service.py")], critical=False)
    
    daemon.start()


if __name__ == '__main__':
    main()
