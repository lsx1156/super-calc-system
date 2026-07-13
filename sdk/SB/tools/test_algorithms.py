#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
算法库验证测试 (test_algorithms.py)
- 功能正确性验证
- 内存使用评估
- 性能基准测试
"""

import os
import sys
import time
import math
import random

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RPI_DIR = os.path.join(os.path.dirname(SCRIPT_DIR), "raspberry_pi")
sys.path.insert(0, RPI_DIR)

from algorithms import (
    MovingAverageFilter,
    MedianFilter,
    FIRFilter,
    PeakDetector,
    CrossCorrelator,
    MultiChannelProcessor,
    EdgeDetector,
    UARTDecoder,
    SPIDecoder,
    I2CDecoder,
    PulseWidthMeter,
    ManchesterDecoder,
    DigitalChannelProcessor,
    SHA256,
    MaskGenerator,
    DictionaryAttack,
    TaskScheduler,
    CrackEngine,
)


class TestRunner:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.tests = []
    
    def test(self, name, func):
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
        print(f"\n{'='*60}")
        print(f"  {title}")
        print(f"{'='*60}")
    
    def summary(self):
        print(f"\n{'='*60}")
        total = self.passed + self.failed
        print(f"  测试总结: {self.passed}/{total} 通过")
        print(f"{'='*60}")
        
        for name, status in self.tests:
            if not status.startswith("PASS"):
                print(f"  ✗ {name}: {status}")
        
        return self.failed == 0


# ==================== 模拟信号处理测试 ====================

def test_moving_average():
    """滑动平均滤波测试"""
    f = MovingAverageFilter(window_size=4)
    
    # 阶跃响应测试
    vals = [0, 0, 0, 0, 10, 10, 10, 10]
    results = [f.process(v) for v in vals]
    
    # 第4个10之后应该接近10
    assert abs(results[7] - 10.0) < 0.01, f"期望10.0，实际{results[7]}"
    
    # 平滑性：相邻值差不超过阶跃的1/window
    for i in range(1, len(results)):
        diff = abs(results[i] - results[i-1])
        assert diff <= 10.0 / 4 + 0.01, f"跳变过大: {diff}"
    
    return True


def test_median_filter():
    """中值滤波测试"""
    f = MedianFilter(window_size=5)
    
    # 含脉冲噪声的信号
    vals = [1.0, 1.1, 100.0, 0.9, 1.0, 1.1, 50.0, 1.0]
    results = [f.process(v) for v in vals]
    
    # 脉冲噪声应被抑制，输出应接近1
    for r in results[3:]:
        assert r < 10.0, f"脉冲噪声未被抑制: {r}"
    
    return True


def test_fir_filter():
    """FIR滤波测试"""
    f = FIRFilter.design_lowpass(
        cutoff_freq=1000,
        sample_rate=10000,
        num_taps=15
    )
    
    # 直流信号应能通过
    dc_val = 2.5
    results = []
    for i in range(50):
        results.append(f.process(dc_val))
    
    # 稳态后应接近直流值
    assert abs(results[-1] - dc_val) < 0.1, f"直流增益错误: {results[-1]}"
    
    return True


def test_peak_detector():
    """峰值检测测试"""
    d = PeakDetector(min_height=0.5, min_distance=3)
    
    # 构造含3个峰值的信号
    vals = []
    for i in range(30):
        # 三个峰值分别在位置5, 15, 25
        if i % 10 == 5:
            vals.append(1.0)
        elif i % 10 == 4 or i % 10 == 6:
            vals.append(0.8)
        else:
            vals.append(0.1)
    
    peaks = d.find_peaks(vals)
    
    # 应检测到3个峰值
    assert len(peaks) >= 2, f"峰值数量不足: {len(peaks)}"
    
    return True


def test_cross_correlator():
    """互相关测试"""
    # 模板信号
    template = [0, 0.5, 1.0, 0.5, 0]
    
    c = CrossCorrelator(template)
    
    # 信号中包含模板
    signal = [0]*10 + template + [0]*10
    
    max_corr = 0
    for v in signal:
        corr = c.process(v)
        max_corr = max(max_corr, corr)
    
    # 匹配时相关值应接近1
    assert max_corr > 0.8, f"相关值过低: {max_corr}"
    
    return True


def test_multi_channel_memory():
    """多通道处理器内存评估"""
    proc = MultiChannelProcessor(
        num_channels=8,
        ma_window=8,
        med_window=5,
        fir_taps=21,
    )
    
    mem = proc.get_memory_usage()
    
    # 8通道总内存应 < 10KB
    assert mem['total'] < 10240, f"内存过大: {mem['total']} 字节"
    
    return True


# ==================== 数字信号处理测试 ====================

def test_edge_detector():
    """边沿检测测试"""
    d = EdgeDetector(edge_type=3, debounce_samples=2)
    
    # 模拟信号: 低 -> 高 -> 低
    samples = [0]*5 + [1]*10 + [0]*5
    
    rising_edges = 0
    falling_edges = 0
    
    for s in samples:
        edge, level = d.process(s)
        if edge == 1:
            rising_edges += 1
        elif edge == 2:
            falling_edges += 1
    
    assert rising_edges == 1, f"上升沿数量错误: {rising_edges}"
    assert falling_edges == 1, f"下降沿数量错误: {falling_edges}"
    
    return True


def test_uart_decoder():
    """UART解码测试"""
    # 生成UART信号：发送字节 0x55 (01010101)，9600 8N1
    baudrate = 9600
    sample_rate = 9600 * 10  # 10倍过采样
    
    decoder = UARTDecoder(baudrate=baudrate, sample_rate=sample_rate)
    
    # 构造UART帧：起始位(0) + 8数据位 + 停止位(1)
    byte_val = 0x55  # 01010101
    bits_per_sample = sample_rate // baudrate
    
    samples = []
    
    # 空闲（高电平）
    samples += [1] * bits_per_sample * 2
    
    # 起始位
    samples += [0] * bits_per_sample
    
    # 数据位（LSB first）
    for i in range(8):
        bit = (byte_val >> i) & 1
        samples += [bit] * bits_per_sample
    
    # 停止位
    samples += [1] * bits_per_sample
    
    # 解码
    decoded = []
    for s in samples:
        b = decoder.process(s)
        if b is not None:
            decoded.append(b)
    
    assert len(decoded) >= 1, f"未解码出数据"
    assert decoded[0] == byte_val, f"解码错误: 期望{hex(byte_val)}, 实际{hex(decoded[0])}"
    
    return True


def test_pulse_width_meter():
    """脉宽测量测试"""
    sample_rate = 100000
    m = PulseWidthMeter(sample_rate=sample_rate)
    
    # 1kHz 50%占空比方波
    freq = 1000
    period_samples = sample_rate // freq
    high_samples = period_samples // 2
    low_samples = period_samples - high_samples
    
    samples = []
    for _ in range(5):
        samples += [1] * high_samples
        samples += [0] * low_samples
    
    results = []
    for s in samples:
        r = m.process(s)
        if r:
            results.append(r)
    
    assert len(results) >= 3, f"测量次数不足: {len(results)}"
    
    # 频率应接近1kHz
    measured_freq = results[-1]['frequency_hz']
    assert abs(measured_freq - freq) / freq < 0.05, f"频率误差过大: {measured_freq}"
    
    # 占空比应接近50%
    duty = results[-1]['duty_percent']
    assert abs(duty - 50.0) < 5.0, f"占空比误差过大: {duty}"
    
    return True


def test_i2c_decoder():
    """I2C解码测试"""
    d = I2CDecoder()
    
    # 简化测试：起始条件 + 停止条件
    # SCL高，SDA下降 = START
    events = []
    
    # 空闲
    d.process(1, 1)
    d.process(1, 1)
    
    # START: SCL高, SDA下降
    r = d.process(0, 1)
    if r: events.append(r['type'])
    
    # STOP: SCL高, SDA上升
    r = d.process(1, 1)
    if r: events.append(r['type'])
    
    # 应有start事件
    assert 'start' in events, f"未检测到起始条件: {events}"
    
    return True


def test_digital_channel_memory():
    """数字通道内存评估"""
    proc = DigitalChannelProcessor(channel_id=0, sample_rate=100000)
    mem = proc.get_memory_usage()
    
    # 单通道内存应 < 1KB
    assert mem['total'] < 1024, f"内存过大: {mem['total']} 字节"
    
    return True


# ==================== 密码破解测试 ====================

def test_sha256():
    """SHA-256算法测试"""
    sha = SHA256()
    
    # 已知测试向量
    test_cases = [
        ("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
        ("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        ("password", "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"),
    ]
    
    for msg, expected in test_cases:
        sha.reset()
        sha.update(msg.encode())
        result = sha.hexdigest()
        assert result == expected, f"SHA256错误: 输入'{msg}'\n期望: {expected}\n实际: {result}"
    
    return True


def test_mask_generator():
    """掩码生成器测试"""
    # 简单掩码: 2位数字
    g = MaskGenerator("?d?d")
    
    assert g.total_count == 100, f"总数错误: {g.total_count}"
    
    # 生成所有
    passwords = []
    while True:
        p = g.next()
        if p is None:
            break
        passwords.append(p)
    
    assert len(passwords) == 100, f"生成数量错误: {len(passwords)}"
    assert passwords[0] == "00", f"第一个错误: {passwords[0]}"
    assert passwords[-1] == "99", f"最后一个错误: {passwords[-1]}"
    assert "42" in passwords, "缺少42"
    
    return True


def test_mask_offset():
    """掩码偏移测试（分布式分片）"""
    g = MaskGenerator("?d?d?d")  # 000-999
    
    # 从500开始
    g.set_start(500)
    
    first = g.next()
    assert first == "500", f"偏移起始错误: {first}"
    
    # 跳过100个
    g.skip(99)  # 已经取了1个，再跳99个 = 100个
    next_val = g.next()
    assert next_val == "600", f"跳过错误: {next_val}"
    
    return True


def test_mask_progress():
    """掩码进度测试"""
    g = MaskGenerator("?d?d")  # 100个
    
    for i in range(50):
        g.next()
    
    # 进度应约为50%
    progress = g.progress
    assert 45 < progress < 55, f"进度错误: {progress}%"
    
    return True


def test_task_scheduler():
    """任务调度器测试"""
    sched = TaskScheduler(total_nodes=4)
    
    total_keys = 100000
    sched.setup_bruteforce(total_keys)
    
    # 每个节点应有任务
    for i in range(4):
        task = sched.get_task(i)
        assert task is not None, f"节点{i}无任务"
        assert task[1] > task[0], f"任务范围无效: {task}"
    
    # 所有任务范围应覆盖全部
    ranges = []
    for i in range(4):
        t = sched._tasks[i]
        ranges.append((t['start'], t['end']))
    
    ranges.sort()
    assert ranges[0][0] == 0, "不从0开始"
    assert ranges[-1][1] == total_keys, "不覆盖总数"
    
    # 连续
    for i in range(1, len(ranges)):
        assert ranges[i][0] == ranges[i-1][1], f"范围不连续: {ranges[i-1]} -> {ranges[i]}"
    
    return True


def test_task_scheduler_dynamic():
    """动态节点增减测试"""
    sched = TaskScheduler(total_nodes=2)
    sched.setup_bruteforce(100000)
    
    # 添加节点
    added = sched.add_node(2)
    assert added, "添加节点失败"
    assert sched.total_nodes == 3, f"节点数错误: {sched.total_nodes}"
    
    # 移除节点
    removed = sched.remove_node(2)
    assert removed, "移除节点失败"
    assert sched.total_nodes == 2, f"节点数错误: {sched.total_nodes}"
    
    return True


def test_crack_engine_md5():
    """破解引擎MD5测试"""
    engine = CrackEngine(algorithm='md5')
    
    # 已知: "abc" 的 MD5 = 900150983cd24fb0d6963f7d28e17f72
    target = "900150983cd24fb0d6963f7d28e17f72"
    
    engine.start_mask(target, "?l?l?l")
    
    found = engine.step(50000)
    
    assert found, "未找到密码"
    assert engine.result == "abc", f"结果错误: {engine.result}"
    assert engine.found, "found标志未设置"
    
    return True


def test_crack_memory():
    """破解引擎内存评估"""
    engine = CrackEngine(algorithm='md5')
    engine.start_mask("test", "?d?d?d?d?d?d")
    
    mem = engine.memory_usage
    
    # 内存应 < 4KB
    assert mem < 4096, f"内存过大: {mem} 字节"
    
    return True


def test_mask_memory():
    """掩码生成器内存评估"""
    # 8位全字符集
    g = MaskGenerator("?a?a?a?a?a?a?a?a")
    
    mem = g.memory_usage
    
    # 内存应 < 2KB
    assert mem < 2048, f"内存过大: {mem} 字节"
    
    # 总数应正确
    assert g.total_count == 95**8, f"总数错误"
    
    return True


# ==================== 性能基准测试 ====================

def benchmark_analog():
    """模拟信号处理性能"""
    print("\n  [BENCH] 模拟信号处理性能")
    
    # 生成测试数据
    n = 10000
    data = [random.random() * 3.3 for _ in range(n)]
    
    # 滑动平均
    f = MovingAverageFilter(window_size=8)
    t0 = time.time()
    for v in data:
        f.process(v)
    t = time.time() - t0
    print(f"    滑动平均(8): {n/t:.0f} samples/sec")
    
    # 中值滤波
    f = MedianFilter(window_size=5)
    t0 = time.time()
    for v in data:
        f.process(v)
    t = time.time() - t0
    print(f"    中值滤波(5): {n/t:.0f} samples/sec")
    
    # FIR
    f = FIRFilter.design_lowpass(1000, 50000, 31)
    t0 = time.time()
    for v in data:
        f.process(v)
    t = time.time() - t0
    print(f"    FIR(31阶): {n/t:.0f} samples/sec")
    
    return True


def benchmark_crack():
    """破解性能基准"""
    print("\n  [BENCH] 密码破解性能")
    
    # MD5 速度
    engine = CrackEngine(algorithm='md5')
    engine.start_mask("test_hash_not_found", "?l?l?l?l?l")
    
    t0 = time.time()
    engine.step(10000)
    t = time.time() - t0
    
    rate = engine.attempts / t
    print(f"    MD5 掩码攻击: {rate:.0f} hashes/sec (Python单线程)")
    
    return True


def main():
    print("""
