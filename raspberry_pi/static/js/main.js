/**
 * 超采集算系统 - Web前端JavaScript
 */

// WebSocket连接
const socket = io();

// 模拟波形图表
let analogChart = null;

// 初始化图表
function initCharts() {
    const analogCtx = document.getElementById('analogChart').getContext('2d');
    
    analogChart = new Chart(analogCtx, {
        type: 'line',
        data: {
            labels: Array.from({length: 64}, (_, i) => 'CH' + (i + 1)),
            datasets: [{
                label: '模拟采样值',
                data: Array(64).fill(0),
                borderColor: '#4ecca3',
                backgroundColor: 'rgba(78, 204, 163, 0.1)',
                fill: true,
                tension: 0.4,
                pointRadius: 2,
                pointHoverRadius: 5
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: {
                        color: '#a0a0a0'
                    }
                }
            },
            scales: {
                x: {
                    ticks: { color: '#a0a0a0' },
                    grid: { color: '#0f3460' }
                },
                y: {
                    ticks: { color: '#a0a0a0' },
                    grid: { color: '#0f3460' },
                    min: 0,
                    max: 3.3
                }
            }
        }
    });
}

// 初始化数字通道显示
function initDigitalChannels() {
    const container = document.getElementById('digital_channels');
    container.innerHTML = '';
    
    for (let i = 0; i < 128; i++) {
        const channel = document.createElement('div');
        channel.className = 'digital-channel';
        channel.id = `digital_${i}`;
        channel.title = `通道 ${i}`;
        container.appendChild(channel);
    }
}

// 更新状态显示
function updateStatus(data) {
    // 工作模式
    $('#work_mode').text(data.work_mode);
    
    // 运行状态
    const runStatus = $('#run_status');
    runStatus.text(data.run_status);
    if (data.run_status === 'Running') {
        runStatus.addClass('running');
    } else {
        runStatus.removeClass('running');
    }
    
    // 核心温度
    const coreTemp = $('#core_temp');
    coreTemp.text(data.core_temp + '℃');
    coreTemp.removeClass('normal warning danger');
    if (data.core_temp >= 70) {
        coreTemp.addClass('danger');
    } else if (data.core_temp >= 60) {
        coreTemp.addClass('warning');
    } else {
        coreTemp.addClass('normal');
    }
    
    // 电池电量
    const battery = $('#battery');
    battery.text(data.battery + '%');
    battery.removeClass('normal warning danger');
    if (data.battery <= 20) {
        battery.addClass('danger');
    } else if (data.battery <= 30) {
        battery.addClass('warning');
    } else {
        battery.addClass('normal');
    }
    
    // Pico频率
    $('#pico_freq').text(data.pico_freq + 'MHz');
    
    // Pico2频率
    $('#pico2_freq').text(data.pico2_freq + 'MHz');
    
    // 存储使用
    const storage = $('#storage');
    storage.text(data.storage + '%');
    storage.removeClass('normal warning danger');
    if (data.storage >= 90) {
        storage.addClass('danger');
    } else if (data.storage >= 80) {
        storage.addClass('warning');
    } else {
        storage.addClass('normal');
    }
    
    // 故障信息
    const faultInfo = $('#fault_info');
    faultInfo.text(data.fault_info);
    faultInfo.removeClass('normal danger');
    if (data.fault_info !== 'None') {
        faultInfo.addClass('danger');
    } else {
        faultInfo.addClass('normal');
    }
}

// 更新数据显示
function updateData(data) {
    // 更新模拟波形
    if (data.analog && data.analog.length > 0) {
        analogChart.data.datasets[0].data = data.analog;
        analogChart.update('none');
    }
    
    // 更新数字通道
    if (data.digital && data.digital.length > 0) {
        for (let i = 0; i < 128; i++) {
            const channel = document.getElementById(`digital_${i}`);
            if (channel) {
                if (data.digital[i] === 1) {
                    channel.classList.add('active');
                } else {
                    channel.classList.remove('active');
                }
            }
        }
    }
    
    // 更新破解进度
    if (data.crack_progress !== undefined) {
        const progress = data.crack_progress;
        $('#crack_progress').text(progress + '%');
        $('#crack_progress_bar').css('width', progress + '%');
    }
    
    // 更新破解结果
    if (data.crack_result) {
        $('#crack_result').text(data.crack_result);
    }
}

// WebSocket事件处理
socket.on('connect', () => {
    console.log('WebSocket连接成功');
    $('#connection_status').text('已连接').addClass('normal');
});

socket.on('disconnect', () => {
    console.log('WebSocket断开连接');
    $('#connection_status').text('断开连接').addClass('danger');
});

socket.on('status_update', (data) => {
    updateStatus(data);
});

socket.on('data_update', (data) => {
    updateData(data);
});

// 按钮事件处理
$('#btn_start_sample').click(() => {
    socket.emit('start_sample');
});

$('#btn_stop_sample').click(() => {
    socket.emit('stop_sample');
});

$('#btn_start_crack').click(() => {
    const targetHash = $('#target_hash').val();
    const keyLength = parseInt($('#key_length').val());
    
    if (!targetHash) {
        alert('请输入目标哈希值');
        return;
    }
    
    socket.emit('start_crack', {
        target_hash: targetHash,
        key_length: keyLength
    });
});

$('#btn_stop_crack').click(() => {
    socket.emit('stop_crack');
});

// 设置事件处理
$('#sample_rate').change(() => {
    const rate = parseInt($('#sample_rate').val());
    socket.emit('set_sample_rate', { rate: rate });
});

$('#overclock_mode').change(() => {
    const mode = parseInt($('#overclock_mode').val());
    socket.emit('set_overclock', { mode: mode });
});

// 页面加载完成后初始化
$(document).ready(() => {
    initCharts();
    initDigitalChannels();
    
    // 定期请求状态更新
    setInterval(() => {
        socket.emit('get_status');
    }, 1000);
});