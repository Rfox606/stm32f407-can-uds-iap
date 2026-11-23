# CAN / ISO-TP / UDS协议模块（待实现）

此目录用于后续协议适配，**当前不是已实现的UDS协议栈**。详细约定见docs/03_protocol.md。

建议在CAN接收中断与前台状态机之间使用有界缓冲：
CAN收帧 → ISO-TP处理 → UDS请求 → 更新核心

- 0x34连接iap_updater_begin；
- 0x36连接iap_updater_write；
- 0x37连接iap_updater_finish；
- 会话超时、异常响应与重试由协议适配层处理。

第三方候选代码为MIT许可的driftregion/iso14229，接入前需固定版本并记录许可证。
