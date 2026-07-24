FROM ubuntu:24.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential \
      gcc-multilib \
      grub-common \
      grub-pc-bin \
      make \
      python3 \
      qemu-system-x86 \
      xorriso && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
