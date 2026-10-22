"""Minimal ControlCAN transport for the CANalyst-II bench updater."""

import ctypes as ct
from collections import deque
from pathlib import Path
import time

import can


DEVICE_TYPE = 4  # VCI_USBCAN2
DEVICE_INDEX = 0
ERROR = 0xFFFFFFFF


class InitConfig(ct.Structure):
    _fields_ = [
        ("acceptance_code", ct.c_uint32), ("acceptance_mask", ct.c_uint32),
        ("reserved", ct.c_uint32), ("filter", ct.c_uint8),
        ("timing0", ct.c_uint8), ("timing1", ct.c_uint8), ("mode", ct.c_uint8),
    ]


class Frame(ct.Structure):
    _fields_ = [
        ("id", ct.c_uint32), ("timestamp", ct.c_uint32),
        ("time_flag", ct.c_uint8), ("send_type", ct.c_uint8),
        ("remote", ct.c_uint8), ("extended", ct.c_uint8),
        ("length", ct.c_uint8), ("data", ct.c_uint8 * 8),
        ("reserved", ct.c_uint8 * 3),
    ]


class CanalystBus:
    def __init__(self, driver, channel):
        if channel not in (1, 2):
            raise ValueError("CANalyst-II channel must be 1 or 2")
        if ct.sizeof(InitConfig) != 16 or ct.sizeof(Frame) != 24:
            raise RuntimeError("Unexpected ControlCAN structure layout")
        self.dll = ct.WinDLL(str(Path(driver).resolve()))
        for name, argtypes in {
            "VCI_OpenDevice": [ct.c_uint32] * 3,
            "VCI_CloseDevice": [ct.c_uint32] * 2,
            "VCI_InitCAN": [ct.c_uint32] * 3 + [ct.POINTER(InitConfig)],
            "VCI_StartCAN": [ct.c_uint32] * 3,
            "VCI_ResetCAN": [ct.c_uint32] * 3,
            "VCI_Transmit": [ct.c_uint32] * 3 + [ct.POINTER(Frame), ct.c_uint32],
            "VCI_Receive": [ct.c_uint32] * 3 + [ct.POINTER(Frame), ct.c_uint32, ct.c_int],
        }.items():
            fn = getattr(self.dll, name)
            fn.argtypes = argtypes
            fn.restype = ct.c_uint32
        self.channel = channel - 1
        self.pending = deque()
        self.open = self.dll.VCI_OpenDevice(DEVICE_TYPE, DEVICE_INDEX, 0) == 1
        if not self.open:
            raise OSError("Cannot open CANalyst-II")
        try:
            # ControlCAN timing registers: 500 kbit/s, normal mode, accept all IDs.
            config = InitConfig(0, 0xFFFFFFFF, 0, 1, 0, 28, 0)
            if self.dll.VCI_InitCAN(DEVICE_TYPE, DEVICE_INDEX, self.channel, ct.byref(config)) != 1:
                raise OSError("Cannot initialize CANalyst-II channel")
            if self.dll.VCI_StartCAN(DEVICE_TYPE, DEVICE_INDEX, self.channel) != 1:
                raise OSError("Cannot start CANalyst-II channel")
            self._drain()
        except BaseException:
            self.close()
            raise

    def _drain(self):
        frames = (Frame * 32)()
        while True:
            count = self.dll.VCI_Receive(DEVICE_TYPE, DEVICE_INDEX, self.channel, frames, 32, 0)
            if count == ERROR:
                raise OSError("CANalyst-II receive failed")
            if count == 0:
                return

    def send(self, message, timeout=None):
        if message.is_remote_frame or len(message.data) > 8:
            raise ValueError("Only classic data frames are supported")
        if message.arbitration_id > (0x1FFFFFFF if message.is_extended_id else 0x7FF):
            raise ValueError("CAN identifier out of range")
        frame = Frame()
        frame.id = message.arbitration_id
        frame.extended = int(message.is_extended_id)
        frame.length = len(message.data)
        for i, byte in enumerate(message.data):
            frame.data[i] = byte
        if self.dll.VCI_Transmit(DEVICE_TYPE, DEVICE_INDEX, self.channel, ct.byref(frame), 1) != 1:
            raise OSError("CANalyst-II transmit failed")

    def recv(self, timeout=None):
        if self.pending:
            return self.pending.popleft()
        deadline = time.monotonic() + (timeout if timeout is not None else 3600)
        frames = (Frame * 32)()
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            count = self.dll.VCI_Receive(DEVICE_TYPE, DEVICE_INDEX, self.channel, frames, 32,
                                         max(1, min(1000, int(remaining * 1000))))
            if count == ERROR:
                raise OSError("CANalyst-II receive failed")
            for frame in frames[:count]:
                self.pending.append(can.Message(arbitration_id=frame.id,
                                                is_extended_id=bool(frame.extended),
                                                is_remote_frame=bool(frame.remote),
                                                data=bytes(frame.data[:frame.length])))
            if self.pending:
                return self.pending.popleft()

    def close(self):
        if self.open:
            self.dll.VCI_ResetCAN(DEVICE_TYPE, DEVICE_INDEX, self.channel)
            self.dll.VCI_CloseDevice(DEVICE_TYPE, DEVICE_INDEX)
            self.open = False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
