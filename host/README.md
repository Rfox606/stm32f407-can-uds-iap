# Python上位机固件升级工具（M3）

主入口：\`python3 -m host.flash_cli\`。离线检查不需要第三方库；真实CAN通信依赖\`python-can\`和\`can-isotp\`。**目前仅软件级联调通过，真实CAN/Flash/断电行为需硬件验证。**

## 运行方法

\`\`\`bash
# 离线解析可执行APP镜像、验证MSP和复位入口、计算CRC32及SHA256
python3 -m host.flash_cli inspect build-arm/app.bin --version 0x010002

# 仅实物通信时安装依赖
python3 -m pip install -r host/requirements-can.txt

# Linux先正确配置can0到500 kbit/s，接好收发器和终端电阻
python3 -m host.flash_cli status --interface socketcan --channel can0

# 设备需处于Bootloader模式，先请求版本、再执行升级和可选复位
python3 -m host.flash_cli flash build-arm/app.bin --version 0x010002 \
    --interface socketcan --channel can0 --retries 2 --reset
\`\`\`

## 完整请求流程

\`10 02\`进入编程会话 → \`34 00 44 + addr + size\` 请求下载 → 多次\`36 + seq + 256字节以内\` → \`37 + CRC32 + version\` → \`31 01 FF 01\`完整校验 → \`22 F1 90\`读取已提交版本 → 可选\`11 01\`请求复位。

其中\`0x37\`的CRC+版本为本项目自定义扩展，**不能认为与任意UDS ECU兼容**。现有APP演示程序不响应CAN诊断请求，所以上位机在复位前查询新版本，复位后是否成功运行APP仍需实际验证。

重传策略：**仅\`0x36\`响应超时后重发完全相同的数据块**，最多由\`--retries\`指定的次数；\`0x34\`和\`0x37\`一旦超时不自动重发，因为操作可能已经执行。设备端使用读回对比保证已确认数据块的幂等行为。

\`0x7F SID 0x78\`表示请求处理中，上位机继续等待同一请求的最终响应，受单次和总等待上限约束。错误码与阶段日志由CLI输出。

## 不需要实物的实际验证

\`\`\`bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
\`\`\`

这里的\`host_updater\`测试使用真实C版UDS服务和更新核心，通过一个标准输入/输出桥接收Python请求，Flash由既有测试接口提供；测试覆盖完整固件下载、响应丢失重发、CRC损坏、升级中断、块序号跨越255等。**这不涉及真实CAN帧或物理Flash，因此不能代表上板成功。**

## 上位机依赖及参考

- [python-can](https://python-can.readthedocs.io/)：连接SocketCAN和USB-CAN设备。
- [can-isotp](https://can-isotp.readthedocs.io/)：ISO-TP的多帧、流控和超时处理。M3使用其\`NotifierBasedCanStack\`，不重复实现上位机ISO-TP。
- 设备端协议细节见\`docs/12_M2_协议实现与验证.md\`。
