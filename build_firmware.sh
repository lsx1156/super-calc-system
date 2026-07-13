#!/bin/bash
# 超采集算系统 - Pico/Pico2固件构建脚本

echo "========================================="
echo "超采集算系统 - 固件构建"
echo "========================================="

# 检查Pico SDK
if [ ! -d "$PICO_SDK_PATH" ]; then
    echo "错误: PICO_SDK_PATH 未设置"
    echo "请设置环境变量:"
    echo "  export PICO_SDK_PATH=/path/to/pico-sdk"
    exit 1
fi

# 构建Pico固件
echo "构建Pico终端芯片固件..."
cd pico/firmware
mkdir -p build
cd build
cmake ..
make -j4
echo "Pico固件构建完成: build/super_calc_pico.uf2"

# 构建Pico2固件
echo "构建Pico2协处理器固件..."
cd ../../pico2/firmware
mkdir -p build
cd build
cmake ..
make -j4
echo "Pico2固件构建完成: build/super_calc_pico2.uf2"

echo "========================================="
echo "固件构建完成！"
echo "========================================="
echo ""
echo "固件文件:"
echo "  pico/firmware/build/super_calc_pico.uf2"
echo "  pico2/firmware/build/super_calc_pico2.uf2"
echo ""
echo "烧录方法:"
echo "  1. 按住BOOTSEL按钮，连接USB"
echo "  2. 复制.uf2文件到RPI-RP2驱动器"
echo "  3. 设备将自动重启并运行固件"
echo ""
echo "========================================="