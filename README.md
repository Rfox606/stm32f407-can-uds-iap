# 基于STM32F407的CAN/UDS在线固件升级系统

> **当前状态：M0–M4的软件工程、源码审查、故障注入与面试材料已建立，ARM交叉编译及自动化测试通过。** 尚未进行真实开发板CAN通信、Flash擦写、APP启动或物理断电验证。项目是个人嵌入式工程实践，不代表量产车规升级系统。

## 项目背景与目标

部署后的MCU设备可能不方便重新接入SWD调试器。本项目在STM32F407VGT6上实现常驻Bootloader，让正常APP能够通过CAN诊断请求进入升级模式，再利用ISO-TP/UDS接收新固件、写入Flash、校验并启动APP。重点研究**Cortex-M启动、Flash分区、通信状态机、失败恢复及软件级故障定位**。

### 整体架构

```text
PC升级工具（Python）
   ├─ BIN和向量表检查 / CRC32和SHA256
   ├─ UDS服务、分块下载、超时与有界重试
   └─ python-can + can-isotp（真实CAN时依赖外部库）
                  │ 11位经典CAN，500 kbit/s，0x7E0/0x7E8
                  ▼
STM32F407VGT6（16 MHz HSI，CAN1 PB8/PB9接外置收发器）
   ├─ 正常APP：版本查询、运行模式查询、编程会话请求
   │    └─ SRAM预留标志 + SYSRESETREQ → Bootloader
   └─ 常驻Bootloader
        ├─ bxCAN轮询、ISO-TP固定缓冲、UDS编程会话
        ├─ 0x34 → 逐扇区擦除，0x78等待响应
        ├─ 0x36 → 写Flash、读回、允许相同块重发
        ├─ 0x37 → 整包CRC、向量校验、元数据提交
        ├─ 0x31 → 再次检查镜像有效性
        └─ 0x11 → SYSRESETREQ → 有效APP启动
```

**目前只支持单APP槽：** 升级中断后旧程序可能失效，常驻Bootloader提供重新刷写入口；**不支持自动回滚、数字签名或安全访问认证**。

## Flash及SRAM分区

| 区域 | 地址 | 大小 |
|---|---|---:|
| Bootloader，扇区0～3 | 0x08000000 | 64 KB |
| 镜像元数据，扇区4 | 0x08010000 | 64 KB |
| APP，扇区5～11 | 0x08020000 | 896 KB |
| Bootloader与APP共用的保留SRAM | 0x2001FFC0～0x2001FFFF | 64 B |

Bootloader与APP的链接脚本都排除了SRAM末尾64字节，并将初始MSP设为0x2001FFC0。APP需要通过保留内存中的双字标志与软件复位请求Bootloader；启动时还检查RCC软件复位标志，避免普通上电误进入升级模式。**保留SRAM交权机制仍需实际硬件验证。**

## UDS服务与厂家自定义数据编号

支持`0x10`编程会话、`0x22`数据读取、`0x34`请求下载、`0x36`数据传输、`0x37`结束传输、`0x31`完整校验、`0x11`复位和`0x3E`保持会话。

厂家自定义DID为：**F150=固件版本、F151=升级状态、F152=当前运行模式**（0为APP，1为Bootloader）。避免误用标准的F190（VIN）等已定义编号。请求地址固定为0x08020000；`0x37`包含本项目约定的4字节大端CRC32和4字节版本号。

详细报文和时序见[协议实现说明](docs/12_M2_协议实现与验证.md)、[M3实际问题记录](docs/14_M3_软件联调与问题排查.md)。

## 源码结构

| 路径 | 内容 |
|---|---|
| `ports/stm32f407` | 独立Bootloader/APP、启动汇编、链接脚本、Flash与CAN驱动、APP软件交权 |
| `include/iap`、`src` | Flash抽象、固件更新事务、CRC32与镜像有效性 |
| `protocol` | ISO-TP、UDS升级状态机与厂家定义的诊断接口 |
| `host` | Python升级工具、真实CAN接口、软件联调用的C协议桥接客户端 |
| `tests` | C单元测试、Python客户端测试、实际C UDS协议桥接及故障注入 |
| `tools` | ARM交叉编译、固件头检查与初次烧录元数据生成 |
| `docs`、`evidence` | 技术依据、验证范围、排查过程、面试验收标准及构建证据 |

## 构建和运行

```bash
# 独立STM32F407 Bootloader/APP固件，需要arm-none-eabi-gcc工具链
bash tools/build_firmware.sh

# 检查实际生成的BIN：该命令不需要硬件或第三方库
python3 -m host.flash_cli inspect build-arm/app.bin --version 0x010002

# C核心和Python/C端到端软件测试
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# 有真实CAN硬件时才安装和使用下面的依赖
python3 -m pip install -r host/requirements-can.txt
python3 -m host.flash_cli status --interface socketcan --channel can0
python3 -m host.flash_cli flash build-arm/app.bin --version 0x010002 \
    --interface socketcan --channel can0 --retries 2 --reset
```

实物通信前必须配置CAN接口波特率500 kbit/s、外置收发器、供电和终端电阻；软件构建成功不等于真实通信成功。

## 验证现状

**软件证据：** ARM GCC交叉编译、链接及向量表检查；C端M0更新核心与Flash边界测试、M2协议测试、M3 APP交权策略、M4 CAN应答确认门控测试；Python与实际C版UDS更新核心的跨进程联调测试。后者绕过了CAN物理层，不属于整机仿真或实物刷写。

**待硬件验证：** CAN收发与时钟误差、Flash实际擦写时长、单Bank阻塞影响、APP与Bootloader软件复位交权、实际掉电恢复和真实升级总耗时。

最新程序体积与软件审查见[《M4参数与理论分析》](docs/18_M4_参数与理论分析.md)、[《M4工程审查与验收》](docs/15_M4_工程审查与验收.md)；所有状态均应以对应提交的GitHub自动化测试结果为准。

## M4：安全门控、故障复盘与面试交付

新增`reply_gate.c`，让0x34擦除、0x37镜像提交、0x31完整校验、0x11复位及APP软件交权必须等到**对应UDS响应真正通过CAN控制器TXOK确认**后执行；CAN邮箱短暂忙时先暂存响应，再重试发送，不提前修改危险状态。

软件测试已增加3组CAN应答门控故障注入和4项CAN理论帧数计算回归。理论估算工具：

```bash
python3 tools/estimate_upgrade.py 3184
```

本项目五项交付物及技术细节见：

- [工程审查及验收结论](docs/15_M4_工程审查与验收.md)
- [四项具体技术问题及代码证据](docs/16_M4_故障复盘案例.md)
- [逐函数源码导读与18道面试追问](docs/17_M4_源码导读与面试手册.md)
- [关键参数、实际ARM占用与理论CAN吞吐](docs/18_M4_参数与理论分析.md)
- [M4构建与测试记录](evidence/M4/build_and_tests.md)

项目已经具备软件工程与面试交付材料，**尚不能宣称完成真实板级CAN刷写、上电跳转、Flash擦写和断电恢复实测**。

## 项目计划与许可证

[M1底层工程](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/1) ·
[M2刷写协议](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/2) ·
[M3上位机及联调](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/3) ·
[M4面试技术复盘](https://github.com/Rfox606/stm32f407-can-uds-iap/issues/4)

项目自主源码使用MIT许可证，`python-can`与`can-isotp`作为外部安装依赖、未将其源码复制到本仓库。所有引用资料与限制见[代码来源记录](docs/07_licenses_and_sources.md)与[最终验收标准](docs/09_项目深度与验收标准.md)。
