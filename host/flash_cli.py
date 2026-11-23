#!/usr/bin/env python3
"""STM32F407 CAN/UDS 升级工具（中文命令行）。

示例：
 python3 -m host.flash_cli inspect build-arm/app.bin --version 0x010002
 python3 -m host.flash_cli status --interface socketcan --channel can0
 python3 -m host.flash_cli flash build-arm/app.bin --version 0x010002 --channel can0
"""
from __future__ import annotations
import argparse
import json
import logging
import sys

from .uds_client import Firmware, UpgradeError, UdsClient, Timeouts


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="STM32F407 CAN/UDS固件升级工具（无实物时可离线检查固件）")
    sub = p.add_subparsers(dest="command", required=True)
    inspect = sub.add_parser("inspect", help="离线检查BIN和生成校验信息，不访问CAN设备")
    inspect.add_argument("image")
    inspect.add_argument("--version", type=lambda x: int(x, 0), default=0x010000)

    for action in ("status", "flash"):
        s = sub.add_parser(action, help="查询设备版本" if action == "status" else "执行完整升级")
        s.add_argument("--interface", default="socketcan", help="python-can接口名")
        s.add_argument("--channel", default="can0", help="如can0、vcan0或具体USB-CAN通道")
        s.add_argument("--request-id", type=lambda x: int(x, 0), default=0x7E0)
        s.add_argument("--response-id", type=lambda x: int(x, 0), default=0x7E8)
        s.add_argument("--response-timeout", type=float, default=2.0)
        s.add_argument("--pending-timeout", type=float, default=6.0)
        s.add_argument("--pending-total", type=float, default=90.0)
        if action == "flash":
            s.add_argument("image")
            s.add_argument("--version", type=lambda x: int(x, 0), required=True)
            s.add_argument("--retries", type=int, default=2, help="仅针对0x36确认超时重发")
            s.add_argument("--reset", action="store_true", help="成功后请求ECU复位")
    args = p.parse_args(argv)
    try:
        if args.command == "inspect":
            firm = Firmware.open(args.image)
            print(json.dumps(firm.summary(args.version), ensure_ascii=False, indent=2))
            return 0

        if not 0 <= args.request_id <= 0x7FF or not 0 <= args.response_id <= 0x7FF:
            raise UpgradeError("仅支持标准11位CAN标识符")
        from .can_link import CanIsoTpLink
        timeouts = Timeouts(args.response_timeout, args.pending_timeout, args.pending_total)
        with CanIsoTpLink(interface=args.interface, channel=args.channel,
                           request_id=args.request_id, response_id=args.response_id) as link:
            client = UdsClient(link, timeouts, log=lambda m: print(m, file=sys.stderr))
            if args.command == "status":
                mode = client.get_mode()
                print(f"设备当前模式：{'APP' if mode == 0 else 'Bootloader'}")
                print(f"设备提交的固件版本：0x{client.get_version():08X}")
                print("注意：该版本仅由元数据CRC验证；完整APP CRC需通过0x31检查。")
                return 0
            firm = Firmware.open(args.image)
            print(json.dumps(firm.summary(args.version), ensure_ascii=False, indent=2))
            print("请确认已进入Bootloader恢复模式，并且设备供电和CAN收发器稳定。")
            result = client.upgrade(
                firm, version=args.version, retries=args.retries, reset=args.reset,
                progress=lambda done, total: print(f"\r固件传输 {done}/{total} 字节（{100*done/total:.1f}%）",
                                                  end="\n" if done == total else "", flush=True))
            print(f"\n完整校验及复位前版本回读通过：0x{result:08X}")
            if args.reset:
                print("已请求复位；APP实际启动状态需通过实物观察或后续APP诊断功能确认。")
            return 0
    except (OSError, UpgradeError, ValueError, RuntimeError) as exc:
        print(f"操作失败：{exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("\n用户中断。可能留下部分更新的固件，需要重新进入Bootloader刷写。", file=sys.stderr)
        return 130
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
