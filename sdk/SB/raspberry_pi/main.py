#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 - 主入口程序
完整版：支持自适应集群扩展、多模式调度
适配树莓派Zero 2W（512MB RAM，单核USB）
"""

import os
import sys
import time
import signal
import argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.config import SYSTEM_NAME, SYSTEM_VERSION, DEBUG, LOG_DIR, DATA_DIR, RUN_ON_ZERO2W
from core.logger import get_logger, setup_logger


def print_banner():
    """打印启动横幅"""
    if RUN_ON_ZERO2W:
        banner = f"""
╔══════════════════════════════════════════════════════════════╗
║           超采集算系统 Super Collection Compute System        ║
║                    完整版 V{SYSTEM_VERSION} - Zero 2W适配                      ║
╠══════════════════════════════════════════════════════════════╣
║  三层分布式架构: 树莓派Zero 2W + Pico2 + Pico 集群            ║
║  最大支持: 1×Pico2 + 8×Pico = 32路模拟 + 64路数字           ║
║  工作模式: 采样 / 破译 / 暴力破解 / 硬件测试                   ║
║  内存优化: 512MB限制，精简队列和缓冲                          ║
╚══════════════════════════════════════════════════════════════╝
    """
    else:
        banner = f"""
╔══════════════════════════════════════════════════════════════╗
║           超采集算系统 Super Collection Compute System        ║
║                    完整版 V{SYSTEM_VERSION} - 自适应集群                      ║
╠══════════════════════════════════════════════════════════════╣
║  三层分布式架构: 树莓派4B + Pico2 + Pico 集群                 ║
║  最大支持: 8×Pico2 + 128×Pico = 64路模拟 + 128路数字         ║
║  工作模式: 采样 / 破译 / 暴力破解 / 硬件测试                   ║
╚══════════════════════════════════════════════════════════════╝
    """
    print(banner)


def check_environment():
    """检查运行环境"""
    logger = get_logger("Main")
    
    logger.info("检查运行环境...")
    
    os.makedirs(LOG_DIR, exist_ok=True)
    os.makedirs(DATA_DIR, exist_ok=True)
    
    required_dirs = [
        "core",
        "daemons",
        "cluster",
        "modes",
        "services",
        "web",
    ]
    
    base_dir = os.path.dirname(os.path.abspath(__file__))
    all_ok = True
    
    for d in required_dirs:
        path = os.path.join(base_dir, d)
        if not os.path.exists(path):
            logger.warning(f"目录不存在: {d}")
            all_ok = False
    
    if RUN_ON_ZERO2W:
        logger.info("运行平台: 树莓派Zero 2W")
        logger.info("内存优化: 已启用")
        logger.info("硬件看门狗: 已禁用（使用软件看门狗）")
    else:
        logger.info("运行平台: 树莓派4B/其他")
    
    if all_ok:
        logger.info("环境检查通过")
    else:
        logger.warning("部分目录缺失，可能影响功能")
    
    return all_ok


def run_watchdog_mode():
    """以软件看门狗模式运行（Zero 2W推荐）"""
    logger = get_logger("Main")
    
    if RUN_ON_ZERO2W:
        logger.info("启动软件看门狗模式（Zero 2W）...")
    else:
        logger.info("启动看门狗模式...")
    
    from daemons.watchdog_daemon import WatchdogDaemon
    
    daemon = WatchdogDaemon()
    
    base_dir = os.path.dirname(os.path.abspath(__file__))
    python = sys.executable
    
    daemon.register_process(
        "data_receiver",
        [python, os.path.join(base_dir, "daemons", "data_receiver.py")],
        critical=True
    )
    daemon.register_process(
        "mode_scheduler",
        [python, os.path.join(base_dir, "daemons", "mode_scheduler.py")],
        critical=True
    )
    daemon.register_process(
        "business_logic",
        [python, os.path.join(base_dir, "daemons", "business_logic.py")],
        critical=True
    )
    
    if not RUN_ON_ZERO2W:
        daemon.register_process(
            "storage_remote",
            [python, os.path.join(base_dir, "daemons", "storage_remote.py")],
            critical=False
        )
        daemon.register_process(
            "hmi_task",
            [python, os.path.join(base_dir, "daemons", "hmi_task.py")],
            critical=False
        )
    
    daemon.register_process(
        "web_service",
        [python, os.path.join(base_dir, "services", "web_service.py")],
        critical=False
    )
    
    try:
        daemon.start()
    except KeyboardInterrupt:
        logger.info("收到中断信号，正在退出...")
        daemon.stop()


def run_standalone_mode():
    """单进程模式（用于调试）"""
    logger = get_logger("Main")
    logger.info("启动单进程调试模式...")
    
    import threading
    
    from core.usb_comm import usb_mgr
    from core.status_manager import status_mgr
    from cluster.adaptive_cluster import cluster_mgr
    from modes.mode_scheduler import mode_scheduler
    from daemons.data_receiver import DataReceiver
    from daemons.business_logic import SignalProcessor
    from daemons.storage_remote import StorageManager
    from services.web_service import WebService
    
    usb_mgr.init()
    
    status_mgr.update_system(
        work_mode="standby",
        run_status="Stop",
        start_time=time.time(),
    )
    
    cluster_mgr.init()
    mode_scheduler.init()
    
    receiver = DataReceiver()
    processor = SignalProcessor()
    storage = StorageManager()
    web = WebService()
    
    if RUN_ON_ZERO2W:
        from queue import Queue
        storage_queue = Queue(maxsize=DATA_QUEUE_SIZE // 4)
    else:
        from multiprocessing import Queue as MPQueue
        storage_queue = MPQueue(maxsize=DATA_QUEUE_SIZE)
    
    processor.set_queues(receiver.data_queue, None, storage_queue)
    storage.set_queue(storage_queue)
    web.set_data_queue(receiver.web_queue)
    
    threads = []
    
    def run_receiver():
        receiver._running = True
        receiver._rx_loop()
    
    def run_processor():
        processor._running = True
        processor._process_loop()
    
    def run_storage():
        storage._running = True
        storage._write_loop()
    
    def run_web():
        web.start()
    
    threads.append(threading.Thread(target=run_receiver, daemon=True))
    threads.append(threading.Thread(target=run_processor, daemon=True))
    
    if not RUN_ON_ZERO2W:
        threads.append(threading.Thread(target=run_storage, daemon=True))
    
    threads.append(threading.Thread(target=run_web, daemon=True))
    
    for t in threads:
        t.start()
    
    logger.info("系统启动完成，进入运行状态")
    
    try:
        while True:
            time.sleep(1)
            status_mgr.update_system(
                uptime=int(time.time() - status_mgr.get_system().get("start_time", time.time()))
            )
    except KeyboardInterrupt:
        logger.info("收到中断信号，正在退出...")
    finally:
        receiver.stop()
        processor.stop()
        storage.stop()
        web.stop()
        cluster_mgr.stop()
        mode_scheduler.stop()


def main():
    parser = argparse.ArgumentParser(
        description=f"{SYSTEM_NAME} V{SYSTEM_VERSION} - 完整版（Zero 2W适配）"
    )
    parser.add_argument(
        "--mode", "-m",
        choices=["watchdog", "standalone"],
        default="watchdog",
        help="运行模式: watchdog(多进程看门狗) / standalone(单进程调试)"
    )
    parser.add_argument(
        "--debug", "-d",
        action="store_true",
        help="启用调试模式"
    )
    parser.add_argument(
        "--version", "-v",
        action="version",
        version=f"{SYSTEM_NAME} V{SYSTEM_VERSION}"
    )
    
    args = parser.parse_args()
    
    print_banner()
    
    setup_logger(debug=args.debug or DEBUG)
    
    logger = get_logger("Main")
    logger.info(f"{SYSTEM_NAME} V{SYSTEM_VERSION} 启动")
    logger.info(f"运行模式: {args.mode}")
    logger.info(f"运行平台: {'树莓派Zero 2W' if RUN_ON_ZERO2W else '树莓派4B/其他'}")
    
    check_environment()
    
    if args.mode == "watchdog":
        run_watchdog_mode()
    else:
        run_standalone_mode()
    
    logger.info("系统已退出")


if __name__ == "__main__":
    main()