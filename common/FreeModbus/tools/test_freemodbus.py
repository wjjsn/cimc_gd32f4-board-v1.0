# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5"]
# ///

"""FreeModbus离线现场串口回归工具。

Windows示例：
    uv.exe run test_freemodbus.py --port COM31 --read-only
    uv.exe run test_freemodbus.py --port COM31
    uv.exe run test_freemodbus.py --port COM31 --repeat 50 --frame-gap-ms 4

客户寄存器表变化后，优先只改下面的REGISTER_LAYOUT。这里全部使用Modbus报文中的
0-based地址，不是30001/40001显示地址，也不是固件回调中的1-based地址。
"""

import argparse
import json
import math
import struct
import time

import serial


# 现场改表后先同步这一块，测试逻辑本身通常不用改。
REGISTER_LAYOUT = {
    "input_start": 0,       # 显示地址30001
    "input_count": 16,      # 30001-30016
    "holding_start": 0,     # 显示地址40001
    "holding_count": 15,    # 40001-40015
    "coil_start": 0,        # 显示地址00001
    "coil_count": 3,        # 00001-00003
    "discrete_start": 0,    # 显示地址10001
    "discrete_count": 5,    # 10001-10005
    "dac_address": 14,      # 显示地址40015
    "work_led_address": 2,  # 显示地址00003
}


