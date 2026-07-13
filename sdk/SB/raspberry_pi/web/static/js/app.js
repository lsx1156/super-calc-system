class SuperCalcApp {
    constructor() {
        this.socket = null;
        this.charts = {};
        this.statusData = null;
        this.waveformData = [];
        this.fftData = { freq: [], mag: [] };
        this.maxDataPoints = 100;
        this.init();
    }

    init() {
        this.initTabs();
        this.initCharts();
        this.initSocket();
        this.initControls();
        this.initDigitalChannels();
        this.initAnalogChannels();
        this.updateTime();
        setInterval(() => this.updateTime(), 1000);
    }

    initTabs() {
        const tabBtns = document.querySelectorAll('.tab-btn');
        tabBtns.forEach(btn => {
            btn.addEventListener('click', () => {
                const tab = btn.dataset.tab;
                this.switchTab(tab);
            });
        });
    }

    switchTab(tabName) {
        document.querySelectorAll('.tab-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `tab-${tabName}`);
        });
        
        setTimeout(() => {
            Object.values(this.charts).forEach(chart => {
                if (chart) chart.resize();
            });
        }, 100);
    }

    initCharts() {
        const chartDefaults = {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 300 },
            plugins: {
                legend: {
                    display: false,
                    labels: { color: '#94a3b8' }
                }
            },
            scales: {
                x: {
                    grid: { color: 'rgba(51, 65, 85, 0.5)' },
                    ticks: { color: '#94a3b8' }
                },
                y: {
                    grid: { color: 'rgba(51, 65, 85, 0.5)' },
                    ticks: { color: '#94a3b8' }
                }
            }
        };

        const waveformCtx = document.getElementById('waveform-chart');
        if (waveformCtx) {
            this.charts.waveform = new Chart(waveformCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: '模拟信号',
                        data: [],
                        borderColor: '#3b82f6',
                        backgroundColor: 'rgba(59, 130, 246, 0.1)',
                        fill: true,
                        tension: 0.4,
                        pointRadius: 0,
                        borderWidth: 2
                    }]
                },
                options: {
                    ...chartDefaults,
                    plugins: { legend: { display: false } },
                    scales: {
                        x: { ...chartDefaults.scales.x, display: false },
                        y: { ...chartDefaults.scales.y, min: 0, max: 3.3 }
                    }
                }
            });
        }

        const fftCtx = document.getElementById('fft-chart');
        if (fftCtx) {
            this.charts.fft = new Chart(fftCtx, {
                type: 'bar',
                data: {
                    labels: [],
                    datasets: [{
                        label: '幅值',
                        data: [],
                        backgroundColor: 'rgba(139, 92, 246, 0.7)',
                        borderColor: '#8b5cf6',
                        borderWidth: 1
                    }]
                },
                options: {
                    ...chartDefaults,
                    plugins: { legend: { display: false } }
                }
            });
        }

        const multiCtx = document.getElementById('multi-channel-chart');
        if (multiCtx) {
            const colors = ['#3b82f6', '#22c55e', '#f59e0b', '#ef4444', '#8b5cf6', '#06b6d4', '#ec4899', '#84cc16'];
            this.charts.multi = new Chart(multiCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: Array.from({ length: 8 }, (_, i) => ({
                        label: `CH${i + 1}`,
                        data: [],
                        borderColor: colors[i],
                        backgroundColor: 'transparent',
                        tension: 0.3,
                        pointRadius: 0,
                        borderWidth: 1.5
                    }))
                },
                options: {
                    ...chartDefaults,
                    plugins: {
                        legend: {
                            display: true,
                            labels: { color: '#94a3b8', boxWidth: 12, padding: 16 }
                        }
                    },
                    scales: {
                        x: { ...chartDefaults.scales.x, display: false },
                        y: { ...chartDefaults.scales.y, min: 0, max: 3.3 }
                    }
                }
            });
        }

        const logicCtx = document.getElementById('logic-chart');
        if (logicCtx) {
            this.charts.logic = new Chart(logicCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: Array.from({ length: 8 }, (_, i) => ({
                        label: `D${i}`,
                        data: [],
                        borderColor: '#22c55e',
                        backgroundColor: 'transparent',
                        tension: 0,
                        pointRadius: 0,
                        borderWidth: 2,
                        stepped: true
                    }))
                },
                options: {
                    ...chartDefaults,
                    plugins: {
                        legend: {
                            display: true,
                            labels: { color: '#94a3b8', boxWidth: 12, padding: 16 }
                        }
                    },
                    scales: {
                        x: { ...chartDefaults.scales.x, display: false },
                        y: {
                            ...chartDefaults.scales.y,
                            min: -0.5,
                            max: 8.5,
                            ticks: { stepSize: 1, callback: (v) => `D${v}` }
                        }
                    }
                }
            });
        }
    }

    initSocket() {
        this.socket = io({ transports: ['polling', 'websocket'] });

        this.socket.on('connect', () => {
            console.log('WebSocket connected');
            this.addLog('INFO', 'Web连接成功');
        });

        this.socket.on('disconnect', () => {
            console.log('WebSocket disconnected');
            this.addLog('WARNING', 'Web连接断开');
        });

        this.socket.on('status_update', (data) => {
            this.updateStatus(data);
        });

        this.socket.on('data_update', (data) => {
            this.updateData(data);
        });

        this.socket.on('mode_changed', (data) => {
            this.addLog('INFO', `模式切换: ${data.name}`);
        });
    }

    initControls() {
        document.querySelectorAll('.mode-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const mode = btn.dataset.mode;
                this.switchMode(mode);
            });
        });

        document.getElementById('btn-start-analog')?.addEventListener('click', () => {
            this.switchMode('sample');
        });

        document.getElementById('btn-stop-analog')?.addEventListener('click', () => {
            this.stopSystem();
        });

        document.getElementById('btn-start-digital')?.addEventListener('click', () => {
            this.switchMode('sample');
        });

        document.getElementById('btn-stop-digital')?.addEventListener('click', () => {
            this.stopSystem();
        });

        document.getElementById('btn-start-crack')?.addEventListener('click', () => {
            this.startCrack();
        });

        document.getElementById('btn-stop-crack')?.addEventListener('click', () => {
            this.stopCrack();
        });

        document.getElementById('btn-detect-nodes')?.addEventListener('click', () => {
            this.detectNodes();
        });

        document.getElementById('btn-auto-heal')?.addEventListener('click', () => {
            this.addLog('INFO', '执行故障自愈...');
        });

        document.getElementById('btn-sys-start')?.addEventListener('click', () => {
            this.switchMode('sample');
        });

        document.getElementById('btn-sys-stop')?.addEventListener('click', () => {
            this.stopSystem();
        });

        document.getElementById('btn-sys-restart')?.addEventListener('click', () => {
            this.addLog('WARNING', '重启系统...');
        });

        document.getElementById('btn-sys-shutdown')?.addEventListener('click', () => {
            if (confirm('确定要执行紧急停机吗？')) {
                this.addLog('ERROR', '执行紧急停机！');
            }
        });
    }

    initDigitalChannels() {
        const container = document.getElementById('digital-channels');
        if (!container) return;

        container.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const ch = document.createElement('div');
            ch.className = 'digital-channel low';
            ch.id = `digital-ch-${i}`;
            ch.innerHTML = `
                <div class="ch-name">D${String(i).padStart(2, '0')}</div>
                <div class="ch-state">LOW</div>
            `;
            container.appendChild(ch);
        }
    }

    initAnalogChannels() {
        const container = document.getElementById('analog-channels');
        if (!container) return;

        container.innerHTML = '';
        const colors = ['#3b82f6', '#22c55e', '#f59e0b', '#ef4444', '#8b5cf6', '#06b6d4', '#ec4899', '#84cc16'];
        for (let i = 0; i < 8; i++) {
            const ch = document.createElement('div');
            ch.className = 'channel-item';
            ch.id = `analog-ch-${i}`;
            ch.innerHTML = `
                <div class="channel-name">ADC CH${i + 1}</div>
                <div class="channel-value" style="color: ${colors[i]}">0.00V</div>
            `;
            container.appendChild(ch);
        }
    }

    updateStatus(data) {
        this.statusData = data;
        
        const system = data.system || {};
        const cluster = data.cluster || {};
        const sample = data.sample || {};
        const crack = data.crack || {};
        const hardware = data.hardware || {};
        const performance = data.performance || {};

        const modeNames = {
            'sample': '采样模式',
            'crack': '破译模式',
            'bruteforce': '暴力破解',
            'hw_test': '硬件测试',
            'standby': '待机'
        };

        const mode = system.work_mode || 'standby';
        const modeName = modeNames[mode] || mode;
        const statusText = system.run_status === 'Running' ? '运行中' : '待机中';

        document.getElementById('current-mode').textContent = modeName;
        document.getElementById('info-mode').textContent = modeName;
        document.getElementById('info-status').textContent = system.run_status || 'Stop';
        document.getElementById('info-version').textContent = system.version || 'V1.0';
        document.getElementById('info-fault').textContent = system.fault_code || 0;

        const statusDot = document.getElementById('status-dot');
        const statusTextEl = document.getElementById('status-text');
        statusTextEl.textContent = statusText;
        
        if (system.run_status === 'Running') {
            statusDot.className = 'status-dot';
        } else if (system.fault_code > 0) {
            statusDot.className = 'status-dot error';
        } else {
            statusDot.className = 'status-dot warning';
        }

        const uptime = system.uptime || 0;
        document.getElementById('stat-uptime').textContent = this.formatUptime(uptime);

        document.getElementById('stat-online-nodes').textContent = cluster.online_pico || 0;
        document.getElementById('stat-sample-rate').textContent = this.formatNumber(sample.sample_rate || 0);
        document.getElementById('stat-total-samples').textContent = this.formatNumber(sample.total_samples || 0);

        document.getElementById('info-pico2-total').textContent = cluster.pico2_count || 0;
        document.getElementById('info-pico2-online').textContent = cluster.pico2_online || 0;
        document.getElementById('info-pico-total').textContent = cluster.total_pico || 0;
        document.getElementById('info-pico-online').textContent = cluster.online_pico || 0;
        document.getElementById('info-pico-fault').textContent = cluster.fault_pico || 0;

        document.getElementById('info-cpu').textContent = `${performance.cpu_usage || 0}%`;
        document.getElementById('info-mem').textContent = `${performance.memory_usage || 0}%`;
        document.getElementById('info-disk').textContent = `${performance.disk_usage || 0}%`;
        document.getElementById('info-temp').textContent = `${hardware.core_temp?.toFixed(1) || 25}°C`;

        this.updateCrackStatus(crack);
        this.updateHardwareStatus(hardware);
        this.updateClusterNodes(cluster);
        this.updateModeButtons(mode);
    }

    updateData(data) {
        if (data.analog_values && data.analog_values.length > 0) {
            this.updateWaveform(data.analog_values);
            
            for (let i = 0; i < Math.min(8, data.analog_values.length); i++) {
                const el = document.getElementById(`analog-ch-${i}`);
                if (el) {
                    const valEl = el.querySelector('.channel-value');
                    if (valEl) {
                        valEl.textContent = `${data.analog_values[i].toFixed(3)}V`;
                    }
                }
            }

            if (this.charts.multi) {
                const now = new Date().toLocaleTimeString();
                this.charts.multi.data.labels.push(now);
                if (this.charts.multi.data.labels.length > this.maxDataPoints) {
                    this.charts.multi.data.labels.shift();
                }
                
                for (let i = 0; i < 8; i++) {
                    const val = data.analog_values[i] !== undefined ? data.analog_values[i] : null;
                    this.charts.multi.data.datasets[i].data.push(val);
                    if (this.charts.multi.data.datasets[i].data.length > this.maxDataPoints) {
                        this.charts.multi.data.datasets[i].data.shift();
                    }
                }
                this.charts.multi.update('none');
            }
        }

        if (data.fft_freq && data.fft_mag) {
            this.updateFFT(data.fft_freq, data.fft_mag);
        }
    }

    updateWaveform(values) {
        if (!this.charts.waveform) return;

        const now = new Date().toLocaleTimeString();
        this.waveformData.push(...values);
        if (this.waveformData.length > 200) {
            this.waveformData = this.waveformData.slice(-200);
        }

        const labels = this.waveformData.map((_, i) => i);
        this.charts.waveform.data.labels = labels;
        this.charts.waveform.data.datasets[0].data = this.waveformData;
        this.charts.waveform.update('none');
    }

    updateFFT(freq, mag) {
        if (!this.charts.fft) return;

        this.charts.fft.data.labels = freq.slice(0, 32);
        this.charts.fft.data.datasets[0].data = mag.slice(0, 32);
        this.charts.fft.update('none');
    }

    updateCrackStatus(crack) {
        const statusEl = document.getElementById('crack-status');
        if (statusEl) {
            statusEl.textContent = crack.running ? '运行中' : '未启动';
            statusEl.className = `status-pill ${crack.running ? 'running' : ''}`;
        }

        const progress = crack.progress || 0;
        const progressFill = document.getElementById('crack-progress-fill');
        const progressText = document.getElementById('crack-progress-text');
        if (progressFill && progressText) {
            progressFill.style.width = `${progress}%`;
            progressText.textContent = `${progress.toFixed(2)}%`;
        }

        const attemptsEl = document.getElementById('crack-attempts');
        if (attemptsEl) attemptsEl.textContent = this.formatNumber(crack.attempts || 0);

        const rateEl = document.getElementById('crack-rate');
        if (rateEl) rateEl.textContent = this.formatNumber(crack.rate || 0);

        const elapsedEl = document.getElementById('crack-elapsed');
        if (elapsedEl) elapsedEl.textContent = this.formatUptime(crack.elapsed || 0);

        const resultEl = document.getElementById('crack-result');
        if (resultEl) {
            if (crack.found && crack.result) {
                resultEl.textContent = `找到密钥: ${crack.result}`;
                resultEl.className = 'result-text found';
            } else if (crack.running) {
                resultEl.textContent = '破解进行中...';
                resultEl.className = 'result-text';
            } else {
                resultEl.textContent = '等待中...';
                resultEl.className = 'result-text';
            }
        }
    }

    updateHardwareStatus(hw) {
        const tempEl = document.getElementById('hw-temp');
        const tempFill = document.getElementById('temp-fill');
        if (tempEl && tempFill) {
            const temp = hw.core_temp || 25;
            tempEl.textContent = `${temp.toFixed(1)}°C`;
            const percent = Math.min(100, Math.max(0, temp));
            tempFill.style.width = `${percent}%`;
        }

        document.getElementById('hw-pico-freq').textContent = `${hw.pico_freq || 133} MHz`;
        document.getElementById('hw-pico2-freq').textContent = `${hw.pico2_freq || 150} MHz`;
        document.getElementById('hw-oc-status').textContent = hw.overclock_active ? '开启' : '关闭';
        document.getElementById('hw-battery-level').textContent = `${hw.battery_level || 100}%`;
        document.getElementById('hw-battery-volt').textContent = `${hw.battery_voltage || 7.4}V`;
        document.getElementById('hw-charging').textContent = hw.charging ? '充电中' : '未充电';
        document.getElementById('hw-vcore').textContent = `${hw.vcore || 1.1}V`;
    }

    updateClusterNodes(cluster) {
        document.getElementById('cluster-total').textContent = cluster.pico2_count || 0;
        document.getElementById('cluster-online').textContent = cluster.pico2_online || 0;
        document.getElementById('cluster-pico-total').textContent = cluster.total_pico || 0;
        document.getElementById('cluster-pico-online').textContent = cluster.online_pico || 0;

        const container = document.getElementById('cluster-nodes');
        if (!container) return;

        const devices = cluster.pico2_devices || [];
        if (devices.length === 0) {
            container.innerHTML = '<div style="text-align: center; padding: 40px; color: #64748b;">暂无Pico2设备</div>';
            return;
        }

        container.innerHTML = devices.map(pico2 => `
            <div class="pico2-node">
                <div class="pico2-header">
                    <div class="pico2-info">
                        <div class="pico2-status ${pico2.online ? 'online' : 'offline'}"></div>
                        <div class="pico2-id">Pico2 #${pico2.id}</div>
                    </div>
                    <div class="pico2-stats">
                        <span>Pico: <strong>${pico2.online_count || 0}/${pico2.pico_count || 0}</strong></span>
                        <span>温度: <strong>${(pico2.temperature || 25).toFixed(1)}°C</strong></span>
                        <span>故障: <strong>${pico2.fault_count || 0}</strong></span>
                    </div>
                </div>
                <div class="pico-nodes">
                    ${(pico2.nodes && Object.values(pico2.nodes)) ? Object.values(pico2.nodes).map(node => `
                        <div class="pico-node ${node.fault ? 'fault' : (node.online ? 'online' : 'offline')}">
                            <div class="node-id">#${node.node_id}</div>
                            <div class="node-status">${node.fault ? '故障' : (node.online ? '在线' : '离线')}</div>
                        </div>
                    `).join('') : ''}
                </div>
            </div>
        `).join('');
    }

    updateModeButtons(currentMode) {
        document.querySelectorAll('.mode-btn').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.mode === currentMode);
        });
    }

    switchMode(mode) {
        if (this.socket) {
            this.socket.emit('switch_mode', { mode: mode });
        }
        
        fetch('/api/mode/switch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mode })
        })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                this.addLog('INFO', `切换模式: ${data.name}`);
            }
        })
        .catch(err => {
            this.addLog('ERROR', `模式切换失败: ${err}`);
        });
    }

    stopSystem() {
        fetch('/api/control/stop', { method: 'POST' })
            .then(() => this.addLog('INFO', '系统已停止'))
            .catch(err => this.addLog('ERROR', `停止失败: ${err}`));
    }

    startCrack() {
        const targetHash = document.getElementById('crack-target').value;
        const keyLength = parseInt(document.getElementById('crack-keylen').value);
        const charset = document.getElementById('crack-charset').value;

        if (!targetHash) {
            alert('请输入目标哈希值');
            return;
        }

        fetch('/api/crack/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ target_hash: targetHash, key_length: keyLength, charset })
        })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                this.addLog('INFO', '暴力破解已启动');
            }
        })
        .catch(err => {
            this.addLog('ERROR', `启动破解失败: ${err}`);
        });
    }

    stopCrack() {
        fetch('/api/crack/stop', { method: 'POST' })
            .then(() => this.addLog('INFO', '破解已停止'))
            .catch(err => this.addLog('ERROR', `停止失败: ${err}`));
    }

    detectNodes() {
        fetch('/api/cluster/detect', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                this.addLog('INFO', `节点检测完成，在线 ${data.online_count} 个节点`);
            })
            .catch(err => {
                this.addLog('ERROR', `节点检测失败: ${err}`);
            });
    }

    addLog(level, message) {
        const viewer = document.getElementById('log-viewer');
        if (!viewer) return;

        const entry = document.createElement('div');
        entry.className = `log-entry ${level.toLowerCase()}`;
        const time = new Date().toLocaleTimeString();
        entry.textContent = `[${time}] [${level}] ${message}`;
        viewer.appendChild(entry);
        viewer.scrollTop = viewer.scrollHeight;
    }

    formatNumber(num) {
        if (num >= 1000000) return (num / 1000000).toFixed(1) + 'M';
        if (num >= 1000) return (num / 1000).toFixed(1) + 'K';
        return num.toString();
    }

    formatUptime(seconds) {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = Math.floor(seconds % 60);
        
        if (h > 0) return `${h}h ${m}m`;
        if (m > 0) return `${m}m ${s}s`;
        return `${s}s`;
    }

    updateTime() {
        const el = document.getElementById('footer-time');
        if (el) {
            el.textContent = new Date().toLocaleString('zh-CN');
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.app = new SuperCalcApp();
});
