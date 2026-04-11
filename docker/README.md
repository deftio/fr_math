# FR_Math Cross-Compile Size Report

Docker container with cross-compilers for generating the multi-architecture
size table in the project README.

## Targets

| Target | Compiler | Word size |
|--------|----------|-----------|
| Cortex-M0 (Thumb-1) | arm-none-eabi-gcc | 32-bit |
| Cortex-M4 (Thumb-2) | arm-none-eabi-gcc | 32-bit |
| MSP430 | msp430-elf-gcc (TI) | 16-bit |
| RISC-V 32 (rv32im) | riscv64-unknown-elf-gcc | 32-bit |
| ESP32 (Xtensa) | xtensa-esp-elf-gcc | 32-bit |
| 68k | m68k-linux-gnu-gcc-12 | 32-bit |
| x86-32 | gcc -m32 | 32-bit |
| x86-64 | gcc | 64-bit |
| 8051 | sdcc | 8-bit |

## Usage

Build the image (one-time, ~5 min):

```bash
docker build -t fr-math-sizes docker/
```

Run the size report:

```bash
docker run --rm -v $(pwd):/src fr-math-sizes bash /src/docker/build_sizes.sh
```

Output goes to stdout and `build/size_table.md`.

## Alternative: pocketdock

The container can also be managed via
[pocketdock](https://github.com/deftio/pocketdock), a lightweight
container runner for dev workflows:

```python
from pocketdock import Container
c = Container(dockerfile="docker/Dockerfile", tag="fr-math-sizes")
c.run("bash /src/docker/build_sizes.sh", mount={"src": "."})
```
