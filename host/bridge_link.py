"""仅用于软件级端到端测试：对接真正的C UDS逻辑，但不模拟CAN物理层。

每个Python send()对应C的uds_boot_request()；擦除和校验按C状态机
继续推进。测试故障注入针对协议层，不代表真实闪存与总线时序。
"""
from __future__ import annotations
from queue import Queue, Empty
import subprocess
from threading import Thread

class CBridgeLink:
    def __init__(self, executable: str, *args: str) -> None:
        self.p = subprocess.Popen([executable,*args], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True, bufsize=1)
        self.q: Queue[str | None] = Queue()
        self.reader = Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self) -> None:
        assert self.p.stdout
        for line in self.p.stdout:
            self.q.put(line.strip())
        self.q.put(None)

    def send(self, payload: bytes) -> None:
        if self.p.poll() is not None:
            raise RuntimeError(f"C协议进程已退出：{self.p.returncode}")
        assert self.p.stdin is not None
        self.p.stdin.write("S " + payload.hex() + "\n")
        self.p.stdin.flush()

    def recv(self, timeout_s: float) -> bytes | None:
        try:
            line = self.q.get(timeout=timeout_s)
        except Empty:
            return None
        if line is None:
            raise RuntimeError("C协议测试进程异常结束")
        if line == "TIMEOUT":
            return None
        if not line.startswith("R "):
            raise RuntimeError(f"无法识别C协议响应：{line}")
        return bytes.fromhex(line[2:])

    def reset(self) -> None:
        assert self.p.stdin is not None
        self.p.stdin.write("RESET\n")
        self.p.stdin.flush()
        try:
            answer = self.q.get(timeout=2.0)
        except Empty as exc:
            raise RuntimeError("C协议复位命令未响应") from exc
        if answer != "OK":
            raise RuntimeError(f"复位命令结果异常：{answer}")

    def close(self) -> None:
        if self.p.poll() is None:
            try:
                assert self.p.stdin is not None
                self.p.stdin.write("QUIT\n")
                self.p.stdin.flush()
                self.p.communicate(timeout=2.0)
            except (OSError, subprocess.TimeoutExpired):
                self.p.kill()
                self.p.communicate()
        self.reader.join(timeout=0.5)

    def __enter__(self) -> "CBridgeLink":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
