# M4：实际ARM目标构建、测试与已解决问题的证据索引

日期：2026-11-24；开发分支：`feature/m4-hardening-interview`。所有软件测试数字以下列实际GitHub Actions运行为依据。

## 一、ARM GCC构建（修复应答顺序后）

实际成功记录：[ARM交叉编译](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45304932200)，对应提交`018fa29eb8fe0edb94bea99ff699306b2765ebe2`。

```text
Bootloader: boot.bin = 6312 B
  boot.elf text=6312 B, data=0 B, bss=768 B
  MSP=0x2001FFC0; Reset=0x08000189
APP: app.bin = 3188 B
  app.elf text=3188 B, data=0 B, bss=704 B
  MSP=0x2001FFC0; Reset=0x08020199
Factory metadata: 20 B at 0x08010000
```

这些数据是**当前所述提交**用ARM GCC生成的实际固件体积与静态RAM占用，不是物理开发板程序运行时测量值；`bss`不含实时栈峰值。

## 二、软件测试

[对应软件测试记录](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45304932172)。

- **CTest 6/6套件通过**：`boot_core`、`flash_layout`、`m2_protocol`、`handoff_policy`、`reply_gate`、`host_updater`。
- Python与实际C版UDS更新核心：**9/9测试通过**；覆盖正常下载、0x36确认丢失、CRC错误、运行时状态丢失后恢复、块序号回绕、APP交权请求顺序及客户端超时。
- M4新增`tests/test_reply_gate.c`：**3/3场景通过**，对CAN发送邮箱繁忙、提交失败、CAN未确认以及APP交权的时序进行**软件注入**。真实CAN物理发送未开展。
- M4新增`tests/test_estimate_upgrade.py`：**4/4公式测试通过**，验证256B块使用39帧、3188B镜像0x36阶段使用487帧，并核对非实测的总线占用下界。参见[含四项测试日志的CI记录](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45304460115)。

## 三、可定位的实际开发问题

1. **M4审查确认的时序缺陷**：0x78未真正发送仍可能执行Flash擦除、APP 0x50 02未发送仍可能软件复位。修正为应答暂存→提交→TXOK确认→一次性动作授权，见`protocol/src/reply_gate.c`、`boot_main.c`、`app_main.c`和`test_reply_gate.c`。
2. **M4后续状态顺序审查**：复位/交权确认到达时，优先消费确认并执行动作，避免新诊断请求覆盖已确认状态。见`boot_main.c`与`app_main.c`对应提交。
3. **M3真实ARM链接失败**：`undefined reference to bxcan_send_response/isotp_init/app_uds_request/boot_handoff_request`，因APP新增代码未纳入链接。见[失败记录](https://github.com/Rfox606/stm32f407-can-uds-iap/actions/runs/45159785472)及后续成功构建。
4. **M3软件丢应答复现**：使用C版UDS更新核心和Flash测试接口，刻意吞掉一个0x36的正响应，主机超时重发相同数据块，成功完成下载；参见`tests/uds_bridge.c`、`tests/test_host_updater.py`。
5. **M2源码审查**：整包CRC正确但APP入口向量不合法时也可进入提交，已经在`src/updater.c`增加复位向量和MSP检查，并更新主机测试。

## 四、证据边界与结果类型

- 实际编译和软件测试：**完成，日志可查**。
- 软件故障注入：**完成，C/Python代码及断言可查**。
- CAN速率、升级时间：**只做参数推导**，相关公式及排除项见`docs/18_M4_参数与理论分析.md`。
- 物理Flash写入、CAN波形、HardFault寄存器、真实掉电、板级APP交权：**未进行**。

此文件是索引而不是硬件测试报告，不能将这里的软件测试描述为上板实验。
