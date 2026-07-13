#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
系统部署工具 (deploy.py)
- 环境检查
- 依赖安装
- 固件编译与烧录
- 系统配置
- 服务注册
"""

import os
import sys
import subprocess
import platform
import shutil
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
RPI_DIR = os.path.join(PROJECT_ROOT, "raspberry_pi")
PICO_DIR = os.path.join(PROJECT_ROOT, "pico")
PICO2_DIR = os.path.join(PROJECT_ROOT, "pico2")


class DeployTool:
    """部署工具"""
    
    def __init__(self):
        self.steps = []
        self.errors = []
    
    def print_header(self, title):
        """打印标题"""
        print(f"\n{'='*60}")
        print(f"  {title}")
        print(f"{'='*60}")
    
    def print_step(self, step):
        """打印步骤"""
        print(f"\n[步骤] {step}")
    
    def print_success(self, msg):
        """打印成功"""
        print(f"  ✓ {msg}")
    
    def print_warning(self, msg):
        """打印警告"""
        print(f"  ⚠ {msg}")
    
    def print_error(self, msg):
        """打印错误"""
        print(f"  ✗ {msg}")
        self.errors.append(msg)
    
    def run_command(self, cmd, cwd=None, check=False):
        """运行命令"""
        try:
            result = subprocess.run(
                cmd,
                shell=True,
                cwd=cwd,
                capture_output=True,
                text=True
            )
            return result.returncode == 0, result.stdout, result.stderr
        except Exception as e:
            return False, "", str(e)
    
    def check_python(self):
        """检查Python环境"""
        self.print_step("检查Python环境")
        
        version = sys.version_info
        if version.major >= 3 and version.minor >= 8:
            self.print_success(f"Python {version.major}.{version.minor}.{version.micro}")
            return True
        else:
            self.print_error(f"Python版本过低，需要3.8+，当前{version.major}.{version.minor}")
            return False
    
    def check_pip(self):
        """检查pip"""
        self.print_step("检查pip")
        
        ok, out, err = self.run_command(f"{sys.executable} -m pip --version")
        if ok:
            self.print_success(out.strip())
            return True
        else:
            self.print_error("pip不可用")
            return False
    
    def install_dependencies(self):
        """安装Python依赖"""
        self.print_step("安装Python依赖")
        
        req_file = os.path.join(RPI_DIR, "requirements.txt")
        if not os.path.exists(req_file):
            self.print_warning("requirements.txt不存在，跳过")
            return True
        
        ok, out, err = self.run_command(
            f"{sys.executable} -m pip install -r {req_file}"
        )
        
        if ok:
            self.print_success("依赖安装完成")
            return True
        else:
            self.print_warning(f"依赖安装可能不完整: {err[:200]}")
            return False
    
    def check_cmake(self):
        """检查CMake"""
        self.print_step("检查CMake")
        
        ok, out, err = self.run_command("cmake --version")
        if ok:
            self.print_success(out.strip().split('\n')[0])
            return True
        else:
            self.print_warning("CMake未安装，无法编译Pico固件")
            return False
    
    def check_pico_sdk(self):
        """检查Pico SDK"""
        self.print_step("检查Pico SDK")
        
        sdk_path = os.environ.get("PICO_SDK_PATH", "")
        if sdk_path and os.path.exists(sdk_path):
            self.print_success(f"PICO_SDK_PATH: {sdk_path}")
            return True
        else:
            self.print_warning("PICO_SDK_PATH未设置，无法编译Pico固件")
            return False
    
    def build_pico_firmware(self):
        """编译Pico固件"""
        self.print_step("编译Pico终端固件")
        
        build_dir = os.path.join(PICO_DIR, "firmware", "build")
        
        if not os.path.exists(os.path.join(PICO_DIR, "firmware", "CMakeLists.txt")):
            self.print_warning("Pico固件工程不存在")
            return False
        
        os.makedirs(build_dir, exist_ok=True)
        
        ok, out, err = self.run_command(
            "cmake ..",
            cwd=build_dir
        )
        
        if not ok:
            self.print_error(f"CMake配置失败: {err[:200]}")
            return False
        
        ok, out, err = self.run_command(
            "make -j$(nproc 2>/dev/null || echo 4)",
            cwd=build_dir
        )
        
        if ok:
            self.print_success("Pico固件编译完成")
            return True
        else:
            self.print_error(f"编译失败: {err[:200]}")
            return False
    
    def build_pico2_firmware(self):
        """编译Pico2固件"""
        self.print_step("编译Pico2协处理器固件")
        
        build_dir = os.path.join(PICO2_DIR, "firmware", "build")
        
        if not os.path.exists(os.path.join(PICO2_DIR, "firmware", "CMakeLists.txt")):
            self.print_warning("Pico2固件工程不存在")
            return False
        
        os.makedirs(build_dir, exist_ok=True)
        
        ok, out, err = self.run_command(
            "cmake ..",
            cwd=build_dir
        )
        
        if not ok:
            self.print_error(f"CMake配置失败: {err[:200]}")
            return False
        
        ok, out, err = self.run_command(
            "make -j$(nproc 2>/dev/null || echo 4)",
            cwd=build_dir
        )
        
        if ok:
            self.print_success("Pico2固件编译完成")
            return True
        else:
            self.print_error(f"编译失败: {err[:200]}")
            return False
    
    def create_directories(self):
        """创建必要目录"""
        self.print_step("创建数据目录")
        
        dirs = [
            os.path.join(RPI_DIR, "data"),
            os.path.join(RPI_DIR, "logs"),
            os.path.join(RPI_DIR, "config"),
            os.path.join(RPI_DIR, "tmp"),
        ]
        
        for d in dirs:
            os.makedirs(d, exist_ok=True)
            self.print_success(f"创建目录: {os.path.basename(d)}/")
        
        return True
    
    def setup_systemd(self):
        """配置systemd服务"""
        self.print_step("配置系统服务")
        
        if platform.system() != "Linux":
            self.print_warning("非Linux系统，跳过systemd配置")
            return False
        
        service_content = f"""[Unit]
