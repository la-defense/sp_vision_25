#!/usr/bin/env python3
"""Read one HikRobot/MVS frame at a time and stream packed BGR8 packets to WSL."""

import argparse
import ctypes
import ipaddress
import os
import socket
import struct
import sys
from pathlib import Path

import cv2
import numpy as np

_DLL_DIRECTORY_HANDLES = []


def load_mvs(root: Path):
    import_dir = root / "Samples" / "Python" / "MvImport"
    if not import_dir.is_dir():
        raise RuntimeError(f"MVS Python bindings not found: {import_dir}")
    sys.path.insert(0, str(import_dir))

    common = Path(os.environ.get("CommonProgramFiles(x86)", r"C:\Program Files (x86)\Common Files"))
    dll_dir = common / "MVS" / "Runtime" / "Win64_x64"
    if dll_dir.is_dir() and hasattr(os, "add_dll_directory"):
        _DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(str(dll_dir)))

    from PixelType_header import PixelType_Gvsp_BGR8_Packed  # noqa: E402
    from CameraParams_header import MV_CC_DEVICE_INFO, MV_CC_DEVICE_INFO_LIST, MV_CC_PIXEL_CONVERT_PARAM_EX, MV_FRAME_OUT  # noqa: E402
    from MvCameraControl_class import MvCamera, MV_ACCESS_Exclusive, MV_USB_DEVICE  # noqa: E402

    return (MvCamera, MV_CC_DEVICE_INFO, MV_CC_DEVICE_INFO_LIST,
            MV_CC_PIXEL_CONVERT_PARAM_EX, MV_FRAME_OUT, MV_ACCESS_Exclusive,
            MV_USB_DEVICE, PixelType_Gvsp_BGR8_Packed)


