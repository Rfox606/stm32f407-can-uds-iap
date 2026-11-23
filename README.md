# 基于STM32F407的CAN/UDS在线固件升级系统

> **当前阶段：M0已完成。** 已实现固件更新核心、Flash抽象接口、CRC32、有效性检查与9个主机端测试场景；STM32固件工程、CAN/UDS刷写和真实硬件测试尚未完成。

## 项目背景

对于部署后的MCU设备，重新接入SWD调试接口并不总是方便。本项目设计常驻Bootloader，研究通过经典CAN总线，使用ISO-TP和UDS协议更新STM32F407应用固件，同时处理传输错误、Flash写入异常与未完成升级的恢复。

本项目定位为**个人嵌入式软件工程实践**，并不声称具备量产车规软件的完整安全保证。

## 技术方案

\`\`\`text
PC/Linux升级工具（M3）
   │ 固件BIN / 版本信息 / 进度和错误处理
   ▼
经典CAN 500 kbit/s / ISO-TP / UDS（M2）
   ▼
STM32F407VGT6 常驻Bootloader（M1）
   ├─ bxCAN及接收缓冲
   ├─ ISO-TP与UDS升级事务状态机
   ├─ 固件更新核心、CRC32、元数据提交（M0已实现）
   ├─ STM32 Flash擦写、地址保护（M1待实现）
   └─ 启动判断及MSP/VTOR切换（M1待实现）
   ▼
Application：APP业务演示及版本查询（M1待实现）
\`\`\`

### Flash布局（STM32F407VG，1 MB）

| 区域 | 地址及扇区 | 大小 |
|---|---|---:|
| 常驻Bootloader | 0x08000000，扇区0～3 | 64 KB |
| 固件元数据 | 0x08010000，扇区4 | 64 KB |
| 可更新APP | 0x08020000，扇区5～11 | 896 KB |

采用单APP槽。升级中断后可能失去旧APP，但应保留Bootloader重新刷写入口；**当前不支持A/B自动回滚**。

## 当前代码与测试

| 目录 | 内容 |
|---|---|
| include/iap、src | 固件更新、Flash抽象、CRC32、镜像验证 |
| tests | 模拟Flash、9个主机端故障场景 |
| ports/stm32f407 | STM32适配位置及内存布局片段，目前未实现完整固件 |
| protocol、host、application | 后续协议、上位机及APP模块的接口说明 |
| tools | 固件大小、入口地址及CRC校验辅助工具 |
| docs | 架构、分区、协议、测试、故障案例及面试验收标准 |
| evidence | 当前实际测试日志 |

在具备CMake与C编译器的环境中运行：

\`\`\`bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/iap_core_test
\`\`\`

结果：CTest 1/1，通过；测试程序9/9场景通过。**这仅证明主机端逻辑测试，不是STM32真实Flash、CAN或掉电实测。**

## 开发任务

- [M1：STM32F407 Bootloader和APP底层工程](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/1)
- [M2：CAN、ISO-TP与UDS升级事务状态机](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/2)
- [M3：Python升级工具与联调](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/3)
- [M4：故障排查与面试准备](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/4)

详细方案见[开发路线](docs/00_roadmap.md)、[整体架构](docs/01_architecture.md)、[Flash分区](docs/02_memory_map.md)、[协议约定](docs/03_protocol.md)、[测试矩阵](docs/04_test_matrix.md)及[面试验收](docs/09_项目深度与验收标准.md)。

## 代码来源与许可

当前M0核心代码为本项目独立编写，采用MIT许可。尚未直接复制第三方Bootloader或UDS协议栈源码。后续引入协议库及STM32驱动时，将保留许可证并在[来源记录](docs/07_licenses_and_sources.md)中说明。