# RoboMaster 视觉识别与低延迟传输系统

本项目是一个基于 C++、OpenCV 和 Zenoh 的装甲板视觉识别系统，支持基于共享内存（SHM）的零拷贝视频流传输。

## 核心特性 (Features)

- 基于传统视觉的装甲板识别（二值化、轮廓提取、灯条配对）。
- 基于 Zenoh 的共享内存池，实现极低延迟的跨进程图像传输。
- 在Mat构建与图像处理过程中零拷贝。

## 环境依赖 (Prerequisites)

在编译之前，请确保你的系统（推荐 Ubuntu 20.04/22.04）安装了以下依赖：

- **C++ 编译器**：支持 C++17 (GCC 9+)
- **CMake**：3.10 及以上
- **OpenCV**：4.5 及以上
- **Zenoh-c & zenoh-cpp**：1.10.1 版本

## 编译与运行 (Build & Run)

### 1. 编译项目
```bash
mkdir build
cmake -S . -B build
cmake --build build
```

### 2. 运行发布端 (发送视频)
```bash
./video_pub <视频路径参数>
# 例如：./video_pub raw.mp4
```
在运行过程中，单击**esc**中断

### 3. 运行订阅端 (接收并识别)
```bash
./armor_sub
```
在运行过程中，单击**esc**中断

### 4. 运行效果
两终端均会拉起窗口：
订阅端为**标记后**的图像；
发布端为**处理前**图像。
同时 两终端还会打印**帧序号**及**分辨率**。

## 通信协议说明 (Data Protocol)
**注意** 本项目所有多字节整数使用**小端字节序**编码
发送端通过共享内存传输的数据包结构如下（共计 24 字节头部 + 图像数据）：

| 字段名 (Field) | 字节数 (Bytes) | 偏移量 (Offset) | 说明 (Description) |
| :--- | :---: | :---: | :--- |
| `seq` | 8 | 0 | 帧序号，逐帧递增 |
| `timestamp_ns` | 8 | 8 | 发送端获取帧时的单调时钟时间（纳秒） |
| `width` | 4 | 16 | 图像宽度 |
| `height` | 4 | 20 | 图像高度 |
| `BGR Data` | `w * h * 3` | 24 | 图像像素数据，格式为 `CV_8UC3` (BGR)|

## 目录结构 (Structure)

```text
├── CMakeLists.txt
├── README.md
├── video_pub.cpp      # 视频发布端源码
├── armor_sub.cpp      # 视频接收与识别端源码
├── test.mp4           # 测试录屏
└── raw.mp4            # 测试视频
```