def require_ok(ret, operation):
    if ret != 0:
        raise RuntimeError(f"{operation} failed with MVS error 0x{ret:x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mvs-root", default=os.environ.get("MVCAM_COMMON_RUNENV", r"D:\MVS\Development"))
    parser.add_argument("--host", default="0.0.0.0", help="WSL-reachable listener address")
    parser.add_argument("--port", type=int, default=5001)
    parser.add_argument("--allow-peer", required=True, help="Only accept this WSL client IP")
    parser.add_argument("--vid", type=lambda x: int(x, 16), default=0x2BDF)
    parser.add_argument("--pid", type=lambda x: int(x, 16), default=0x0001)
    parser.add_argument("--exposure-us", type=float, default=2000.0)
    parser.add_argument("--gain", type=float, default=16.0)
    parser.add_argument("--jpeg-quality", type=int, default=85)
    args = parser.parse_args()
    if not 1 <= args.jpeg_quality <= 100:
        parser.error("--jpeg-quality must be between 1 and 100")
    try:
        allowed_peer = str(ipaddress.ip_address(args.allow_peer))
    except ValueError as exc:
        parser.error(f"invalid --allow-peer IP address: {exc}")

    (MvCamera, MV_CC_DEVICE_INFO, MV_CC_DEVICE_INFO_LIST, MV_CC_PIXEL_CONVERT_PARAM_EX,
     MV_FRAME_OUT, MV_ACCESS_Exclusive, MV_USB_DEVICE, PixelType_Gvsp_BGR8_Packed) = load_mvs(Path(args.mvs_root))

    camera = None
    opened = False
    grabbing = False
    try:
        require_ok(MvCamera.MV_CC_Initialize(), "initialize MVS SDK")
        devices = MV_CC_DEVICE_INFO_LIST()
        require_ok(MvCamera.MV_CC_EnumDevices(MV_USB_DEVICE, devices), "enumerate USB cameras")
        selected = None
        for index in range(devices.nDeviceNum):
            info = ctypes.cast(devices.pDeviceInfo[index], ctypes.POINTER(MV_CC_DEVICE_INFO)).contents
            usb = info.SpecialInfo.stUsb3VInfo
            if info.nTLayerType == MV_USB_DEVICE and usb.idVendor == args.vid and usb.idProduct == args.pid:
                selected = info
                break
        if selected is None:
            raise RuntimeError(f"MVS camera {args.vid:04x}:{args.pid:04x} was not found")

        camera = MvCamera()
        require_ok(camera.MV_CC_CreateHandle(selected), "create camera handle")
        require_ok(camera.MV_CC_OpenDevice(MV_ACCESS_Exclusive, 0), "open camera")
        opened = True
        require_ok(camera.MV_CC_SetEnumValue("TriggerMode", 0), "disable trigger mode")
        camera.MV_CC_SetEnumValue("ExposureAuto", 0)
        camera.MV_CC_SetEnumValue("GainAuto", 0)
        camera.MV_CC_SetFloatValue("ExposureTime", args.exposure_us)
        camera.MV_CC_SetFloatValue("Gain", args.gain)
        require_ok(camera.MV_CC_StartGrabbing(), "start camera stream")
        grabbing = True

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((args.host, args.port))
            server.listen(1)
            print(f"MVS bridge ready on {args.host}:{args.port}; waiting for WSL client")
            while True:
                client, peer = server.accept()
                if peer[0] != allowed_peer:
                    print(f"rejected client {peer[0]} (allowed: {allowed_peer})")
                    client.close()
                    continue
                print(f"WSL client connected: {peer}")
                with client:
                    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    client.settimeout(3.0)
                    frames = 0
                    while True:
                        frame = MV_FRAME_OUT()
                        ctypes.memset(ctypes.byref(frame), 0, ctypes.sizeof(frame))
                        ret = camera.MV_CC_GetImageBuffer(frame, 1000)
                        if ret != 0 or not frame.pBufAddr:
                            print(f"camera frame timeout/error 0x{ret:x}; waiting for next frame")
                            continue
                        try:
                            width = int(frame.stFrameInfo.nWidth)
                            height = int(frame.stFrameInfo.nHeight)
                            if width <= 0 or height <= 0 or width > 8192 or height > 8192:
                                raise RuntimeError(f"invalid camera frame size {width}x{height}")
                            capacity = width * height * 3
                            converted = MV_CC_PIXEL_CONVERT_PARAM_EX()
                            ctypes.memset(ctypes.byref(converted), 0, ctypes.sizeof(converted))
                            converted.nWidth = width
                            converted.nHeight = height
                            converted.pSrcData = frame.pBufAddr
                            converted.nSrcDataLen = frame.stFrameInfo.nFrameLen
                            converted.enSrcPixelType = frame.stFrameInfo.enPixelType
                            converted.enDstPixelType = PixelType_Gvsp_BGR8_Packed
                            converted.pDstBuffer = (ctypes.c_ubyte * capacity)()
                            converted.nDstBufferSize = capacity
                            require_ok(camera.MV_CC_ConvertPixelTypeEx(converted), "convert camera frame to BGR8")
                            payload_size = int(converted.nDstLen)
                            if payload_size != capacity:
                                raise RuntimeError(f"unexpected converted frame size {payload_size} (expected {capacity})")
                            bgr = np.frombuffer(
                                ctypes.string_at(converted.pDstBuffer, payload_size), dtype=np.uint8
                            ).reshape(height, width, 3)
                            encoded_ok, encoded = cv2.imencode(
                                ".jpg", bgr, [int(cv2.IMWRITE_JPEG_QUALITY), args.jpeg_quality]
                            )
                            if not encoded_ok:
                                raise RuntimeError("OpenCV JPEG encoding failed")
                            payload = encoded.tobytes()
                            # SPJ1 header: magic + network-order width, height, and JPEG payload length.
                            client.sendall(struct.pack("!4sIII", b"SPJ1", width, height, len(payload)))
                            client.sendall(payload)
                            frames += 1
                            if frames == 1 or frames % 300 == 0:
                                print(
                                    f"sent {frames} frames ({width}x{height}, "
                                    f"{payload_size} raw -> {len(payload)} JPEG bytes)"
                                )
                        except (ConnectionError, OSError, TimeoutError):
                            print("WSL client disconnected")
                            break
                        finally:
                            camera.MV_CC_FreeImageBuffer(frame)
    except KeyboardInterrupt:
        print("stopping MVS bridge")
    finally:
        if camera is not None:
            if grabbing:
                camera.MV_CC_StopGrabbing()
            if opened:
                camera.MV_CC_CloseDevice()
            camera.MV_CC_DestroyHandle()
        MvCamera.MV_CC_Finalize()


if __name__ == "__main__":
    main()
