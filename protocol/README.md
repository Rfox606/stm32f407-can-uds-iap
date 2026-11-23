# M2协议子系统

**本目录已经有实际C代码，不再是占位结构。**

- `include/isotp.h`、`src/isotp.c`：固定缓冲的经典CAN ISO-TP收发与流控状态机。
- `include/uds_boot.h`、`src/uds_boot.c`：面向单APP槽Bootloader的UDS服务和异步擦除/校验。
- `tests/test_m2_protocol.c`：正常和异常协议测试。
- STM32接入路径：`ports/stm32f407/src/bxcan.c` → `boot_main.c` → ISO-TP → UDS → `iap_updater`。

具体帧格式、时序、风险和现阶段证据见[《M2协议实现与验证》](../docs/12_M2_协议实现与验证.md)。

当前为专项子集，不是完整ISO-TP/UDS标准认证协议栈；未进行硬件联调。正式使用需要为CAN添加实际收发器，适配时钟与波特率，并进行总线及Flash时序验证。
