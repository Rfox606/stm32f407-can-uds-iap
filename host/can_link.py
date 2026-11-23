"""真实CAN接口：延迟加载python-can和can-isotp。

Windows可选由python-can支持的适配器；Linux可用SocketCAN can0/vcan0。
所连接的CAN物理网络必须在此程序启动前正确配置到500 kbit/s。
"""
from __future__ import annotations

import logging
from typing import Any

class CanIsoTpLink:
    def __init__(self, *, interface: str, channel: str,
                 request_id: int = 0x7E0, response_id: int = 0x7E8) -> None:
        try:
            import can
            import isotp
        except ImportError as exc:
            raise RuntimeError(
                "真实CAN模式缺少依赖：pip install -r host/requirements-can.txt"
            ) from exc
        self.bus: Any = can.Bus(interface=interface, channel=channel)
        self.notifier: Any = None
        self.stack: Any = None
        try:
            self.notifier = can.Notifier(self.bus, [])
            address = isotp.Address(isotp.AddressingMode.Normal_11bits,
                                    txid=request_id, rxid=response_id)
            self.stack = isotp.NotifierBasedCanStack(
                self.bus, self.notifier, address=address,
                error_handler=lambda exc: logging.warning("ISO-TP异常：%s", exc),
                params={"blocking_send": True, "tx_padding": 0,
                        "tx_data_length": 8, "max_frame_size": 320,
                        "blocksize": 0, "stmin": 0,
                        "rx_flowcontrol_timeout": 1000,
                        "rx_consecutive_frame_timeout": 1000})
            self.stack.start()
        except BaseException:
            self.close()
            raise

    def send(self, payload: bytes) -> None:
        assert self.stack is not None
        # 发送完整UDS应用层请求，多帧分包交给成熟的can-isotp库。
        self.stack.send(payload, send_timeout=8)

    def recv(self, timeout_s: float) -> bytes | None:
        assert self.stack is not None
        result = self.stack.recv(block=True, timeout=timeout_s)
        return bytes(result) if result is not None else None

    def close(self) -> None:
        if self.stack is not None:
            self.stack.stop()
            self.stack = None
        if self.notifier is not None:
            self.notifier.stop()
            self.notifier = None
        if getattr(self, "bus", None) is not None:
            self.bus.shutdown()
            self.bus = None

    def __enter__(self) -> "CanIsoTpLink":
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()
