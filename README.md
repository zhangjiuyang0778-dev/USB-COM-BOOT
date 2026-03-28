# USB-COM-BOOT 项目说明

## 项目概述

**项目名称**：USB-COM-BOOT
**硬件平台**：MINI-GD32F103C8T6
**功能描述**：基于GD32F103C8T6的USB虚拟串口Bootloader，支持Ymodem协议固件升级

---

## 功能特点

1. **USB虚拟串口**：连接电脑后自动识别为USB虚拟串口设备，无需额外串口线
2. **Ymodem协议**：支持标准的Ymodem协议文件传输，稳定可靠
3. **地址配置**：应用程序起始地址为 `0x08004000`（16KB Bootloader空间）
4. **Flash操作**：内置Flash擦除和写入功能，支持最大48KB固件

---

## 硬件要求

- **主控芯片**：GD32F103C8T6
- **晶振**：8MHz外部晶振，系统时钟72MHz
- **USB接口**：PA11（USB_DM）、PA12（USB_DP）
- **进入Boot模式**：短接 PA0 → GND

---

## 使用方法

### 1. 进入Bootloader模式

- 短接 PA0 引脚到 GND
- 上电或复位开发板
- 板子进入Bootloader模式，USB连接电脑后识别为虚拟串口

### 2. 正常启动应用程序

- 不短接 PA0（悬空或接高电平）
- 上电后自动检测应用程序有效性
- 若应用程序有效则自动跳转执行

### 3. 固件下载流程

1. 使用串口工具（如Xshell、SecureCRT等）连接虚拟串口
2. 按下数字键 `1` 开始下载模式
3. 选择Ymodem协议发送 `.bin` 文件
4. 等待传输完成，系统自动跳转运行新固件

---

## 项目结构

```
USB-COM-BOOT/
├── CMSIS/                    # ARM Cortex-M3 内核支持文件
├── Library/                  # GD32F10x 标准库
├── Startup/                  # 启动汇编文件
├── User/                     # 用户代码
│   ├── GD32F10x_usbd_driver/   # USB设备驱动
│   ├── GD32F10x_usbfs_driver/  # USBFS驱动
│   ├── cdc_acm_core.c/h       # USB CDC虚拟串口实现
│   ├── main.c                  # 主程序入口
│   ├── ymodem.c/h              # Ymodem协议实现
│   ├── systick.c/h             # 系统延时
│   └── gd32f10x_it.c           # 中断处理
├── project/                  # Keil MDK工程文件
│   └── template.uvprojx      # 工程主文件

```

---

## 内存分配

| 区域       | 起始地址   | 大小 | 用途           |
| ---------- | ---------- | ---- | -------------- |
| Bootloader | 0x08000000 | 16KB | Bootloader程序 |
| 应用程序   | 0x08004000 | 48KB | 用户应用程序   |
| Flash末尾  | 0x0800F800 | 2KB  | 保留           |

---

## 关键配置

- **应用程序地址**：`0x08004000`
- **Flash页大小**：1KB（0x400）
- **最大固件大小**：48KB
- **USB VID/PID**：在 `cdc_acm_core.h` 中定义

---

## 开发环境

- **IDE**：Keil MDK 5.x
- **编译器**：ARMCC/ARMClang
- **调试器**：J-Link / ST-Link
- **串口工具**：Xshell、SecureCRT等支持Ymodem的工具

---

## 注意事项

1. 应用程序编译时需设置起始地址为 `0x08004000`
2. 应用程序向量表需要重映射到 `0x08004000`
3. 使用Ymodem传输时需选择正确的 `.bin` 文件
4. 传输过程中请勿断电或断开USB连接

---


---

## 版本历史

| 版本   | 日期       | 说明                                            |
| ------ | ---------- | ----------------------------------------------- |
| V1.0.0 | 2021-04-01 | 初始版本，实现USB虚拟串口Bootloader和Ymodem协议 |

---

## 许可说明

本项目基于STMicroelectronics的Ymodem实现，遵循相关开源许可协议。
