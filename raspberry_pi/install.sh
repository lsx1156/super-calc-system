#!/bin/bash
# 超采集算系统 - 树莓派端安装脚本

echo "========================================="
echo "超采集算系统 V0.1 - 树莓派端安装"
echo "========================================="

# 检查是否为root用户
if [ "$EUID" -ne 0 ]; then
    echo "请使用root权限运行此脚本"
    exit 1
fi

# 设置工作目录
WORK_DIR="/home/pi/super_calc"
mkdir -p $WORK_DIR
mkdir -p $WORK_DIR/data
mkdir -p $WORK_DIR/logs
mkdir -p $WORK_DIR/config

# 安装系统依赖
echo "安装系统依赖..."
apt update
apt install -y python3-pip python3-dev i2c-tools

# 安装Python依赖
echo "安装Python依赖..."
pip3 install -r requirements.txt

# 复制服务文件
echo "安装系统服务..."
cp systemd/super_calc_web.service /etc/systemd/system/
cp systemd/super_calc_daemon.service /etc/systemd/system/

# 启用服务
systemctl daemon-reload
systemctl enable super_calc_web.service
systemctl enable super_calc_daemon.service

# 配置GPIO权限
echo "配置GPIO权限..."
usermod -a -G gpio pi
usermod -a -G i2c pi

# 配置串口权限
echo "配置串口权限..."
usermod -a -G dialout pi

# 设置开机启动
echo "配置开机启动..."
raspi-config nonint do_i2c 0  # 启用I2C
raspi-config nonint do_spi 0  # 启用SPI

echo "========================================="
echo "安装完成！"
echo "========================================="
echo ""
echo "启动服务:"
echo "  sudo systemctl start super_calc_web.service"
echo "  sudo systemctl start super_calc_daemon.service"
echo ""
echo "查看状态:"
echo "  sudo systemctl status super_calc_web.service"
echo ""
echo "Web界面:"
echo "  http://<树莓派IP>:5000"
echo ""
echo "========================================="