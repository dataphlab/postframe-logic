<p align="center">
  <img src="assets/program_base/logo-mini.png" width="250" alt="PostFrame-Logic Logo">
</p>

## A tool that allows you to do motion tracking in real time using a modified realsense D4xx/D5xx.

**There is almost nothing in the project, soon (maybe) there will be something useful.**

# Minimum specifications
- Processor with AVX2 support
- 8GB of RAM
- Optional: CUDA-enabled graphics card with at least Turing architecture
- RealSense D4xx/D5xx
- Raspberry Pi Pico
- Any I2C MPU-6050 module

# Quickstart
Download or clone the repository
```bash
# Clone the repository
git clone --recursive https://github.com/dataphlab/postframe-logic.git
cd postframe-logic

# Build
cmake -B build
cmake --build build

# Run!
./build/postframe-logic
```
