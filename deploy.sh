#!/bin/bash
# 超采集算系统 - 完整部署脚本

echo "========================================="
echo "超采集算系统 V0.1 - 完整部署"
echo "========================================="

# 项目根目录
PROJECT_ROOT=$(dirname $(readlink -f $0))

# 步骤1: 构建固件
echo ""
echo "步骤1: 构建Pico/Pico2固件..."
echo ""
if [ -n "$PICO_SDK_PATH" ]; then
    cd $PROJECT_ROOT
    bash build_firmware.sh
else
    echo "警告: PICO_SDK_PATH 未设置，跳过固件构建"
    echo "请手动设置环境变量并构建固件"
fi

# 步骤2: 部署树莓派代码
echo ""
echo "步骤2: 部署树莓派端代码..."
echo ""
WORK_DIR="/home/pi/super_calc"

# 创建目录结构
mkdir -p $WORK_DIR
mkdir -p $WORK_DIR/data
mkdir -p $WORK_DIR/logs
mkdir -p $WORK_DIR/config

# 复制文件
cp -r $PROJECT_ROOT/raspberry_pi/* $WORK_DIR/

# 步骤3: 安装依赖
echo ""
echo "步骤3: 安装依赖..."
echo ""
cd $WORK_DIR
pip3 install -r requirements.txt --break-system-packages

# 步骤4: 配置系统服务
echo ""
echo "步骤4: 配置系统服务..."
echo ""
cp $WORK_DIR/systemd/*.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable super_calc_web.service
systemctl enable super_calc_daemon.service

# 步骤5: 配置权限
echo ""
echo "步骤5: 配置权限..."
echo ""
usermod -a -G gpio pi
usermod -a -G i2c pi
usermod -a -G dialout pi

# 步骤6: 配置树莓派
echo ""
echo "步骤6: 配置树莓派..."
echo ""
# 启用I2C
if command -v raspi-config &> /dev/null; then
    raspi-config nonint do_i2c 0
    raspi-config nonint do_spi 0
fi

echo ""
echo "========================================="
echo "部署完成！"
echo "========================================="
echo ""
echo "固件烧录:"
echo "  Pico固件: pico/firmware/build/super_calc_pico.uf2"
echo "  Pico2固件: pico2/firmware/build/super_calc_pico2.uf2"
echo ""
echo "启动服务:"
echo "  sudo systemctl start super_calc_web.service"
echo "  sudo systemctl start super_calc_daemon.service"
echo ""
echo "Web界面:"
echo "  http://<树莓派IP>:5000"
echo ""
echo "========================================="