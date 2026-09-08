# Dockerfile — 在 Windows/macOS/Linux 上一键构建 WHNos
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    lld \
    nasm \
    make \
    qemu-system-x86 \
    grub-pc-bin \
    grub-common \
    xorriso \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# 默认：构建 ISO
CMD ["make"]
