# ⚡ Hardware Accelerated Ray Tracer (HWRT)
> A high performance, physically based, progressive **Path Tracer** built from scratch using **C++20** and **Vulkan**.

![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-yellow)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.4-red.svg)](https://www.vulkan.org/)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)

## 🖼️ Showcase
<p align="center">
  <img src="screenshots/bistro.png" alt="Amazon Lumberyard Bistro (exterior)">
  <em>Amazon Lumberyard Bistro (exterior)</em>
</p>
<p align="center">
  <img src="screenshots/bistro_int.png" alt="Amazon Lumberyard Bistro (interior)">
  <em>Amazon Lumberyard Bistro (interior)</em>
</p>
<p align="center">
  <img src="screenshots/sponza.png" alt="Crytek Sponza">
  <em>Crytek Sponza</em>
</p>
<p align="center">
  <img src="screenshots/cornell_box.png" alt="Cornell Box">
  <em>Cornell Box</em>
</p>

## ✨ Features
* Vulkan Ray Tracing Pipeline
* Unidirectional Path Tracing
* Physically Based Materials
* Next Event Estimation
* Multiple Importance Sampling
* glTF 2.0 Scene Loading
* Khronos PBR Neutral Tone mapping
* Anti-Aliasing
* Procedural Atmosphere

## 🖥️ Requirements
* OS: Windows/Linux (x64)
* GPU: NVIDIA RTX or AMD RX 6000+
* Vulkan SDK: 1.4
* CMake: 4.0

## ⚙️ Build
```bash
git clone --recursive https://github.com/gavrix32/hwrt.git
cd hwrt
mkdir build && cd build
cmake ..
cmake --build . --config Release
./hwrt -m ../assets/models/cornell_box.glb
```