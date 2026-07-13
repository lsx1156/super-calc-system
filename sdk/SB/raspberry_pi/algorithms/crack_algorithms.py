#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
密码破解算法库 (crack_algorithms.py)
- SHA-256 算法实现（纯Python，便于Pico移植参考）
- 掩码攻击 (Mask Attack)
- 字典攻击 (Dictionary Attack)
- 分布式任务分片调度
- 进度估算与ETA计算

内存设计原则：
- 所有缓冲区固定大小
- 字典分片加载，不全量载入
- 任务状态机 < 1KB
"""

import struct
import hashlib
import math
import time
from typing import List, Optional, Tuple, Dict, Callable
from collections import deque


# ==================== SHA-256 算法实现 ====================

class SHA256:
    """
    SHA-256 算法实现（纯Python）
    - 用于验证和算法参考
    - 实际Pico端用C实现，逻辑相同
    - 内存：约 256字节（状态+缓冲区）
    """
    
    K = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ]
    
    def __init__(self):
        self._state = [
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
        ]
        self._buffer = bytearray(64)
        self._buffer_len = 0
        self._total_len = 0
    
    def _rotr(self, x: int, n: int) -> int:
        return ((x >> n) | (x << (32 - n))) & 0xFFFFFFFF
    
    def _ch(self, x: int, y: int, z: int) -> int:
        return (x & y) ^ (~x & z)
    
    def _maj(self, x: int, y: int, z: int) -> int:
        return (x & y) ^ (x & z) ^ (y & z)
    
    def _sig0(self, x: int) -> int:
        return self._rotr(x, 2) ^ self._rotr(x, 13) ^ self._rotr(x, 22)
    
    def _sig1(self, x: int) -> int:
        return self._rotr(x, 6) ^ self._rotr(x, 11) ^ self._rotr(x, 25)
    
    def _gamma0(self, x: int) -> int:
        return self._rotr(x, 7) ^ self._rotr(x, 18) ^ (x >> 3)
    
    def _gamma1(self, x: int) -> int:
        return self._rotr(x, 17) ^ self._rotr(x, 19) ^ (x >> 10)
    
    def _compress(self, block: bytes):
        w = [0] * 64
        
        for i in range(16):
            w[i] = struct.unpack('>I', block[i*4:(i+1)*4])[0]
        
        for i in range(16, 64):
            w[i] = (self._gamma1(w[i-2]) + w[i-7] + 
                    self._gamma0(w[i-15]) + w[i-16]) & 0xFFFFFFFF
        
        a, b, c, d, e, f, g, h = self._state
        
        for i in range(64):
            t1 = h + self._sig1(e) + self._ch(e, f, g) + self.K[i] + w[i]
            t2 = self._sig0(a) + self._maj(a, b, c)
            h = g
            g = f
            f = e
            e = (d + t1) & 0xFFFFFFFF
            d = c
            c = b
            b = a
            a = (t1 + t2) & 0xFFFFFFFF
        
        self._state[0] = (self._state[0] + a) & 0xFFFFFFFF
        self._state[1] = (self._state[1] + b) & 0xFFFFFFFF
        self._state[2] = (self._state[2] + c) & 0xFFFFFFFF
        self._state[3] = (self._state[3] + d) & 0xFFFFFFFF
        self._state[4] = (self._state[4] + e) & 0xFFFFFFFF
        self._state[5] = (self._state[5] + f) & 0xFFFFFFFF
        self._state[6] = (self._state[6] + g) & 0xFFFFFFFF
        self._state[7] = (self._state[7] + h) & 0xFFFFFFFF
    
    def update(self, data: bytes):
        self._total_len += len(data)
        pos = 0
        
        while pos < len(data):
            space = 64 - self._buffer_len
            take = min(space, len(data) - pos)
            self._buffer[self._buffer_len:self._buffer_len + take] = data[pos:pos + take]
            self._buffer_len += take
            pos += take
            
            if self._buffer_len == 64:
                self._compress(bytes(self._buffer))
                self._buffer_len = 0
    
    def digest(self) -> bytes:
        # Padding
        pad = bytearray()
        pad.append(0x80)
        
        while (self._buffer_len + len(pad)) % 64 != 56:
            pad.append(0)
        
        bit_len = self._total_len * 8
        pad += struct.pack('>Q', bit_len)
        
        self.update(bytes(pad))
        
        result = b''
        for s in self._state:
            result += struct.pack('>I', s)
        return result
    
    def hexdigest(self) -> str:
        return self.digest().hex()
    
    def reset(self):
        self._state = [
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
        ]
        self._buffer_len = 0
        self._total_len = 0
    
    @property
    def memory_usage(self) -> int:
        return 256 + 64  # 状态 + 缓冲区


# ==================== 掩码攻击 (Mask Attack) ====================

class MaskGenerator:
    """
    掩码生成器
    - 掩码语法：?l?d?u?s
      ?l = 小写字母 a-z
      ?u = 大写字母 A-Z
      ?d = 数字 0-9
      ?s = 特殊字符 !@#$%...
      ?a = 所有可打印字符
    - 固定内存，增量生成
    - 支持起始偏移，便于分布式分片
    """
    
    CHARSET_LOWER = 'abcdefghijklmnopqrstuvwxyz'
    CHARSET_UPPER = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    CHARSET_DIGIT = '0123456789'
    CHARSET_SPECIAL = '!@#$%^&*()-_=+[]{}|;:,.<>?\'"/\\`~'
    CHARSET_SPACE = ' '
    CHARSET_ALL = ''.join(chr(i) for i in range(0x20, 0x7F))  # 95个可打印ASCII
    
    def __init__(self, mask: str, custom_chars: Optional[str] = None):
        self.mask = mask
        self._charsets = []
        self._positions = []
        self._total_count = 1
        self._current = 0
        self._done = False
        
        self._parse_mask(custom_chars)
    
    def _parse_mask(self, custom_chars: Optional[str]):
        i = 0
        while i < len(self.mask):
            if self.mask[i] == '?' and i + 1 < len(self.mask):
                c = self.mask[i + 1]
                if c == 'l':
                    self._charsets.append(self.CHARSET_LOWER)
                elif c == 'u':
                    self._charsets.append(self.CHARSET_UPPER)
                elif c == 'd':
                    self._charsets.append(self.CHARSET_DIGIT)
                elif c == 's':
                    self._charsets.append(self.CHARSET_SPECIAL)
                elif c == 'a':
                    self._charsets.append(self.CHARSET_ALL)
                elif c == '?':
                    self._charsets.append('?')
                else:
                    self._charsets.append(c)
                i += 2
            else:
                self._charsets.append(self.mask[i])
                i += 1
        
        self._positions = [0] * len(self._charsets)
        
        self._total_count = 1
        for cs in self._charsets:
            self._total_count *= len(cs)
    
    def reset(self):
        self._positions = [0] * len(self._charsets)
        self._current = 0
        self._done = False
    
    def set_start(self, offset: int):
        """设置起始位置，用于分布式分片"""
        if offset >= self._total_count:
            self._done = True
            return
        
        remaining = offset
        for i in range(len(self._charsets) - 1, -1, -1):
            cs_len = len(self._charsets[i])
            self._positions[i] = remaining % cs_len
            remaining //= cs_len
        
        self._current = offset
        self._done = False
    
    def next(self) -> Optional[str]:
        """生成下一个密码，返回None表示结束"""
        if self._done:
            return None
        
        result = ''.join(
            self._charsets[i][self._positions[i]]
            for i in range(len(self._charsets))
        )
        
        # 进位
        carry = 1
        for i in range(len(self._charsets) - 1, -1, -1):
            if carry == 0:
                break
            
            self._positions[i] += carry
            cs_len = len(self._charsets[i])
            
            if self._positions[i] >= cs_len:
                self._positions[i] = 0
                carry = 1
            else:
                carry = 0
        
        self._current += 1
        if carry == 1:
            self._done = True
        
        return result
    
    def skip(self, count: int):
        """跳过N个，O(N)优化版"""
        for _ in range(count):
            if self._done:
                break
            self.next()
    
    @property
    def total_count(self) -> int:
        return self._total_count
    
    @property
    def current_index(self) -> int:
        return self._current
    
    @property
    def done(self) -> bool:
        return self._done
    
    @property
    def progress(self) -> float:
        if self._total_count == 0:
            return 100.0
        return (self._current / self._total_count) * 100.0
    
    @property
    def memory_usage(self) -> int:
        return len(self._charsets) * 64 + 64


# ==================== 字典攻击 ====================

class DictionaryAttack:
    """
    字典攻击器
    - 支持规则变形（大小写反转、首字母大写、后加数字等）
    - 流式读取，不全量载入内存
    - 内存占用：一行 + 规则状态 < 1KB
    """
    
    def __init__(self, dictionary_path: str, rules: Optional[List[str]] = None):
        self.dictionary_path = dictionary_path
        self.rules = rules or ['none']
        self._file = None
        self._current_line = 0
        self._rule_index = 0
        self._total_lines = 0
        self._done = False
    
    def _count_lines(self) -> int:
        count = 0
        try:
            with open(self.dictionary_path, 'r', encoding='utf-8', errors='ignore') as f:
                for _ in f:
                    count += 1
        except:
            pass
        return count
    
    def open(self):
        self._file = open(self.dictionary_path, 'r', encoding='utf-8', errors='ignore')
        self._total_lines = self._count_lines()
        self._current_line = 0
        self._rule_index = 0
        self._done = False
    
    def close(self):
        if self._file:
            self._file.close()
            self._file = None
    
    def _apply_rule(self, word: str, rule: str) -> str:
        if rule == 'none':
            return word
        elif rule == 'upper':
            return word.upper()
        elif rule == 'lower':
            return word.lower()
        elif rule == 'capitalize':
            return word.capitalize()
        elif rule == 'reverse':
            return word[::-1]
        elif rule == 'toggle_case':
            return word.swapcase()
        return word
    
    def next(self) -> Optional[str]:
        if self._done or self._file is None:
            return None
        
        line = self._file.readline()
        if not line:
            self._rule_index += 1
            if self._rule_index >= len(self.rules):
                self._done = True
                return None
            
            self._file.seek(0)
            self._current_line = 0
            line = self._file.readline()
            if not line:
                self._done = True
                return None
        
        self._current_line += 1
        word = line.strip()
        return self._apply_rule(word, self.rules[self._rule_index])
    
    @property
    def progress(self) -> float:
        if self._total_lines == 0:
            return 0.0
        total = self._total_lines * len(self.rules)
        current = self._current_line + self._rule_index * self._total_lines
        return (current / total) * 100.0
    
    @property
    def memory_usage(self) -> int:
        return 1024  # 约1KB


# ==================== 分布式任务分片调度 ====================

class TaskScheduler:
    """
    分布式任务调度器
    - 将破解任务分片分配给多个节点
    - 支持动态增减节点
    - 任务进度追踪与结果汇总
    - 内存：节点状态表 < 2KB
    """
    
    def __init__(self, total_nodes: int = 1):
        self.total_nodes = total_nodes
        self._tasks = {}  # node_id -> (start, end, status)
        self._results = {}
        self._found = False
        self._result = ''
        self._total_keys = 0
        self._completed_keys = 0
        self._start_time = 0
    
    def setup_bruteforce(self, total_keys: int):
        """设置暴力破解任务，自动分片"""
        self._total_keys = total_keys
        self._completed_keys = 0
        self._found = False
        self._result = ''
        self._start_time = time.time()
        
        per_node = total_keys // self.total_nodes
        remainder = total_keys % self.total_nodes
        
        self._tasks = {}
        start = 0
        
        for node_id in range(self.total_nodes):
            count = per_node + (1 if node_id < remainder else 0)
            end = start + count
            self._tasks[node_id] = {
                'start': start,
                'end': end,
                'status': 'pending',  # pending / running / done / error
                'progress': 0,
                'attempts': 0,
                'rate': 0,
            }
            start = end
    
    def get_task(self, node_id: int) -> Optional[Tuple[int, int]]:
        """获取节点的任务范围"""
        if node_id in self._tasks:
            task = self._tasks[node_id]
            task['status'] = 'running'
            return (task['start'], task['end'])
        return None
    
    def update_progress(self, node_id: int, progress: float, attempts: int, rate: float):
        """更新节点进度"""
        if node_id in self._tasks:
            task = self._tasks[node_id]
            task['progress'] = progress
            task['attempts'] = attempts
            task['rate'] = rate
            
            task_range = task['end'] - task['start']
            self._completed_keys = sum(
                (t['end'] - t['start']) * t['progress'] / 100.0
                for t in self._tasks.values()
            )
    
    def mark_done(self, node_id: int, found: bool = False, result: str = ''):
        """标记节点任务完成"""
        if node_id in self._tasks:
            self._tasks[node_id]['status'] = 'done'
            self._tasks[node_id]['progress'] = 100.0
            
            if found:
                self._found = True
                self._result = result
                for tid in self._tasks:
                    if self._tasks[tid]['status'] == 'running':
                        self._tasks[tid]['status'] = 'cancelled'
    
    def add_node(self, node_id: int) -> bool:
        """动态添加节点，从最忙的节点分走一部分任务"""
        if node_id in self._tasks:
            return False
        
        # 找任务最多的节点
        busiest = None
        max_remaining = 0
        
        for nid, task in self._tasks.items():
            if task['status'] in ('pending', 'running'):
                remaining = task['end'] - task['start'] - int(
                    (task['end'] - task['start']) * task['progress'] / 100.0
                )
                if remaining > max_remaining:
                    max_remaining = remaining
                    busiest = nid
        
        if busiest is None or max_remaining < 1000:
            return False
        
        # 从最忙节点分走一半剩余任务
        task = self._tasks[busiest]
        current_pos = task['start'] + int(
            (task['end'] - task['start']) * task['progress'] / 100.0
        )
        remaining_start = current_pos
        split_pos = remaining_start + (task['end'] - remaining_start) // 2
        
        new_start = split_pos
        new_end = task['end']
        task['end'] = split_pos
        
        self._tasks[node_id] = {
            'start': new_start,
            'end': new_end,
            'status': 'pending',
            'progress': 0,
            'attempts': 0,
            'rate': 0,
        }
        
        self.total_nodes += 1
        return True
    
    def remove_node(self, node_id: int) -> bool:
        """移除节点，将任务转给其他节点"""
        if node_id not in self._tasks:
            return False
        
        task = self._tasks[node_id]
        if task['status'] == 'done':
            del self._tasks[node_id]
            return True
        
        # 找最闲的节点接手
        least_busy = None
        min_remaining = float('inf')
        
        for nid, t in self._tasks.items():
            if nid == node_id:
                continue
            if t['status'] in ('pending', 'running'):
                remaining = t['end'] - t['start'] - int(
                    (t['end'] - t['start']) * t['progress'] / 100.0
                )
                if remaining < min_remaining:
                    min_remaining = remaining
                    least_busy = nid
        
        if least_busy is None:
            return False
        
        # 合并任务
        current_pos = task['start'] + int(
            (task['end'] - task['start']) * task['progress'] / 100.0
        )
        self._tasks[least_busy]['end'] = task['end']
        
        del self._tasks[node_id]
        self.total_nodes -= 1
        return True
    
    def get_total_progress(self) -> float:
        """获取总进度"""
        if self._total_keys == 0:
            return 0.0
        return (self._completed_keys / self._total_keys) * 100.0
    
    def get_total_rate(self) -> float:
        """获取总算力 (keys/sec)"""
        return sum(t['rate'] for t in self._tasks.values())
    
    def get_eta(self) -> float:
        """估算剩余时间（秒）"""
        rate = self.get_total_rate()
        if rate <= 0:
            return 0
        remaining = self._total_keys - self._completed_keys
        return remaining / rate
    
    def get_node_status(self) -> Dict:
        """获取所有节点状态"""
        return {nid: dict(t) for nid, t in self._tasks.items()}
    
    @property
    def found(self) -> bool:
        return self._found
    
    @property
    def result(self) -> str:
        return self._result
    
    @property
    def memory_usage(self) -> int:
        return self.total_nodes * 64 + 128


# ==================== 破解引擎 ====================

class CrackEngine:
    """
    破解引擎
    - 支持MD5/SHA-256
    - 支持暴力破解/掩码攻击/字典攻击
    - 进度追踪 + ETA估算
    - 内存：< 4KB
    """
    
    ALGO_MD5 = 'md5'
    ALGO_SHA256 = 'sha256'
    
    MODE_BRUTEFORCE = 'bruteforce'
    MODE_MASK = 'mask'
    MODE_DICTIONARY = 'dictionary'
    
    def __init__(self, algorithm: str = 'md5'):
        self.algorithm = algorithm
        self.mode = self.MODE_BRUTEFORCE
        self._target_hash = ''
        self._running = False
        self._found = False
        self._result = ''
        self._attempts = 0
        self._start_time = 0
        self._mask_gen: Optional[MaskGenerator] = None
        self._dict_attack: Optional[DictionaryAttack] = None
    
    def start_bruteforce(self, target_hash: str, charset: str, min_len: int, max_len: int):
        """启动暴力破解"""
        self._target_hash = target_hash.lower()
        self.mode = self.MODE_BRUTEFORCE
        self._running = True
        self._found = False
        self._result = ''
        self._attempts = 0
        self._start_time = time.time()
        
        mask = '?a' * max_len
        self._mask_gen = MaskGenerator(mask, charset)
    
    def start_mask(self, target_hash: str, mask: str):
        """启动掩码攻击"""
        self._target_hash = target_hash.lower()
        self.mode = self.MODE_MASK
        self._running = True
        self._found = False
        self._result = ''
        self._attempts = 0
        self._start_time = time.time()
        
        self._mask_gen = MaskGenerator(mask)
    
    def start_dictionary(self, target_hash: str, dict_path: str, rules: List[str] = None):
        """启动字典攻击"""
        self._target_hash = target_hash.lower()
        self.mode = self.MODE_DICTIONARY
        self._running = True
        self._found = False
        self._result = ''
        self._attempts = 0
        self._start_time = time.time()
        
        self._dict_attack = DictionaryAttack(dict_path, rules)
        self._dict_attack.open()
    
    def _compute_hash(self, text: str) -> str:
        if self.algorithm == self.ALGO_MD5:
            return hashlib.md5(text.encode()).hexdigest()
        elif self.algorithm == self.ALGO_SHA256:
            return hashlib.sha256(text.encode()).hexdigest()
        return ''
    
    def step(self, count: int = 1000) -> bool:
        """
        执行count次尝试
        找到返回True，否则False
        """
        if not self._running or self._found:
            return self._found
        
        generator = self._mask_gen if self._mask_gen else None
        use_dict = (self.mode == self.MODE_DICTIONARY)
        
        for _ in range(count):
            if use_dict and self._dict_attack:
                candidate = self._dict_attack.next()
                if candidate is None:
                    self._running = False
                    break
            elif generator:
                candidate = generator.next()
                if candidate is None:
                    self._running = False
                    break
            else:
                break
            
            self._attempts += 1
            h = self._compute_hash(candidate)
            
            if h == self._target_hash:
                self._found = True
                self._result = candidate
                self._running = False
                return True
        
        return False
    
    def stop(self):
        self._running = False
        if self._dict_attack:
            self._dict_attack.close()
    
    @property
    def progress(self) -> float:
        if self._mask_gen:
            return self._mask_gen.progress
        if self._dict_attack:
            return self._dict_attack.progress
        return 0.0
    
    @property
    def rate(self) -> float:
        elapsed = time.time() - self._start_time
        if elapsed <= 0:
            return 0
        return self._attempts / elapsed
    
    @property
    def elapsed(self) -> float:
        return time.time() - self._start_time
    
    @property
    def eta(self) -> float:
        r = self.rate
        if r <= 0:
            return 0
        if self._mask_gen:
            remaining = self._mask_gen.total_count - self._mask_gen.current_index
            return remaining / r
        return 0
    
    @property
    def found(self) -> bool:
        return self._found
    
    @property
    def result(self) -> str:
        return self._result
    
    @property
    def attempts(self) -> int:
        return self._attempts
    
    @property
    def memory_usage(self) -> int:
        total = 256
        if self._mask_gen:
            total += self._mask_gen.memory_usage
        if self._dict_attack:
            total += self._dict_attack.memory_usage
        return total
