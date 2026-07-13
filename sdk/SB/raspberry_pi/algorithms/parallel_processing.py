#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
并行处理框架 (parallel_processing.py)
- 多进程并行计算，充分利用多核CPU
- 线程池处理I/O密集型任务
- 任务队列 + 结果汇总
- 内存可控，避免内存溢出
"""

import os
import sys
import time
import multiprocessing as mp
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
from typing import List, Callable, Any, Optional, Tuple
from dataclasses import dataclass
from queue import Queue
import threading

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from core.logger import get_logger

logger = get_logger("Parallel")


@dataclass
class TaskResult:
    """任务结果"""
    task_id: int
    success: bool
    data: Any = None
    error: str = ""
    duration: float = 0.0


class ParallelProcessor:
    """
    并行处理器
    - CPU密集型任务用多进程
    - I/O密集型任务用多线程
    - 内存可控的批量处理
    """
    
    def __init__(self, max_workers: Optional[int] = None, mode: str = 'process'):
        """
        初始化并行处理器
        mode: 'process' 多进程 (CPU密集)
              'thread' 多线程 (I/O密集)
        """
        self.mode = mode
        self.max_workers = max_workers or (mp.cpu_count() if mode == 'process' else 32)
        self._executor = None
        self._running = False
        
        logger.info(f"并行处理器初始化: mode={mode}, workers={self.max_workers}")
    
    def start(self):
        """启动执行器"""
        if self._executor is not None:
            return
        
        if self.mode == 'process':
            self._executor = ProcessPoolExecutor(max_workers=self.max_workers)
        else:
            self._executor = ThreadPoolExecutor(max_workers=self.max_workers)
        
        self._running = True
        logger.info(f"并行处理器启动: {self.max_workers} 个{'进程' if self.mode == 'process' else '线程'}")
    
    def stop(self):
        """停止执行器"""
        self._running = False
        if self._executor:
            self._executor.shutdown(wait=False)
            self._executor = None
        logger.info("并行处理器已停止")
    
    def map(self, func: Callable, items: List[Any], 
            batch_size: Optional[int] = None) -> List[TaskResult]:
        """
        并行执行函数，处理一批数据
        """
        if not self._running:
            self.start()
        
        results = []
        total = len(items)
        
        if batch_size is None:
            batch_size = max(1, total // (self.max_workers * 4))
        
        batches = [items[i:i + batch_size] for i in range(0, total, batch_size)]
        
        futures = []
        for batch_idx, batch in enumerate(batches):
            future = self._executor.submit(self._process_batch, func, batch, batch_idx)
            futures.append(future)
        
        for future in as_completed(futures):
            try:
                batch_results = future.result()
                results.extend(batch_results)
            except Exception as e:
                logger.error(f"任务执行异常: {e}")
        
        results.sort(key=lambda r: r.task_id)
        
        return results
    
    @staticmethod
    def _process_batch(func: Callable, batch: List[Any], batch_idx: int) -> List[TaskResult]:
        """处理一批任务（在子进程/子线程中执行）"""
        results = []
        base_id = batch_idx * len(batch)
        
        for i, item in enumerate(batch):
            t0 = time.time()
            try:
                data = func(item)
                results.append(TaskResult(
                    task_id=base_id + i,
                    success=True,
                    data=data,
                    duration=time.time() - t0
                ))
            except Exception as e:
                results.append(TaskResult(
                    task_id=base_id + i,
                    success=False,
                    error=str(e),
                    duration=time.time() - t0
                ))
        
        return results
    
    def submit(self, func: Callable, *args, **kwargs):
        """提交单个任务"""
        if not self._running:
            self.start()
        return self._executor.submit(func, *args, **kwargs)
    
    def __enter__(self):
        self.start()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()


class MultiChannelProcessor:
    """
    多通道并行处理器
    专门用于处理多通道信号数据
    """
    
    def __init__(self, num_channels: int, process_func: Callable,
                 max_workers: Optional[int] = None):
        self.num_channels = num_channels
        self.process_func = process_func
        self.max_workers = max_workers or min(num_channels, mp.cpu_count())
        self._processor = ParallelProcessor(max_workers=self.max_workers, mode='process')
    
    def process_channels(self, channel_data: List[Any]) -> List[Any]:
        """
        并行处理多个通道的数据
        channel_data: 每个通道的数据列表
        """
        if len(channel_data) == 0:
            return []
        
        if len(channel_data) == 1:
            return [self.process_func(channel_data[0])]
        
        results = self._processor.map(self.process_func, channel_data)
        
        return [r.data if r.success else None for r in results]
    
    def start(self):
        self._processor.start()
    
    def stop(self):
        self._processor.stop()


class BatchStreamProcessor:
    """
    流式批量处理器
    持续接收数据，积累到一定批量后并行处理
    适用于实时数据流处理
    """
    
    def __init__(self, process_func: Callable, batch_size: int = 100,
                 max_workers: int = 4, mode: str = 'thread',
                 result_callback: Optional[Callable] = None):
        self.process_func = process_func
        self.batch_size = batch_size
        self.result_callback = result_callback
        
        self._processor = ParallelProcessor(max_workers=max_workers, mode=mode)
        self._buffer = []
        self._buffer_lock = threading.Lock()
        self._running = False
        self._thread: Optional[threading.Thread] = None
    
    def start(self):
        """启动流处理器"""
        if self._running:
            return
        
        self._running = True
        self._processor.start()
        self._thread = threading.Thread(target=self._process_loop, daemon=True)
        self._thread.start()
        logger.info("流式批量处理器启动")
    
    def submit(self, data: Any):
        """提交数据"""
        with self._buffer_lock:
            self._buffer.append(data)
    
    def submit_batch(self, data_list: List[Any]):
        """批量提交数据"""
        with self._buffer_lock:
            self._buffer.extend(data_list)
    
    def _process_loop(self):
        """处理循环"""
        while self._running:
            batch = []
            
            with self._buffer_lock:
                if len(self._buffer) >= self.batch_size:
                    batch = self._buffer[:self.batch_size]
                    self._buffer = self._buffer[self.batch_size:]
            
            if batch:
                try:
                    results = self._processor.map(self.process_func, batch)
                    
                    if self.result_callback:
                        for r in results:
                            if r.success:
                                self.result_callback(r.data)
                except Exception as e:
                    logger.error(f"流处理异常: {e}")
            else:
                time.sleep(0.001)
    
    def flush(self):
        """清空缓冲区，处理剩余数据"""
        with self._buffer_lock:
            remaining = list(self._buffer)
            self._buffer.clear()
        
        if remaining:
            results = self._processor.map(self.process_func, remaining)
            if self.result_callback:
                for r in results:
                    if r.success:
                        self.result_callback(r.data)
    
    def stop(self):
        """停止流处理器"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        self.flush()
        self._processor.stop()
        logger.info("流式批量处理器已停止")


