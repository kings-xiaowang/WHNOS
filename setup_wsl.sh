#!/bin/bash
# setup_wsl.sh — WSL2 环境一键配置 (在 WSL Ubuntu 中运行)
# 用法: bash setup_wsl.sh

set -e

echo "===== WHNos — WSL2 环境配置 ====="
echo

# 检测是否在 WSL 中
if ! grep -qi microsoft /proc/version 2>/dev/null; then
    echo "[WARN] 未检测到 WSL 环境，脚本仍会继续..."
fi

echo "[1/3] 更新包列表..."
sudo apt-get update -qq

echo "[2/3] 安装依赖 (Clang + NASM + QEMU + GRUB)..."
sudo apt-get install -y --no-install-recommends \
    clang \
    lld \
    nasm \
    make \
    qemu-system-x86 \
    grub-pc-bin \
    grub-common \
    xorriso

echo "[3/3] 验证安装..."
echo -n "  clang   : "; clang   --version | head -1
echo -n "  nasm    : "; nasm    --version | head -1
echo -n "  qemu    : "; qemu-system-i386 --version | head -1
echo -n "  grub    : "; grub-mkrescue --version 2>&1 | head -1 || echo "OK"
echo -n "  ld.lld  : "; ld.lld --version 2>&1 | head -1 || echo "OK"

echo
echo "===== 配置完成! ====="
echo
echo "现在可以:"
echo "  make          # 编译 ISO"
echo "  make run      # 编译并用 QEMU 运行"
echo "  make debug    # 编译并启动调试 (GDB 端口 1234)"
echo "  make clean    # 清理"
