# 代码来源与许可证

M0–M3的项目C/Python核心代码为本项目独立编写，包括M2的ISO-TP/UDS升级子集和M3上位机业务代码。项目源码使用MIT许可证。下述外部软件通过依赖安装使用，未复制其实现进本仓库。

| 来源 | 用途 | 处理方式 |
|---|---|---|
| ST RM0090及芯片资料 | STM32F407 Flash分区、编程与启动机制 | 技术资料引用 |
| STM32CubeF4 HAL/CMSIS | 参考STM32F407启动/中断向量定义 | M1自行写最小启动及寄存器访问文件；未复制官方HAL启动代码 |
| driftregion/iso14229 | 比较UDS/ISO-TP状态机及实现范围 | MIT，仅参考；M2源码自行实现受限子集 |
| GamzeAkcay/stm32-uds-bootloader | STM32F407故障排查案例 | 仅参考；未确认许可证 |
| cloudsea-yunhong/stm32-can-uds-bootloader | 双槽与刷写流程参考 | MIT，仅参考 |
| feaser/openblt | 成熟CAN刷写方案比较 | GPLv3/商业许可，仅参考 |
| [python-can](https://github.com/hardbyte/python-can) | M3真实CAN收发适配器接口 | BSD-3-Clause，运行时用pip安装，不复制源码 |
| [can-isotp](https://github.com/pylessard/python-can-isotp) | M3上位机分包与流控 | MIT，运行时用pip安装，不复制源码 |
| UDS标准DID资料 | 厂家自定义标识符取值研究 | F150/F151/F152在厂家自定义范围，避免占用F190（VIN） |

引入第三方源码时，应记录上游地址、固定提交号、源文件、修改情况、许可证和测试情况。不应直接把开源实现说成独立编写。
