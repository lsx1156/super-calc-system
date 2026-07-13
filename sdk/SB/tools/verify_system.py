#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
系统验证测试脚本 (verify_system.py)
- 模块导入测试
- 配置验证
- 集群管理功能测试
- 多模式调度测试
- 状态管理测试
"""

import os
import sys
import time
import json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RPI_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "raspberry_pi")
sys.path.insert(0, RPI_DIR)


class SystemVerifier:
    """系统验证器"""
    
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.tests = []
    
    def test(self, name, func):
        """执行单个测试"""
        try:
            result = func()
            if result:
                self.passed += 1
                status = "PASS"
            else:
                self.failed += 1
                status = "FAIL"
        except Exception as e:
            self.failed += 1
            status = f"ERROR: {e}"
        
        self.tests.append((name, status))
        print(f"  [{status[:4]:4s}] {name}")
        return status.startswith("PASS")
    
    def section(self, title):
        """测试分组"""
        print(f"\n{'='*60}")
        print(f"  {title}")
        print(f"{'='*60}")
    
    def summary(self):
        """测试总结"""
        print(f"\n{'='*60}")
        print(f"  测试总结: {self.passed}/{self.passed + self.failed} 通过")
        print(f"{'='*60}")
        
        for name, status in self.tests:
            if not status.startswith("PASS"):
                print(f"  ✗ {name}: {status}")
        
        return self.failed == 0


def test_config_module():
    """测试配置模块"""
    try:
        from core.config import (
            SYSTEM_VERSION, SYSTEM_NAME,
            MAX_PICO2_COUNT, MAX_PICO_PER_PICO2, MAX_TOTAL_PICO,
            WorkMode, MODE_NAMES, CMD, DATA_TYPE,
            WEB_HOST, WEB_PORT,
        )
        
        assert SYSTEM_VERSION is not None
        assert SYSTEM_NAME is not None
        assert MAX_PICO2_COUNT == 8
        assert MAX_PICO_PER_PICO2 == 16
        assert MAX_TOTAL_PICO == 128
        
        assert hasattr(WorkMode, 'SAMPLE')
        assert hasattr(WorkMode, 'CRACK')
        assert hasattr(WorkMode, 'BRUTEFORCE')
        assert hasattr(WorkMode, 'HW_TEST')
        assert hasattr(WorkMode, 'STANDBY')
        
        assert isinstance(CMD, dict)
        assert isinstance(DATA_TYPE, dict)
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_logger_module():
    """测试日志模块"""
    try:
        from core.logger import get_logger, setup_logger
        
        logger = get_logger("Test")
        assert logger is not None
        
        logger.info("测试日志")
        logger.debug("调试日志")
        logger.warning("警告日志")
        logger.error("错误日志")
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_status_manager():
    """测试状态管理器"""
    try:
        from core.status_manager import status_mgr
        
        status = status_mgr.get_all()
        assert "system" in status
        assert "cluster" in status
        assert "sample" in status
        assert "crack" in status
        assert "hardware" in status
        assert "performance" in status
        
        status_mgr.update_system(work_mode="test", run_status="Test")
        sys_status = status_mgr.get_system()
        assert sys_status["work_mode"] == "test"
        assert sys_status["run_status"] == "Test"
        
        status_mgr.update_cluster(
            pico2_count=2,
            pico2_online=2,
            total_pico=16,
            online_pico=16,
        )
        cluster = status_mgr.get_cluster()
        assert cluster["pico2_count"] == 2
        assert cluster["total_pico"] == 16
        
        status_json = status_mgr.to_json()
        data = json.loads(status_json)
        assert isinstance(data, dict)
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_cluster_manager():
    """测试集群管理器"""
    try:
        from cluster.adaptive_cluster import (
            AdaptiveClusterManager, PicoNode, Pico2Node
        )
        
        cluster = AdaptiveClusterManager()
        
        pico2 = Pico2Node(0)
        assert pico2.id == 0
        assert pico2.online == False
        
        pico = PicoNode(0, 0)
        assert pico.pico2_id == 0
        assert pico.node_id == 0
        
        pico2.nodes[0] = pico
        
        pico_dict = pico.to_dict()
        assert "pico2_id" in pico_dict
        assert "node_id" in pico_dict
        
        pico2_dict = pico2.to_dict()
        assert "id" in pico2_dict
        assert "nodes" in pico2_dict
        
        cluster.pico2_nodes[0] = pico2
        online = cluster.get_total_online()
        assert online >= 0
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_mode_scheduler():
    """测试模式调度器"""
    try:
        from modes.mode_scheduler import ModeScheduler, ModeState
        from core.config import WorkMode
        
        scheduler = ModeScheduler()
        
        assert scheduler.get_current_mode() == WorkMode.STANDBY
        assert scheduler.get_state() == ModeState.IDLE
        assert scheduler.is_running() == False
        
        assert WorkMode.SAMPLE in scheduler.mode_configs
        assert WorkMode.CRACK in scheduler.mode_configs
        assert WorkMode.BRUTEFORCE in scheduler.mode_configs
        assert WorkMode.HW_TEST in scheduler.mode_configs
        
        code = scheduler._mode_to_code(WorkMode.SAMPLE)
        assert code == 0x00
        
        code = scheduler._mode_to_code(WorkMode.BRUTEFORCE)
        assert code == 0x02
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_business_logic():
    """测试业务逻辑模块"""
    try:
        from daemons.business_logic import SignalProcessor
        
        processor = SignalProcessor(filter_enabled=True)
        assert processor._running == False
        assert processor._filter_enabled == True
        assert len(processor._ma_filters) == 64
        assert len(processor._fir_filters) == 64
        
        packet = {
            "pico2_id": 0,
            "data_type": 0x10,
            "data": bytes([0x01, 0x02, 0x00]),
            "timestamp": time.time(),
        }
        
        result = processor._process_packet(packet)
        assert result is not None
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_crack_engine():
    """测试破解引擎"""
    try:
        from daemons.crack_engine_daemon import CrackEngineDaemon
        
        engine = CrackEngineDaemon()
        assert engine.is_active() == False
        
        progress = engine.get_progress()
        assert "active" in progress
        assert "total" in progress
        assert "processed" in progress
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_analog_algorithms():
    """测试模拟信号处理算法"""
    try:
        from algorithms.analog_algorithms import (
            MovingAverageFilter,
            MedianFilter,
            FIRFilter,
            PeakDetector,
        )
        
        ma = MovingAverageFilter(window_size=8)
        assert ma.process(1.0) is not None
        
        med = MedianFilter(window_size=5)
        assert med.process(1.0) is not None
        
        fir = FIRFilter.design_lowpass(1000, 10000, 21)
        assert fir.process(1.0) is not None
        
        pd = PeakDetector(min_height=0.1, min_distance=10)
        is_peak, val = pd.process(0.5)
        assert isinstance(is_peak, bool)
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_digital_algorithms():
    """测试数字信号处理算法"""
    try:
        from algorithms.digital_algorithms import (
            EdgeDetector,
            UARTDecoder,
            SPIDecoder,
            I2CDecoder,
            PulseWidthMeter,
        )
        
        ed = EdgeDetector(debounce_samples=3)
        edge, level = ed.process(0)
        assert edge >= 0
        
        uart = UARTDecoder(baudrate=9600, sample_rate=96000)
        assert uart is not None
        
        spi = SPIDecoder()
        assert spi is not None
        
        i2c = I2CDecoder()
        assert i2c is not None
        
        pwm = PulseWidthMeter(sample_rate=100000)
        info = pwm.process(0)
        assert info is None or isinstance(info, dict)
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_crack_algorithms():
    """测试密码破解算法"""
    try:
        from algorithms.crack_algorithms import (
            SHA256,
            MaskGenerator,
            TaskScheduler,
            CrackEngine,
        )
        
        sha = SHA256()
        sha.update(b"test")
        digest = sha.hexdigest()
        assert len(digest) == 64
        
        gen = MaskGenerator("?l?l?l?l")
        assert gen.total_count == 26**4
        
        gen.set_start(0)
        pwd = gen.next()
        assert pwd == "aaaa"
        
        scheduler = TaskScheduler(total_nodes=4)
        assert scheduler.total_nodes == 4
        
        engine = CrackEngine(algorithm='md5')
        assert engine is not None
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_optimized_algorithms():
    """测试优化算法库"""
    try:
        from algorithms.optimized_algorithms import (
            VectorizedMovingAverage,
            VectorizedFIR,
            VectorizedPeakDetector,
            SignalFeatureExtractor,
            SignalAutoIdentifier,
            HashAutoIdentifier,
            ProtocolAutoIdentifier,
        )
        import numpy as np
        
        data = np.random.randn(1000)
        
        ma = VectorizedMovingAverage(num_channels=1, window_size=8)
        result = ma.process_batch(data)
        assert len(result) == len(data)
        
        fir = VectorizedFIR.design_lowpass(1000, 10000, 31, num_channels=1)
        result = fir.process_batch(data)
        assert len(result) == len(data)
        
        pd = VectorizedPeakDetector(min_height=0.1)
        peaks = pd.detect_batch(data)
        assert len(peaks) == 1
        
        extractor = SignalFeatureExtractor()
        features = extractor.extract_features(data, 10000)
        assert 'mean' in features
        assert 'kurtosis' in features
        
        identifier = SignalAutoIdentifier(sample_rate=10000)
        sig_type, conf, feat = identifier.identify(data)
        assert sig_type in identifier.SIGNAL_TYPES
        
        hash_type, conf = HashAutoIdentifier.identify("098f6bcd4621d373cade4e832627b4f6")
        assert hash_type == 'md5'
        
        proto = ProtocolAutoIdentifier(sample_rate=100000)
        assert proto is not None
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_parallel_processing():
    """测试并行处理框架"""
    try:
        from algorithms.parallel_processing import (
            ParallelProcessor,
            MultiChannelProcessor,
            BatchStreamProcessor,
        )
        
        proc = ParallelProcessor(max_workers=2, mode='thread')
        proc.start()
        
        def square(x):
            return x * x
        
        results = proc.map(square, [1, 2, 3, 4, 5])
        assert len(results) == 5
        assert results[0].data == 1
        
        proc.stop()
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_storage_manager():
    """测试存储管理器"""
    try:
        from daemons.storage_remote import StorageManager, AlertManager
        
        storage = StorageManager()
        assert storage._running == False
        
        test_config = {"test": True, "value": 42}
        result = storage.save_config(test_config, "test_config.json")
        assert result == True
        
        loaded = storage.load_config("test_config.json")
        assert loaded is not None
        assert loaded["test"] == True
        assert loaded["value"] == 42
        
        alert = AlertManager()
        alert.check_battery(100)
        alert.check_battery(10)
        
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def test_module_imports():
    """测试所有模块导入"""
    modules = [
        "core.config",
        "core.logger",
        "core.status_manager",
        "cluster.adaptive_cluster",
        "modes.mode_scheduler",
        "algorithms.analog_algorithms",
        "algorithms.digital_algorithms",
        "algorithms.crack_algorithms",
        "algorithms.optimized_algorithms",
        "algorithms.parallel_processing",
        "daemons.data_receiver",
        "daemons.business_logic",
        "daemons.crack_engine_daemon",
        "daemons.storage_remote",
        "daemons.watchdog_daemon",
        "daemons.mode_scheduler",
        "daemons.hmi_task",
    ]
    
    failed = []
    for mod in modules:
        try:
            __import__(mod)
        except Exception as e:
            failed.append(f"{mod}: {e}")
    
    if failed:
        for f in failed:
            print(f"    ✗ {f}")
        return False
    
    return True


def test_directory_structure():
    """测试目录结构"""
    expected_dirs = [
        "raspberry_pi/core",
        "raspberry_pi/daemons",
        "raspberry_pi/cluster",
        "raspberry_pi/modes",
        "raspberry_pi/services",
        "raspberry_pi/web/templates",
        "raspberry_pi/web/static/css",
        "raspberry_pi/web/static/js",
        "pico/firmware/include",
        "pico/firmware/src",
        "pico2/firmware/include",
        "pico2/firmware/src",
        "tools",
    ]
    
    project_root = os.path.dirname(SCRIPT_DIR)
    missing = []
    
    for d in expected_dirs:
        path = os.path.join(project_root, d)
        if not os.path.exists(path):
            missing.append(d)
    
    if missing:
        for m in missing:
            print(f"    缺少目录: {m}")
        return False
    
    return True


def test_pico_firmware_files():
    """测试Pico固件文件"""
    project_root = os.path.dirname(SCRIPT_DIR)
    pico_dir = os.path.join(project_root, "pico", "firmware")
    
    expected_src = [
        "src/main.c",
        "src/adc_sample.c",
        "src/digital_capture.c",
        "src/spi_comm.c",
        "src/overclock.c",
        "src/crack_engine.c",
        "src/hw_test.c",
        "src/status_mgr.c",
    ]
    
    expected_inc = [
        "include/config.h",
        "include/adc_sample.h",
        "include/digital_capture.h",
        "include/spi_comm.h",
        "include/overclock.h",
        "include/crack_engine.h",
        "include/hw_test.h",
        "include/status_mgr.h",
    ]
    
    missing = []
    for f in expected_src + expected_inc:
        path = os.path.join(pico_dir, f)
        if not os.path.exists(path):
            missing.append(f)
    
    if missing:
        for m in missing:
            print(f"    缺少文件: pico/firmware/{m}")
        return False
    
    return True


def test_pico2_firmware_files():
    """测试Pico2固件文件"""
    project_root = os.path.dirname(SCRIPT_DIR)
    pico2_dir = os.path.join(project_root, "pico2", "firmware")
    
    expected_src = [
        "src/main.c",
        "src/spi_master.c",
        "src/usb_cdc_comm.c",
        "src/data_aggregator.c",
        "src/oc_control.c",
        "src/cluster_mgr.c",
        "src/system_status.c",
        "src/foolproof.c",
    ]
    
    expected_inc = [
        "include/config.h",
        "include/spi_master.h",
        "include/usb_cdc_comm.h",
        "include/data_aggregator.h",
        "include/oc_control.h",
        "include/cluster_mgr.h",
        "include/system_status.h",
        "include/foolproof.h",
    ]
    
    missing = []
    for f in expected_src + expected_inc:
        path = os.path.join(pico2_dir, f)
        if not os.path.exists(path):
            missing.append(f)
    
    if missing:
        for m in missing:
            print(f"    缺少文件: pico2/firmware/{m}")
        return False
    
    return True


def test_web_service():
    """测试Web服务模块"""
    try:
        from services.web_service import WebService
        
        web = WebService()
        assert web.app is not None
        assert web.socketio is not None
        
        rules = [rule.rule for rule in web.app.url_map.iter_rules()]
        
        expected_routes = [
            '/',
            '/api/status',
            '/api/system',
            '/api/cluster',
            '/api/mode/current',
            '/api/mode/switch',
        ]
        
        for route in expected_routes:
            if route not in rules:
                print(f"    缺少路由: {route}")
                return False
        
        return True
    except ImportError as e:
        print(f"    跳过 (缺少依赖): {e}")
        return True
    except Exception as e:
        print(f"    错误: {e}")
        return False


def main():
    print("""
