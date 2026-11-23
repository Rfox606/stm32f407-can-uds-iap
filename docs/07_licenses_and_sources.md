# 代码来源与许可证

M0的src/和include/下的C源码为本项目独立编写，尚未复制其他GitHub仓库的协议实现。当前原创内容按MIT许可证发布。

| 来源 | 用途 | 处理方式 |
|---|---|---|
| ST RM0090及芯片资料 | STM32F407 Flash分区、编程与启动机制 | 技术资料引用 |
| STM32CubeF4 HAL/CMSIS | 后续驱动和启动文件 | 未来移植时保留ST许可证 |
| driftregion/iso14229 | UDS/ISO-TP候选协议库 | MIT，尚未复制源码 |
| GamzeAkcay/stm32-uds-bootloader | STM32F407故障排查案例 | 仅参考；未确认许可证 |
| cloudsea-yunhong/stm32-can-uds-bootloader | 双槽与刷写流程参考 | MIT，仅参考 |
| feaser/openblt | 成熟CAN刷写方案比较 | GPLv3/商业许可，仅参考 |

引入第三方源码时，应记录上游地址、固定提交号、源文件、修改情况、许可证和测试情况。不应直接把开源实现说成独立编写。
