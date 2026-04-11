ARG TARGETPLATFORM=linux/amd64
FROM debian:bookworm-slim

# Cross-compilers for FR_Math size report.
# Bare toolchains only — no IDE, no runtime, no extras.
# Build with: docker build --platform linux/amd64 -t fr-math-sizes docker/

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc g++ make bash binutils \
    gcc-arm-none-eabi libnewlib-arm-none-eabi \
    gcc-riscv64-unknown-elf \
    gcc-12-m68k-linux-gnu \
    sdcc \
    wget ca-certificates xz-utils bzip2 \
    && rm -rf /var/lib/apt/lists/*

# MSP430: TI's prebuilt msp430-elf-gcc (not in Debian repos).
RUN wget -q https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-LlCjWuAbzH/9.3.1.2/msp430-gcc-9.3.1.11_linux64.tar.bz2 \
    -O /tmp/msp430-gcc.tar.bz2 \
    && mkdir -p /opt/msp430-gcc \
    && tar -xjf /tmp/msp430-gcc.tar.bz2 -C /opt/msp430-gcc --strip-components=1 \
    && rm /tmp/msp430-gcc.tar.bz2

ENV PATH="/opt/msp430-gcc/bin:${PATH}"

# ESP32 (Xtensa): Espressif's prebuilt toolchain.
RUN wget -q https://github.com/espressif/crosstool-NG/releases/download/esp-13.2.0_20240530/xtensa-esp-elf-13.2.0_20240530-x86_64-linux-gnu.tar.xz \
    -O /tmp/xtensa-gcc.tar.xz \
    && mkdir -p /opt/xtensa-gcc \
    && tar -xJf /tmp/xtensa-gcc.tar.xz -C /opt/xtensa-gcc --strip-components=1 \
    && rm /tmp/xtensa-gcc.tar.xz

ENV PATH="/opt/xtensa-gcc/bin:${PATH}"

WORKDIR /src
