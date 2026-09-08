@echo off
REM build.bat — Windows 一键构建 WHNos (需先安装 Docker Desktop)
REM 用法: 双击运行，或用命令行: build.bat [run]

echo ============================================
echo   WHNos — Windows Docker Build
echo ============================================
echo.

REM 检查 Docker
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] 未检测到 Docker，请先安装 Docker Desktop
    echo         下载: https://www.docker.com/products/docker-desktop
    pause
    exit /b 1
)

REM 构建镜像 (首次需下载 ~5分钟)
echo [1/3] 构建 Docker 镜像 (包含 Clang + QEMU + GRUB)...
docker build -t simpleos-builder . 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Docker 镜像构建失败
    pause
    exit /b 1
)

REM 编译内核
echo [2/3] 编译内核...
docker run --rm -v "%cd%":/workspace simpleos-builder make
if %errorlevel% neq 0 (
    echo [ERROR] 编译失败
    pause
    exit /b 1
)

echo [3/3] 完成! ISO 在 build\simpleos.iso

REM 是否运行
if "%1"=="run" (
    echo.
    echo 启动 QEMU (需要 Docker Desktop + WSL2)...
    docker run --rm -it -v "%cd%":/workspace -e DISPLAY=%DISPLAY% simpleos-builder make run
) else (
    echo.
    echo 提示: 用 build.bat run 可以直接启动 QEMU
    echo       也可以把 build\simpleos.iso 挂到 VirtualBox 启动
)

pause
