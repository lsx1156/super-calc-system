// 超采集算系统 Demo 前端脚本

$(function() {
    // ==================== 初始化 ====================
    
    let socket = io();
    let deviceStatus = {};
    let sampleData = {};
    let crackData = {};
    let hwTestData = {};

    // 图表
    let analogChart, fftChart, tempChart;

    initDigitalGrid();
    initNodeGrid();
    initCharts();
    startClock();

    // ==================== 选项卡切换 ====================
    
    $('.tab').click(function() {
        let tab = $(this).data('tab');
        $('.tab').removeClass('active');
        $(this).addClass('active');
        $('.tab-content').removeClass('active');
        $('#tab-' + tab).addClass('active');
    });

    // ==================== Socket.IO 事件 ====================
    
    socket.on('connect', function() {
        $('#conn_status .dot').addClass('online');
        $('#conn_status span:last').text('已连接');
    });

    socket.on('disconnect', function() {
        $('#conn_status .dot').removeClass('online');
        $('#conn_status span:last').text('断开连接');
    });

    socket.on('status_update', function(data) {
        deviceStatus = data;
        updateStatusUI(data);
    });

    socket.on('data_update', function(data) {
        sampleData = data;
        updateDataUI(data);
    });

    socket.on('crack_update', function(data) {
        crackData = data;
        updateCrackUI(data);
    });

    socket.on('hw_test_update', function(data) {
        hwTestData = data;
        updateHwTestUI(data);
    });

    // ==================== 采样控制 ====================
    
    $('#btn_start_sample').click(function() {
        socket.emit('start_sample');
    });

    $('#btn_stop_sample').click(function() {
        socket.emit('stop_sample');
    });

    $('#sample_rate').change(function() {
        let rate = parseInt($(this).val());
        socket.emit('set_sample_rate', { rate: rate });
    });

    $('#oc_mode_sample').change(function() {
        let mode = parseInt($(this).val());
        socket.emit('set_overclock', { mode: mode });
    });

    // ==================== 破解控制 ====================
    
    $('#btn_start_crack').click(function() {
        let target = $('#target_hash').val().trim();
        let length = parseInt($('#key_length').val());
        if (!target) { alert('请输入目标哈希'); return; }
        socket.emit('start_crack', {
            target_hash: target,
            key_length: length
        });
    });

    $('#btn_stop_crack').click(function() {
        socket.emit('stop_crack');
    });

    // ==================== 硬件测试 ====================
    
    $('.btn-run-test').click(function() {
        let test = $(this).data('test');
        socket.emit('start_hw_test', { test_type: test });
    });

    // ==================== UI 更新函数 ====================
    
    function updateStatusUI(data) {
        $('#big_mode').text(data.work_mode);
        $('#big_run').text(data.run_status);
        $('#big_temp').text(data.core_temp.toFixed(1) + '°C');
        $('#big_pico2').text(data.pico2_freq + 'MHz');
        $('#sample_status').text(data.run_status);
        $('#sample_count').text(data.sample_count.toLocaleString());
        
        let online = data.pico_online ? data.pico_online.filter(x => x).length : 0;
        $('#pico_online').text(online + '/8');
        
        $('#freq_pico').text(data.pico_freq + ' MHz');
        $('#freq_pico2').text(data.pico2_freq + ' MHz');
        $('#vcore').text(data.vcore.toFixed(2) + ' V');
        $('#hw_temp').text(data.core_temp.toFixed(1) + '°C');
        
        // 更新节点状态
        if (data.pico_online) {
            for (let i = 0; i < 8; i++) {
                let $card = $('#node-' + i);
                if (data.pico_online[i]) {
                    $card.addClass('online');
                    $card.find('.node-status').text('在线');
                } else {
                    $card.removeClass('online');
                    $card.find('.node-status').text('离线');
                }
            }
        }
    }

    function updateDataUI(data) {
        // 更新模拟波形
        if (analogChart && data.waveform) {
            let labels = data.waveform.map((_, i) => i);
            analogChart.data.labels = labels;
            analogChart.data.datasets[0].data = data.waveform;
            analogChart.update('none');
        }
        
        // FFT
        if (fftChart && data.fft_freq && data.fft_mag) {
            fftChart.data.labels = data.fft_freq.slice(0, 32).map(f => f.toFixed(0));
            fftChart.data.datasets[0].data = data.fft_mag.slice(0, 32);
            fftChart.update('none');
        }
        
        $('#peak_freq').text(data.peak_freq + ' Hz');
        $('#snr').text(data.snr + ' dB');
        
        // 数字通道（模拟一些随机变化）
        updateDigitalBits();
    }

    function updateCrackUI(data) {
        $('#crack_progress_text').text(data.progress + '%');
        $('#crack_fill').css('width', data.progress + '%');
        $('#crack_attempts').text(data.attempts.toLocaleString());
        $('#crack_rate').text(data.rate.toLocaleString());
        $('#crack_elapsed').text(formatTime(data.elapsed));
        
        if (data.running) {
            $('#crack_status_tag').text('运行中').css('color', '#ff9800');
        } else if (data.result && data.result.indexOf('成功') >= 0) {
            $('#crack_status_tag').text('成功').css('color', '#4caf50');
        } else if (data.result) {
            $('#crack_status_tag').text('完成').css('color', '#8899bb');
        } else {
            $('#crack_status_tag').text('待机').css('color', '#8899bb');
        }
        
        if (data.result) {
            $('#crack_result').text(data.result);
        }
    }

    function updateHwTestUI(data) {
        $('#current_test').text(data.current_test || '--');
        $('#glitch_count').text(data.glitch_count);
        
        if (data.running) {
            $('#hw_test_status').text('运行中').css('color', '#ff9800');
        } else {
            $('#hw_test_status').text('待机').css('color', '#8899bb');
        }
        
        if (data.test_results && Object.keys(data.test_results).length > 0) {
            let html = '';
            for (let key in data.test_results) {
                html += `<div style="margin-bottom:8px"><b>${key}:</b> ${data.test_results[key]}</div>`;
            }
            $('#hw_test_result').html(html);
        }
        
        // 温度图表
        if (tempChart && data.temp_history) {
            let temps = data.temp_history.slice(-30);
            tempChart.data.labels = temps.map((_, i) => i);
            tempChart.data.datasets[0].data = temps;
            tempChart.update('none');
        }
    }

    // ==================== 初始化 ====================
    
    function initDigitalGrid() {
        let $grid = $('#digital_grid');
        for (let i = 0; i < 64; i++) {
            $grid.append(`<div class="digital-bit" id="db-${i}">D${i}</div>`);
        }
    }

    function initNodeGrid() {
        let $grid = $('#node_grid');
        for (let i = 0; i < 8; i++) {
            $grid.append(`
                <div class="node-card" id="node-${i}">
                    <div class="node-id">ID: ${i}</div>
                    <div class="node-name">Pico #${i}</div>
                    <div class="node-status">检测中...</div>
                </div>
            `);
        }
    }

    function initCharts() {
        // 模拟波形图
        let aCtx = $('#analogChart')[0].getContext('2d');
        analogChart = new Chart(aCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'CH0',
                    data: [],
                    borderColor: '#4fc3f7',
                    backgroundColor: 'rgba(79,195,247,0.1)',
                    borderWidth: 1.5,
                    pointRadius: 0,
                    tension: 0.3,
                    fill: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: { display: false },
                    y: {
                        grid: { color: 'rgba(42,58,92,0.5)' },
                        ticks: { color: '#8899bb' }
                    }
                }
            }
        });

        // FFT图
        let fCtx = $('#fftChart')[0].getContext('2d');
        fftChart = new Chart(fCtx, {
            type: 'bar',
            data: {
                labels: [],
                datasets: [{
                    data: [],
                    backgroundColor: 'rgba(129,199,132,0.7)',
                    borderColor: '#81c784',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: { ticks: { color: '#8899bb', font: { size: 9 } } },
                    y: {
                        grid: { color: 'rgba(42,58,92,0.5)' },
                        ticks: { color: '#8899bb' }
                    }
                }
            }
        });

        // 温度图
        let tCtx = $('#tempChart')[0].getContext('2d');
        tempChart = new Chart(tCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: '温度',
                    data: [],
                    borderColor: '#ff7043',
                    backgroundColor: 'rgba(255,112,67,0.1)',
                    borderWidth: 2,
                    pointRadius: 0,
                    fill: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: { legend: { display: false } },
                scales: {
                    x: { display: false },
                    y: {
                        grid: { color: 'rgba(42,58,92,0.5)' },
                        ticks: { color: '#8899bb' }
                    }
                }
            }
        });
    }

    // 模拟数字位变化
    let digitalBits = new Array(64).fill(0);
    function updateDigitalBits() {
        for (let i = 0; i < 64; i++) {
            if (Math.random() < 0.1) {
                digitalBits[i] = 1 - digitalBits[i];
            }
            let $bit = $('#db-' + i);
            if (digitalBits[i]) $bit.addClass('high');
            else $bit.removeClass('high');
        }
    }

    function startClock() {
        setInterval(function() {
            let now = new Date();
            let t = now.getHours().toString().padStart(2, '0') + ':' +
                    now.getMinutes().toString().padStart(2, '0') + ':' +
                    now.getSeconds().toString().padStart(2, '0');
            $('#current_time').text(t);
        }, 1000);
    }

    function formatTime(seconds) {
        if (seconds < 60) return Math.round(seconds) + 's';
        let m = Math.floor(seconds / 60);
        let s = Math.round(seconds % 60);
        return m + 'm' + s + 's';
    }
});