Description=超采集算系统 - 完整版
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory={RPI_DIR}
ExecStart={sys.executable} {os.path.join(RPI_DIR, "main.py")} --mode watchdog
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
"""
        
        service_file = "/etc/systemd/system/super-calc.service"
        
        try:
            if os.path.exists("/etc/systemd/system"):
                with open("/tmp/super-calc.service", "w") as f:
                    f.write(service_content)
                self.print_success(f"服务文件已生成: /tmp/super-calc.service")
                self.print_warning("请手动执行: sudo cp /tmp/super-calc.service /etc/systemd/system/ && sudo systemctl enable super-calc")
            else:
                self.print_warning("systemd目录不存在，跳过服务配置")
        except Exception as e:
            self.print_warning(f"服务配置失败: {e}")
        
        return True
    
    def full_deploy(self):
        """完整部署"""
        self.print_header("超采集算系统 - 完整部署")
        
        results = []
        
        results.append(("Python环境", self.check_python()))
        results.append(("pip", self.check_pip()))
        results.append(("Python依赖", self.install_dependencies()))
        results.append(("数据目录", self.create_directories()))
        
        has_sdk = self.check_pico_sdk()
        has_cmake = self.check_cmake()
        
        if has_sdk and has_cmake:
            results.append(("Pico固件", self.build_pico_firmware()))
            results.append(("Pico2固件", self.build_pico2_firmware()))
        else:
            self.print_warning("跳过固件编译（缺少SDK或CMake）")
        
        results.append(("系统服务", self.setup_systemd()))
        
        self.print_header("部署总结")
        
        passed = sum(1 for _, ok in results if ok)
        total = len(results)
        
        for name, ok in results:
            status = "✓ 通过" if ok else "✗ 失败/跳过"
            print(f"  {name:20s} {status}")
        
        print(f"\n总计: {passed}/{total} 通过")
        
        if self.errors:
            print(f"\n错误列表:")
            for err in self.errors:
                print(f"  - {err}")
        
        return len(self.errors) == 0
    
    def quick_check(self):
        """快速环境检查"""
        self.print_header("环境快速检查")
        
        self.check_python()
        self.check_pip()
        self.check_cmake()
        self.check_pico_sdk()
        
        print(f"\n系统: {platform.system()} {platform.release()}")
        print(f"架构: {platform.machine()}")
        print(f"Python: {sys.executable}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="超采集算系统部署工具")
    parser.add_argument(
        "command",
        nargs="?",
        default="check",
        choices=["check", "deploy", "build", "service"],
        help="操作命令"
    )
    
    args = parser.parse_args()
    
    tool = DeployTool()
    
    if args.command == "check":
        tool.quick_check()
    elif args.command == "deploy":
        tool.full_deploy()
    elif args.command == "build":
        tool.print_header("固件编译")
        if tool.check_cmake() and tool.check_pico_sdk():
            tool.build_pico_firmware()
            tool.build_pico2_firmware()
    elif args.command == "service":
        tool.setup_systemd()


if __name__ == "__main__":
    main()