╔══════════════════════════════════════════════════════════════╗
║           超采集算系统 - 完整版验证测试                       ║
╚══════════════════════════════════════════════════════════════╝
    """)
    
    v = SystemVerifier()
    
    v.section("1. 目录结构验证")
    v.test("工程目录结构完整性", test_directory_structure)
    
    v.section("2. Pico固件文件验证")
    v.test("Pico终端固件文件", test_pico_firmware_files)
    v.test("Pico2协处理器固件文件", test_pico2_firmware_files)
    
    v.section("3. 核心模块验证")
    v.test("配置模块", test_config_module)
    v.test("日志模块", test_logger_module)
    v.test("状态管理器", test_status_manager)
    
    v.section("4. 集群管理验证")
    v.test("集群管理器", test_cluster_manager)
    
    v.section("5. 多模式调度验证")
    v.test("模式调度器", test_mode_scheduler)
    
    v.section("6. 业务模块验证")
    v.test("业务逻辑处理器", test_business_logic)
    v.test("破解引擎", test_crack_engine)
    v.test("存储管理器", test_storage_manager)
    
    v.section("7. 算法模块验证")
    v.test("模拟信号处理算法", test_analog_algorithms)
    v.test("数字信号处理算法", test_digital_algorithms)
    v.test("密码破解算法", test_crack_algorithms)
    v.test("优化算法库", test_optimized_algorithms)
    v.test("并行处理框架", test_parallel_processing)
    
    v.section("8. Web服务验证")
    v.test("Web服务模块", test_web_service)
    
    v.section("9. 完整模块导入测试")
    v.test("所有模块导入", test_module_imports)
    
    success = v.summary()
    
    print(f"\n{'✓ 验证通过!' if success else '✗ 验证失败!'}")
    
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
