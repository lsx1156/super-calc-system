#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Web远程监控服务 (web_service.py)
- Flask + SocketIO 实时Web界面
- 模拟波形显示、数字通道状态
- 破解进度实时展示
- 集群状态监控
- 远程配置与控制
"""

import os
import sys
import time
import json
import threading
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from flask import Flask, render_template, request, jsonify, send_from_directory
from flask_socketio import SocketIO, emit

from core.config import (
    WEB_HOST, WEB_PORT, WEB_SECRET_KEY, WEB_DEBUG,
    SOCKETIO_ASYNC_MODE, SOCKETIO_CORS_ALLOWED,
    WorkMode, MODE_NAMES,
)
from core.logger import get_logger
from core.status_manager import status_mgr
from cluster.adaptive_cluster import cluster_mgr
from modes.mode_scheduler import mode_scheduler

logger = get_logger("WebService")


class WebService:
    """Web服务管理器"""
    
    def __init__(self):
        self.app = Flask(__name__, 
                        template_folder=os.path.join(os.path.dirname(__file__), '..', 'web', 'templates'),
                        static_folder=os.path.join(os.path.dirname(__file__), '..', 'web', 'static'))
        self.app.config['SECRET_KEY'] = WEB_SECRET_KEY
        
        self.socketio = SocketIO(
            self.app,
            async_mode=SOCKETIO_ASYNC_MODE,
            cors_allowed_origins=SOCKETIO_CORS_ALLOWED,
        )
        
        self._running = False
        self._data_queue = None
        self._thread = None
        
        self._setup_routes()
        self._setup_socketio()
    
    def set_data_queue(self, queue):
        self._data_queue = queue
    
    def _setup_routes(self):
        """设置HTTP路由"""
        
        @self.app.route('/')
        def index():
            return render_template('index.html')
        
        @self.app.route('/api/status')
        def api_status():
            return jsonify(status_mgr.get_all())
        
        @self.app.route('/api/system')
        def api_system():
            return jsonify(status_mgr.get_system())
        
        @self.app.route('/api/cluster')
        def api_cluster():
            return jsonify(status_mgr.get_cluster())
        
        @self.app.route('/api/sample')
        def api_sample():
            return jsonify(status_mgr.get_sample())
        
        @self.app.route('/api/crack')
        def api_crack():
            return jsonify(status_mgr.get_crack())
        
        @self.app.route('/api/hardware')
        def api_hardware():
            return jsonify(status_mgr.get_hardware())
        
        @self.app.route('/api/performance')
        def api_performance():
            return jsonify(status_mgr.get_performance())
        
        @self.app.route('/api/mode/list')
        def api_mode_list():
            modes = []
            for key, name in MODE_NAMES.items():
                modes.append({"code": key, "name": name})
            return jsonify({"modes": modes})
        
        @self.app.route('/api/mode/current')
        def api_mode_current():
            return jsonify({
                "mode": mode_scheduler.get_current_mode(),
                "state": mode_scheduler.get_state().value,
                "name": MODE_NAMES.get(mode_scheduler.get_current_mode(), "未知"),
            })
        
        @self.app.route('/api/mode/switch', methods=['POST'])
        def api_mode_switch():
            data = request.get_json()
            target_mode = data.get('mode', '')
            
            if target_mode not in MODE_NAMES:
                return jsonify({"success": False, "error": "无效的模式"}), 400
            
            success = mode_scheduler.switch_mode(target_mode)
            return jsonify({
                "success": success,
                "mode": target_mode,
                "name": MODE_NAMES.get(target_mode, ""),
            })
        
        @self.app.route('/api/cluster/nodes')
        def api_cluster_nodes():
            nodes = []
            for pico2 in cluster_mgr.pico2_nodes.values():
                pico2_data = pico2.to_dict()
                pico2_data["nodes"] = [n.to_dict() for n in pico2.nodes.values()]
                nodes.append(pico2_data)
            return jsonify({"pico2_devices": nodes})
        
        @self.app.route('/api/cluster/set_node_count', methods=['POST'])
        def api_set_node_count():
            data = request.get_json()
            pico2_id = data.get('pico2_id', 0)
            count = data.get('count', 8)
            
            success = cluster_mgr.set_node_count(pico2_id, count)
            return jsonify({"success": success})
        
        @self.app.route('/api/cluster/detect', methods=['POST'])
        def api_cluster_detect():
            count = cluster_mgr.detect_all()
            return jsonify({"success": True, "online_count": count})
        
        @self.app.route('/api/control/start', methods=['POST'])
        def api_control_start():
            current = mode_scheduler.get_current_mode()
            if current == WorkMode.STANDBY:
                success = mode_scheduler.switch_mode(WorkMode.SAMPLE)
            else:
                success = True
            return jsonify({"success": success})
        
        @self.app.route('/api/control/stop', methods=['POST'])
        def api_control_stop():
            mode_scheduler.stop_current()
            return jsonify({"success": True})
        
        @self.app.route('/api/crack/start', methods=['POST'])
        def api_crack_start():
            data = request.get_json()
            target_hash = data.get('target_hash', '')
            key_length = data.get('key_length', 16)
            charset = data.get('charset', '0123456789abcdef')
            
            if not target_hash:
                return jsonify({"success": False, "error": "请输入目标哈希"}), 400
            
            status_mgr.update_crack(
                running=True,
                target_hash=target_hash,
                key_length=key_length,
                charset=charset,
                attempts=0,
                progress=0,
                result="",
                found=False,
                elapsed=0,
            )
            
            success = mode_scheduler.switch_mode(WorkMode.BRUTEFORCE)
            return jsonify({"success": success})
        
        @self.app.route('/api/crack/stop', methods=['POST'])
        def api_crack_stop():
            status_mgr.update_crack(running=False)
            mode_scheduler.stop_current()
            return jsonify({"success": True})
    
    def _setup_socketio(self):
        """设置SocketIO事件"""
        
        @self.socketio.on('connect')
        def handle_connect():
            logger.info(f"Web客户端连接: {request.sid}")
            emit('status_update', status_mgr.get_all())
        
        @self.socketio.on('disconnect')
        def handle_disconnect():
            logger.info(f"Web客户端断开: {request.sid}")
        
        @self.socketio.on('get_status')
        def handle_get_status():
            emit('status_update', status_mgr.get_all())
        
        @self.socketio.on('switch_mode')
        def handle_switch_mode(data):
            target_mode = data.get('mode', '')
            if target_mode in MODE_NAMES:
                mode_scheduler.switch_mode(target_mode)
                emit('mode_changed', {"mode": target_mode, "name": MODE_NAMES[target_mode]})
        
        @self.socketio.on('start_sample')
        def handle_start_sample():
            mode_scheduler.switch_mode(WorkMode.SAMPLE)
        
        @self.socketio.on('stop_sample')
        def handle_stop_sample():
            mode_scheduler.stop_current()
    
    def _broadcast_loop(self):
        """状态广播循环"""
        while self._running:
            try:
                self.socketio.emit('status_update', status_mgr.get_all())
                
                if self._data_queue and not self._data_queue.empty():
                    try:
                        packet = self._data_queue.get_nowait()
                        self.socketio.emit('data_update', self._format_packet(packet))
                    except:
                        pass
                
                time.sleep(0.5)
            except Exception as e:
                logger.error(f"广播异常: {e}")
                time.sleep(1)
    
    def _format_packet(self, packet):
        """格式化数据包用于Web传输"""
        result = {
            "pico2_id": packet.get("pico2_id", 0),
            "data_type": packet.get("data_type", 0),
            "timestamp": packet.get("timestamp", time.time()),
        }
        
        if "analog_values" in packet:
            result["analog_values"] = packet["analog_values"]
        if "digital_values" in packet:
            result["digital_values"] = packet["digital_values"]
        if "fft_freq" in packet:
            result["fft_freq"] = packet["fft_freq"]
            result["fft_mag"] = packet["fft_mag"]
        
        return result
    
    def start(self):
        """启动Web服务"""
        logger.info(f"Web服务启动: http://{WEB_HOST}:{WEB_PORT}")
        self._running = True
        
        self._thread = threading.Thread(target=self._broadcast_loop, daemon=True)
        self._thread.start()
        
        try:
            self.socketio.run(
                self.app,
                host=WEB_HOST,
                port=WEB_PORT,
                debug=WEB_DEBUG,
                use_reloader=False,
            )
        except Exception as e:
            logger.error(f"Web服务启动失败: {e}")
    
    def stop(self):
        """停止Web服务"""
        self._running = False
        logger.info("Web服务已停止")


def main():
    web = WebService()
    web.start()


if __name__ == '__main__':
    main()
