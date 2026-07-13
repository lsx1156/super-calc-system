#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
超采集算系统 Demo - 部署与验证工具
用于一键部署和验证Demo环境
"""

import os
import sys
import time
import json
import platform
import subprocess
from pathlib import Path

# 颜色输出
class Colors:
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'

def cprint(msg, color=''):
    if platform.system() == 'Windows':
        print(msg)
    else:
        print(f"{color}{msg}{Colors.END}")

# 路径配置
BASE_DIR = Path(__file__).parent.parent
DEMO_DIR = BASE_DIR / 'demo'
PICO_FW = DEMO_DIR / 'pico' / 'firmware'
PICO2_FW = DEMO_DIR / 'pico2' / 'firmware'
RPI_APP = DEMO_DIR / 'raspberry_pi'

# ==================== 检查项 ====================

class CheckItem:
    def __init__(self, name, func, critical=False):
        self.name = name
        self.func = func
        self.critical = critical
        self.result = None
        self.message = ''
    
    def run(self):
        try:
            self.result, self.message = self.func()
        except Exception as e:
            self.result = False
            self.message = f"异常: {e}"
        return self.result

def check_pico_sdk():
    """检查Pico SDK"""
    sdk_path = os.environ.get('PICO_SDK_PATH', '')
    if sdk_path and os.path.exists(sdk_path):
        return True, f"PICO_SDK_PATH: {sdk_path}"
    return False, "未设置PICO_SDK_PATH环境变量"

def check_cmake():
    """检查CMake"""
    try:
        result = subprocess.run(['cmake', '--version'], capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            version = result.stdout.split('\n')[0]
            return True, version
        return False, "CMake不可用"
    except:
        return False, "CMake未安装"

def check_arm_gcc():
    """检查ARM GCC"""
    try:
        result = subprocess.run(['arm-none-eabi-gcc', '--version'], 
                              capture_output=True, text=True, timeout=5)
        if result.returncode == 0:
            version = result.stdout.split('\n')[0]
            return True, version
        return False, "ARM GCC不可用"
    except:
        return False, "ARM GCC未安装"

def check_python():
    """检查Python"""
    version = sys.version.split()[0]
    return True, f"Python {version}"

def check_pip_packages():
    """检查Python依赖"""
    packages = ['pyserial', 'flask', 'flask_socketio', 'numpy']
    missing = []
    for pkg in packages:
        try:
            __import__(pkg.replace('-', '_').replace('flask_socketio', 'flask_socketio'))
        except:
            try:
                __import__(pkg.replace('flask-socketio', 'flask_socketio'))
            except:
                missing.append(pkg)
    
    if missing:
        return False, f"缺少: {', '.join(missing)}"
    return True, f"全部已安装: {', '.join(packages)}"

def check_pico_firmware():
    """检查Pico固件源码"""
    main_c = PICO_FW / 'main.c'
    cmake = PICO_FW / 'CMakeLists.txt'
    if main_c.exists() and cmake.exists():
        return True, "Pico固件源码已就绪"
    return False, "Pico固件源码不完整"

def check_pico2_firmware():
    """检查Pico2固件源码"""
    main_c = PICO2_FW / 'main.c'
    cmake = PICO2_FW / 'CMakeLists.txt'
    if main_c.exists() and cmake.exists():
        return True, "Pico2固件源码已就绪"
    return False, "Pico2固件源码不完整"

def check_rpi_app():
    """检查树莓派应用"""
    app = RPI_APP / 'app_demo.py'
    template = RPI_APP / 'templates' / 'index.html'
    css = RPI_APP / 'static' / 'css' / 'style_demo.css'
    js = RPI_APP / 'static' / 'js' / 'demo.js'
    
    missing = []
    for f in [app, template, css, js]:
        if not f.exists():
            missing.append(f.name)
    
    if missing:
        return False, f"缺少: {', '.join(missing)}"
    return True, "树莓派应用已就绪"

# ==================== 构建函数 ====================

def build_pico_firmware():
    """构建Pico固件"""
    cprint("\n[1/2] 构建 Pico (RP2040) 固件...", Colors.CYAN)
    
    build_dir = PICO_FW / 'build'
    build_dir.mkdir(exist_ok=True)
    
    try:
        # cmake configure
        result = subprocess.run(
            ['cmake', '..', '-DPICO_BOARD=pico'],
            cwd=str(build_dir),
            capture_output=True, text=True, timeout=120
        )
        if result.returncode != 0:
            cprint(f"CMake配置失败: {result.stderr}", Colors.RED)
            return False
        
        # make
        result = subprocess.run(
            ['make', '-j4'],
            cwd=str(build_dir),
            capture_output=True, text=True, timeout=300
        )
        if result.returncode != 0:
            cprint(f"编译失败: {result.stderr}", Colors.RED)
            return False
        
        uf2_file = build_dir / 'super_calc_demo_pico.uf2'
        if uf2_file.exists():
            cprint(f"✓ Pico固件构建成功: {uf2_file}", Colors.GREEN)
            return True
        else:
            cprint("构建完成但未找到UF2文件", Colors.YELLOW)
            return False
            
    except Exception as e:
        cprint(f"构建异常: {e}", Colors.RED)
        return False

def build_pico2_firmware():
    """构建Pico2固件"""
    cprint("\n[2/2] 构建 Pico2 (RP2350) 固件...", Colors.CYAN)
    
    build_dir = PICO2_FW / 'build'
    build_dir.mkdir(exist_ok=True)
    
    try:
        result = subprocess.run(
            ['cmake', '..', '-DPICO_BOARD=pico2'],
            cwd=str(build_dir),
            capture_output=True, text=True, timeout=120
        )
        if result.returncode != 0:
            cprint(f"CMake配置失败: {result.stderr}", Colors.RED)
            return False
        
        result = subprocess.run(
            ['make', '-j4'],
            cwd=str(build_dir),
            capture_output=True, text=True, timeout=300
        )
        if result.returncode != 0:
            cprint(f"编译失败: {result.stderr}", Colors.RED)
            return False
        
        uf2_file = build_dir / 'super_calc_demo_pico2.uf2'
        if uf2_file.exists():
            cprint(f"✓ Pico2固件构建成功: {uf2_file}", Colors.GREEN)
            return True
        else:
            cprint("构建完成但未找到UF2文件", Colors.YELLOW)
            return False
            
    except Exception as e:
        cprint(f"构建异常: {e}", Colors.RED)
        return False

# ==================== 验证函数 ====================

def run_sanity_checks():
    """运行完整性检查"""
    cprint("\n" + "="*60, Colors.BOLD)
    cprint("  超采集算系统 Demo - 环境完整性检查", Colors.BOLD + Colors.CYAN)
    cprint("="*60 + "\n", Colors.BOLD)
    
    checks = [
        CheckItem("Python环境", check_python, critical=True),
        CheckItem("Python依赖包", check_pip_packages, critical=True),
        CheckItem("Pico SDK", check_pico_sdk, critical=False),
        CheckItem("CMake", check_cmake, critical=False),
        CheckItem("ARM GCC", check_arm_gcc, critical=False),
        CheckItem("Pico固件源码", check_pico_firmware, critical=True),
        CheckItem("Pico2固件源码", check_pico2_firmware, critical=True),
        CheckItem("树莓派应用", check_rpi_app, critical=True),
    ]
    
    passed = 0
    failed = 0
    critical_fail = False
    
    for check in checks:
        check.run()
        status = "✓" if check.result else "✗"
        color = Colors.GREEN if check.result else (Colors.RED if check.critical else Colors.YELLOW)
        
        cprint(f"  {status} {check.name}: {check.message}", color)
        
        if check.result:
            passed += 1
        else:
            failed += 1
            if check.critical:
                critical_fail = True
    
    cprint(f"\n  结果: {passed} 通过, {failed} 失败", Colors.BOLD)
    
    if critical_fail:
        cprint("  ⚠ 存在关键项失败，部分功能不可用", Colors.RED)
    else:
        cprint("  ✓ 所有关键检查通过", Colors.GREEN)
    
    return not critical_fail

# ==================== 主菜单 ====================

def print_menu():
    cprint("\n" + "="*60, Colors.BOLD)
    cprint("  超采集算系统 Demo 工具", Colors.BOLD + Colors.CYAN)
    cprint("="*60 + "\n", Colors.BOLD)
    
    cprint("  1. 环境完整性检查")
    cprint("  2. 构建Pico固件 (RP2040)")
    cprint("  3. 构建Pico2固件 (RP2350)")
    cprint("  4. 构建全部固件")
    cprint("  5. 安装Python依赖")
    cprint("  6. 启动Web Demo (模拟模式)")
    cprint("  0. 退出")
    print()

def install_dependencies():
    """安装Python依赖"""
    cprint("\n安装Python依赖包...", Colors.CYAN)
    packages = ['pyserial', 'flask', 'flask-socketio', 'numpy']
    
    for pkg in packages:
        cprint(f"  安装 {pkg}...", Colors.YELLOW)
        result = subprocess.run(
            [sys.executable, '-m', 'pip', 'install', pkg],
            capture_output=False, text=True
        )
        if result.returncode != 0:
            cprint(f"  ✗ {pkg} 安装失败", Colors.RED)
            return False
    
    cprint("✓ 全部依赖安装完成", Colors.GREEN)
    return True

def start_simulated_demo():
    """启动模拟Demo（不需要硬件）"""
    cprint("\n启动Demo Web服务 (模拟模式)...", Colors.CYAN)
    cprint("访问 http://localhost:5000 查看界面", Colors.GREEN)
    cprint("按 Ctrl+C 停止\n", Colors.YELLOW)
    
    # 创建一个模拟启动脚本
    sim_script = RPI_APP / 'app_simulated.py'
    
    import shutil
    shutil.copy(RPI_APP / 'app_demo.py', sim_script)
    
    # 用模拟启动
    try:
        result = subprocess.run(
            [sys.executable, str(sim_script)],
            cwd=str(RPI_APP),
            capture_output=False
        )
    except KeyboardInterrupt:
        cprint("\nDemo已停止", Colors.YELLOW)

def main():
    while True:
        print_menu()
        choice = input("请选择操作 [0-6]: ").strip()
        
        if choice == '1':
            run_sanity_checks()
            
        elif choice == '2':
            if run_sanity_checks():
                build_pico_firmware()
                
        elif choice == '3':
            if run_sanity_checks():
                build_pico2_firmware()
                
        elif choice == '4':
            if run_sanity_checks():
                build_pico_firmware()
                build_pico2_firmware()
                
        elif choice == '5':
            install_dependencies()
            
        elif choice == '6':
            start_simulated_demo()
            
        elif choice == '0':
            cprint("再见！", Colors.GREEN)
            break
            
        else:
            cprint("无效选项", Colors.RED)
        
        input("\n按回车继续...")

if __name__ == '__main__':
    main()