def crc16(data: bytes) -> int:
    """计算Modbus RTU CRC16，返回主机整数；上线时低字节先发。"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def lrc(data: bytes) -> int:
    """计算Modbus ASCII LRC。"""
    return (-sum(data)) & 0xFF


class ModbusClient:
    def __init__(
        self,
        port: str,
        mode: str,
        baudrate: int,
        parity: str,
        slave: int,
        timeout: float,
    ):
        parity_value = {
            "none": serial.PARITY_NONE,
            "even": serial.PARITY_EVEN,
            "odd": serial.PARITY_ODD,
        }[parity]
        bytesize = serial.SEVENBITS if mode == "ascii" else serial.EIGHTBITS
        self.serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=bytesize,
            parity=parity_value,
            stopbits=serial.STOPBITS_ONE,
            timeout=timeout,
            write_timeout=timeout,
        )
        self.mode = mode
        self.slave = slave

    def close(self):
        self.serial.close()

    def encode(self, pdu: bytes, bad_checksum: bool = False) -> bytes:
        adu = bytes([self.slave]) + pdu
        if self.mode == "rtu":
            checksum = crc16(adu)
            if bad_checksum:
                checksum ^= 0x0001
            return adu + struct.pack("<H", checksum)

        checksum = lrc(adu)
        if bad_checksum:
            checksum ^= 0x01
        return b":" + (adu + bytes([checksum])).hex().upper().encode() + b"\r\n"

    def decode(self, raw: bytes) -> bytes:
        if self.mode == "rtu":
            if len(raw) < 5:
                raise RuntimeError(f"RTU响应太短: {raw.hex(' ')}")
            if crc16(raw[:-2]) != int.from_bytes(raw[-2:], "little"):
                raise RuntimeError(f"RTU响应CRC错误: {raw.hex(' ')}")
            return raw[:-2]

        raw = raw.strip()
        if not raw.startswith(b":"):
            raise RuntimeError(f"ASCII响应没有冒号开头: {raw!r}")
        try:
            decoded = bytes.fromhex(raw[1:].decode())
        except (UnicodeDecodeError, ValueError) as exc:
            raise RuntimeError(f"ASCII响应不是合法十六进制: {raw!r}") from exc
        if len(decoded) < 4 or lrc(decoded[:-1]) != decoded[-1]:
            raise RuntimeError(f"ASCII响应LRC错误: {raw!r}")
        return decoded[:-1]

    def exchange(
        self,
        pdu: bytes,
        expect_response: bool = True,
        bad_checksum: bool = False,
    ) -> bytes:
        frame = self.encode(pdu, bad_checksum)
        self.serial.reset_input_buffer()
        self.serial.write(frame)
        self.serial.flush()

        if not expect_response:
            time.sleep(0.45)
            return self.serial.read_all()

        if self.mode == "ascii":
            raw = self.serial.read_until(b"\n")
        else:
            # 先读地址、功能码和第三字节，再按功能码决定后续长度，避免等满超时。
            header = self.serial.read(3)
            if len(header) == 3 and header[1] & 0x80:
                raw = header + self.serial.read(2)
            elif len(header) == 3 and header[1] in (1, 2, 3, 4):
                raw = header + self.serial.read(header[2] + 2)
            elif len(header) == 3 and header[1] in (5, 6):
                raw = header + self.serial.read(5)
            else:
                raw = header

        if not raw:
            raise TimeoutError(
                "设备无响应。先检查TX/RX是否交叉、GND是否共地、从站地址、"
                f"{self.mode.upper()}串口格式；发送帧={frame!r}"
            )

        response = self.decode(raw)
        if response[0] != self.slave:
            raise RuntimeError(f"响应从站地址不对: {response.hex(' ')}")
        return response[1:]

    @staticmethod
    def check_exception(response: bytes, expected_function: int):
        if not response:
            raise RuntimeError("响应PDU为空")
        if response[0] & 0x80:
            code = response[1] if len(response) > 1 else -1
            hint = {
                1: "设备不支持这个功能码",
                2: "地址越界，重点检查start/count和地址是否差1",
                3: "写入值非法，或float没有整对写",
                4: "设备处理请求时发生内部错误",
            }.get(code, "未知异常")
            raise RuntimeError(
                f"功能码{expected_function:#04x}返回Modbus异常{code:#04x}: {hint}"
            )

    def read_registers(self, function: int, address: int, count: int) -> list[int]:
        response = self.exchange(struct.pack(">BHH", function, address, count))
        self.check_exception(response, function)
        if response[0] != function or response[1] != count * 2:
            raise RuntimeError(f"寄存器响应长度不对: {response.hex(' ')}")
        return list(struct.unpack(f">{count}H", response[2:]))

    def read_bits(self, function: int, address: int, count: int) -> list[bool]:
        response = self.exchange(struct.pack(">BHH", function, address, count))
        self.check_exception(response, function)
        if response[0] != function:
            raise RuntimeError(f"位响应功能码不对: {response.hex(' ')}")
        data = response[2 : 2 + response[1]]
        return [bool(data[i // 8] & (1 << (i % 8))) for i in range(count)]

    def write_register(self, address: int, value: int):
        request = struct.pack(">BHH", 6, address, value)
        response = self.exchange(request)
        self.check_exception(response, 6)
        if response != request:
            raise RuntimeError(f"写单寄存器回显不一致: {response.hex(' ')}")

    def write_coil(self, address: int, value: bool):
        request = struct.pack(">BHH", 5, address, 0xFF00 if value else 0x0000)
        response = self.exchange(request)
        self.check_exception(response, 5)
        if response != request:
            raise RuntimeError(f"写单线圈回显不一致: {response.hex(' ')}")


def registers_to_float(registers: list[int], word_order: str) -> float:
    if len(registers) != 2:
        raise ValueError("float必须正好使用两个寄存器")
    words = registers if word_order == "high" else list(reversed(registers))
    return struct.unpack(">f", struct.pack(">HH", *words))[0]


def run(args) -> dict:
    result = {
        "port": args.port,
        "mode": args.mode,
        "read_only": args.read_only,
        "tests": [],
    }

    def check(name, action):
        try:
            detail = action()
            result["tests"].append({"name": name, "ok": True, "detail": detail})
            print(f"PASS {name}: {detail}")
            return detail
        except Exception as exc:
            result["tests"].append({"name": name, "ok": False, "detail": str(exc)})
            print(f"FAIL {name}: {exc}")
            raise

    client = ModbusClient(
        args.port,
        args.mode,
        args.baudrate,
        args.parity,
        args.slave,
        args.timeout,
    )
    try:
        inputs = None
        for index in range(args.repeat):
            inputs = check(
                f"读输入寄存器 {index + 1}/{args.repeat}",
                lambda: client.read_registers(
                    4,
                    REGISTER_LAYOUT["input_start"],
                    REGISTER_LAYOUT["input_count"],
                ),
            )
            if index + 1 < args.repeat and args.frame_gap_ms > 0:
                time.sleep(args.frame_gap_ms / 1000.0)

        if inputs is None:
            raise RuntimeError("repeat必须至少为1")

        for name, offset in (("CH0", 0), ("CH1", 2), ("CH2", 4)):
            if len(inputs) >= offset + 2:
                value = check(
                    f"解析{name} float",
                    lambda offset=offset: registers_to_float(
                        inputs[offset : offset + 2], args.word_order
                    ),
                )
                if not math.isfinite(value):
                    raise RuntimeError(f"{name}解析结果不是有限数: {value}")

        holdings = check(
            "读保持寄存器",
            lambda: client.read_registers(
                3,
                REGISTER_LAYOUT["holding_start"],
                REGISTER_LAYOUT["holding_count"],
            ),
        )
        coils = check(
            "读线圈",
            lambda: client.read_bits(
                1,
                REGISTER_LAYOUT["coil_start"],
                REGISTER_LAYOUT["coil_count"],
            ),
        )
        check(
            "读离散输入",
            lambda: client.read_bits(
                2,
                REGISTER_LAYOUT["discrete_start"],
                REGISTER_LAYOUT["discrete_count"],
            ),
        )

        def illegal_address():
            response = client.exchange(struct.pack(">BHH", 4, 0x7FFF, 1))
            if response != bytes([0x84, 0x02]):
                raise RuntimeError(f"期望84 02，实际为{response.hex(' ')}")
            return response.hex(" ")

        check("非法地址返回异常02", illegal_address)

        def bad_checksum():
            response = client.exchange(
                struct.pack(">BHH", 4, REGISTER_LAYOUT["input_start"], 1),
                expect_response=False,
                bad_checksum=True,
            )
            if response:
                raise RuntimeError(f"设备错误地响应了坏校验帧: {response!r}")
            return "已静默丢弃"

        check("坏CRC/LRC静默丢弃", bad_checksum)

        if args.read_only:
            print("只读模式：跳过DAC和工作灯写入测试。")
        else:
            dac_index = (
                REGISTER_LAYOUT["dac_address"] - REGISTER_LAYOUT["holding_start"]
            )
            if not 0 <= dac_index < len(holdings):
                raise RuntimeError("REGISTER_LAYOUT中的dac_address不在保持寄存器读取范围内")
            old_dac = holdings[dac_index]

            def write_dac():
                test_value = 1234 if old_dac != 1234 else 1235
                try:
                    client.write_register(REGISTER_LAYOUT["dac_address"], test_value)
                    actual = client.read_registers(
                        3, REGISTER_LAYOUT["dac_address"], 1
                    )[0]
                    if actual != test_value:
                        raise RuntimeError(f"DAC读回{actual}，期望{test_value}")
                    return {"written": test_value, "restored": old_dac}
                finally:
                    client.write_register(REGISTER_LAYOUT["dac_address"], old_dac)

            check("写入并恢复DAC保持寄存器", write_dac)

            led_index = (
                REGISTER_LAYOUT["work_led_address"] - REGISTER_LAYOUT["coil_start"]
            )
            if not 0 <= led_index < len(coils):
                raise RuntimeError("REGISTER_LAYOUT中的work_led_address不在线圈读取范围内")
            old_led = coils[led_index]

            def write_led():
                try:
                    client.write_coil(REGISTER_LAYOUT["work_led_address"], not old_led)
                    actual = client.read_bits(
                        1, REGISTER_LAYOUT["work_led_address"], 1
                    )[0]
                    if actual == old_led:
                        raise RuntimeError("工作灯线圈写入后状态没有变化")
                    return {"written": not old_led, "restored": old_led}
                finally:
                    client.write_coil(REGISTER_LAYOUT["work_led_address"], old_led)

            check("写入并恢复工作灯线圈", write_led)
    finally:
        client.close()

    result["ok"] = all(item["ok"] for item in result["tests"])
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="GD32F4 FreeModbus RTU/ASCII离线现场回归工具"
    )
    parser.add_argument("--port", default="COM31", help="Windows串口名，默认COM31")
    parser.add_argument("--mode", choices=["rtu", "ascii"], default="rtu")
    parser.add_argument("--baudrate", type=int, default=19200)
    parser.add_argument("--parity", choices=["none", "even", "odd"], default="even")
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=1.5, help="单次串口等待秒数")
    parser.add_argument(
        "--word-order",
        choices=["high", "low"],
        default="high",
        help="32位字序：high=ABCD，low=CDAB",
    )
    parser.add_argument(
        "--read-only",
        action="store_true",
        help="只读和异常测试，不改变DAC、工作灯或持久参数",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=1,
        help="连续读取整段输入寄存器的次数，默认1",
    )
    parser.add_argument(
        "--frame-gap-ms",
        type=float,
        default=4.0,
        help="repeat测试两帧之间的等待毫秒，RTU默认4ms",
    )
    args = parser.parse_args()
    if not 1 <= args.slave <= 247:
        parser.error("slave必须在1到247之间")
    if args.repeat < 1:
        parser.error("repeat必须至少为1")
    if args.frame_gap_ms < 0:
        parser.error("frame-gap-ms不能为负数")
    run(args)