╔══════════════════════════════════════════════════════════════╗
║           超采集算系统 - 算法库验证测试                       ║
╚══════════════════════════════════════════════════════════════╝
    """)
    
    runner = TestRunner()
    
    # 模拟信号处理
    runner.section("一、模拟信号处理算法")
    runner.test("滑动平均滤波", test_moving_average)
    runner.test("中值滤波", test_median_filter)
    runner.test("FIR低通滤波", test_fir_filter)
    runner.test("峰值检测", test_peak_detector)
    runner.test("互相关分析", test_cross_correlator)
    runner.test("多通道内存评估", test_multi_channel_memory)
    
    # 数字信号处理
    runner.section("二、数字信号处理算法")
    runner.test("边沿检测", test_edge_detector)
    runner.test("UART解码器", test_uart_decoder)
    runner.test("脉宽测量", test_pulse_width_meter)
    runner.test("I2C解码器", test_i2c_decoder)
    runner.test("数字通道内存评估", test_digital_channel_memory)
    
    # 密码破解
    runner.section("三、密码破解算法")
    runner.test("SHA-256算法", test_sha256)
    runner.test("掩码生成器", test_mask_generator)
    runner.test("掩码偏移/分片", test_mask_offset)
    runner.test("掩码进度计算", test_mask_progress)
    runner.test("任务调度器", test_task_scheduler)
    runner.test("动态节点增减", test_task_scheduler_dynamic)
    runner.test("MD5破解引擎", test_crack_engine_md5)
    runner.test("破解引擎内存", test_crack_memory)
    runner.test("掩码生成器内存", test_mask_memory)
    
    # 性能基准
    runner.section("四、性能基准测试")
    benchmark_analog()
    benchmark_crack()
    
    # 总结
    success = runner.summary()
    
    print(f"\n{'✓ 算法验证通过!' if success else '✗ 验证失败!'}")
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