class CrackParallelEngine:
    """
    并行破解引擎
    - 多进程分布式破解
    - 任务自动分片
    - 进度实时追踪
    - 找到即停止
    """
    
    def __init__(self, num_workers: Optional[int] = None):
        self.num_workers = num_workers or mp.cpu_count()
        self._running = False
        self._found = False
        self._result = ""
        self._total = 0
        self._processed = 0
        self._lock = threading.Lock()
        self._callbacks = {
            'found': [],
            'progress': [],
            'finished': [],
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
    
    def crack_md5_mask(self, target_hash: str, mask: str,
                       charset: Optional[str] = None) -> Tuple[bool, str]:
        """
        并行MD5掩码破解
        """
        import hashlib
        from algorithms.crack_algorithms import MaskGenerator
        
        self._running = True
        self._found = False
        self._result = ""
        
        gen = MaskGenerator(mask, charset)
        total = gen.total_count
        self._total = total
        self._processed = 0
        
        chunk_size = max(1000, int(total / (self.num_workers * 10)))
        
        logger.info(f"开始并行破解: workers={self.num_workers}, total={total}")
        
        chunks = []
        offset = 0
        while offset < total:
            end = min(offset + chunk_size, total)
            chunks.append((offset, end, mask, charset, target_hash))
            offset = end
        
        found_result = None
        
        with ProcessPoolExecutor(max_workers=self.num_workers) as executor:
            futures = {executor.submit(self._crack_chunk, chunk): chunk for chunk in chunks}
            
            for future in as_completed(futures):
                if self._found:
                    future.cancel()
                    continue
                
                try:
                    result = future.result()
                    chunk = futures[future]
                    self._processed += (chunk[1] - chunk[0])
                    
                    progress = self._processed / total if total > 0 else 0
                    self._fire_event('progress', progress, self._processed, total)
                    
                    if result:
                        with self._lock:
                            if not self._found:
                                self._found = True
                                self._result = result
                                found_result = result
                        
                        self._fire_event('found', result)
                        
                        for f in futures:
                            if not f.done():
                                f.cancel()
                        break
                        
                except Exception as e:
                    logger.error(f"破解任务异常: {e}")
        
        self._running = False
        self._fire_event('finished', self._found, self._result)
        
        logger.info(f"破解完成: found={self._found}, result={self._result}")
        
        return self._found, self._result
    
    @staticmethod
    def _crack_chunk(args):
        """破解单个分片（在子进程中执行）"""
        import hashlib
        from algorithms.crack_algorithms import MaskGenerator
        
        start, end, mask, charset, target_hash = args
        target = target_hash.lower()
        
        gen = MaskGenerator(mask, charset)
        gen.set_start(start)
        
        count = end - start
        for i in range(count):
            pwd = gen.next()
            if pwd is None:
                break
            
            h = hashlib.md5(pwd.encode()).hexdigest()
            if h == target:
                return pwd
        
        return None
    
    def get_progress(self) -> dict:
        """获取进度"""
        return {
            'running': self._running,
            'found': self._found,
            'result': self._result,
            'total': self._total,
            'processed': self._processed,
            'progress': self._processed / self._total if self._total > 0 else 0,
        }
    
    def stop(self):
        """停止破解"""
        self._running = False
