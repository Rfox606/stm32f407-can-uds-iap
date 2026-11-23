# M3软件交叉编译与端到端协议测试记录

记录日期：2026-11-23。测试分支：`feature/m3-host-updater`。本文件保留已实际执行的自动化构建和软件测试证据，不包含实物测试结果。

## 1. 实际ARM交叉编译

构建记录：[GitHub ARM GCC构建 #45160701634](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45160701634)，该次提交：`0b7cdc15aa1d449f3b2119af5511720de4450778`。

```text
boot.bin: 6184 bytes
boot.elf: text=6184, data=0, bss=756  (单位：字节)
app.bin:  3028 bytes
app.elf:  text=3028, data=0, bss=692  (单位：字节)
boot MSP=0x2001FFC0, reset=0x08000189
app  MSP=0x2001FFC0, reset=0x08020199
Factory metadata: 20 bytes at 0x08010000
```

这些是**该提交与ARM GCC构建配置**下的真实链接及`size`结果；当前分支后续若修改源码，应以最新构建结果为准。Flash分区容量为Bootloader 64 KB、APP 896 KB，未超出容量约束。SRAM末尾64 B预留给软件交权标志。

## 2. 软件测试

测试记录：[GitHub主机测试 #45160701646](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45160701646)。

- CTest：**5/5套件通过**，含更新核心、Flash分区、ISO-TP/UDS、保留SRAM复位标志纯逻辑、Python/C联调。
- Python单元及联调：**9/9项通过**，详细测试命令：

```bash
IAP_UDS_BRIDGE=./build/iap_uds_bridge \
  python3 -m unittest discover -s tests -p test_host_updater.py -v
```

在真实C版UDS服务和更新核心上的软件联调覆盖：正常固件升级、已写入数据块确认丢失后的幂等重发、0x37之前破坏镜像造成CRC失败、部分传输后丢失易失状态并重新刷写、块序号255→0回绕。APP自动切换Bootloader的诊断请求顺序采用Python应答脚本测试；真正的SRAM与RCC软件复位行为仅完成代码实现和纯逻辑验证。

## 3. M3实际错误及修复

[APP链接失败记录 #45159785472](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45159785472)：`undefined reference to bxcan_send_response/isotp_init/app_uds_request/boot_handoff_request`等。原因是构建脚本未纳入新增APP依赖源码，已加入构建目标并重新交叉编译。

协议合理性检查发现M2把`F190`用于版本号，而UDS已定义`F190`为车辆VIN。M3把版本、状态、模式统一迁移到厂家自定义的`F150/F151/F152`，并修改C、Python和测试。

为处理整包CRC时可能超过P2时间，`0x37`改为先返回`7F 37 78`，再执行延迟提交；与设备端源码及软件测试对应。

## 4. 验证边界

**已经验证：** ARM目标编译/链接、真实C版UDS业务协议的软件联调、主机故障注入及回复一致性、Python客户端状态机。

**尚未验证：** 真实CAN适配器与CAN总线、时钟与收发器工作状态、STM32 Flash物理擦写、APP→Bootloader软件复位标志保持、上电跳转实际运行、硬件断电测试及升级吞吐实测。当前不依据软件通过结果断言实物成功。
