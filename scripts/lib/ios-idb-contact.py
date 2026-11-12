#!/usr/bin/env python3
"""Isolated HID experiment: one pinned idb stream; no GUI or input retry.

RPC success is transport completion, never evidence of guest touch delivery.
The XCTest consumer separately requires the real SDL DIRECT receipt.
"""
import argparse
import asyncio
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import time


class ContractError(Exception):
    pass


def require(value, message):
    if not value:
        raise ContractError(message)


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def read_json(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as stream:
        identity = os.fstat(stream.fileno())
        require(stat.S_ISREG(identity.st_mode), "Expected regular JSON file")
        raw = stream.read(1024 * 1024 + 1)
    current = os.lstat(path)
    require(stat.S_ISREG(current.st_mode) and
            (current.st_dev, current.st_ino) == (identity.st_dev, identity.st_ino), "JSON path replaced")
    require(len(raw) <= 1024 * 1024, "Oversized JSON evidence")
    return raw, json.loads(raw)


def write_once(path, value):
    # A response must never replace a previous result or admit a second attempt.
    data = (json.dumps(value, sort_keys=True, allow_nan=False) + "\n").encode()
    temporary = path.with_name(path.name + ".tmp-" + str(os.getpid()))
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
        os.link(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def validate_context(context, contact=False):
    require(type(context.get("version")) is int and context["version"] == 1, "Wrong context version")
    require(isinstance(context.get("deviceUDID"), str) and
            re.fullmatch(r"[A-Fa-f0-9]{8}(?:-[A-Fa-f0-9]{4}){3}-[A-Fa-f0-9]{12}", context["deviceUDID"]), "Missing owned UDID")
    if contact:
        require(type(context.get("pid")) is int and context["pid"] > 0, "Missing bound PID")
        require(isinstance(context.get("nonce"), str) and
                re.fullmatch(r"[a-f0-9]{64}", context["nonce"]), "Invalid input nonce")
        require(isinstance(context.get("appID"), str) and context["appID"], "Missing app ID")


def validate_screen(screen):
    require(all(number(screen.get(k)) and screen[k] > 0 for k in
                ("widthPoints", "heightPoints", "widthPixels", "heightPixels", "density")),
            "Missing independently described device geometry")
    require(abs(screen["widthPixels"] / screen["density"] - screen["widthPoints"]) < 1e-6 and
            abs(screen["heightPixels"] / screen["density"] - screen["heightPoints"]) < 1e-6 and
            screen["heightPoints"] > screen["widthPoints"], "Device pixel/portrait-point units disagree")


def validate_request(raw, admitted, request, context, now_ms):
    require(raw == admitted, "Request bytes differ from exclusive admission")
    validate_context(context, contact=True)
    require(type(request.get("version")) is int and request["version"] == 1 and
            request.get("nonce") == context["nonce"] and type(request.get("pid")) is int and
            request["pid"] == context["pid"], "Request launch identity mismatch")
    require(all(isinstance(context.get(key), str) and context[key] and
                request.get(key) == context[key] for key in ("stdout", "stdoutDevice", "stdoutInode")),
            "Request differs from the admitted stdout identity")
    require(isinstance(request.get("requestID"), str) and
            re.fullmatch(r"[A-Fa-f0-9]{8}(?:-[A-Fa-f0-9]{4}){3}-[A-Fa-f0-9]{12}", request["requestID"]), "Invalid request ID")
    expected = {"testFilesExportBackup": ("start", 0.20),
                "testFilesImportBackupAfterRelaunch": ("load", 0.05)}
    require(request.get("test") in expected, "Unexpected test case")
    control, duration = expected[request["test"]]
    require(request.get("controlID") == control and request.get("duration") == duration,
            "Admitted first-contact action or proxy duration changed")
    require(type(request.get("generation")) is int and request["generation"] > 0,
            "Invalid committed generation")
    created, expires = request.get("createdAt"), request.get("expiresAt")
    require(number(created) and number(expires) and 0 < expires - created <= 45000 and
            created <= now_ms < expires, "Expired or invalid request lifetime")
    point, frame, screen = request.get("point", {}), request.get("appFrame", {}), context.get("screen", {})
    require(all(number(point.get(k)) for k in ("x", "y")) and
            all(number(frame.get(k)) for k in ("x", "y", "width", "height")), "Invalid geometry")
    validate_screen(screen)
    require(request.get("orientation") == "portrait", "Original portrait orientation unproven")
    require(frame["x"] == 0 and frame["y"] == 0 and
            abs(frame["width"] - screen["widthPoints"]) <= 0.5 and
            abs(frame["height"] - screen["heightPoints"]) <= 0.5 and
            frame["height"] > frame["width"], "Expected original full-screen portrait geometry")
    require(0 < point["x"] < screen["widthPoints"] and
            0 < point["y"] < screen["heightPoints"], "Point would require clamping")
    return point, duration


def events_for(request, pb):
    point = request["point"]
    action = pb.HIDEvent.HIDPressAction(touch=pb.HIDEvent.HIDTouch(
        point=pb.Point(x=point["x"], y=point["y"])))
    return [pb.HIDEvent(press=pb.HIDEvent.HIDPress(action=action, direction=pb.HIDEvent.DOWN)),
            pb.HIDEvent(delay=pb.HIDEvent.HIDDelay(duration=request["duration"])),
            pb.HIDEvent(press=pb.HIDEvent.HIDPress(action=action, direction=pb.HIDEvent.UP))]


async def send_stream(stub, events, timeout):
    # An empty preflight is ZERO messages, not one default/empty HIDEvent.
    # All three contact messages are queued; the server applies the delay.
    async with stub.hid.open(timeout=timeout) as stream:
        if not events:
            await stream.send_request(end=True)
        else:
            for event in events:
                await stream.send_message(event)
            await stream.end()
        response = await stream.recv_message()
        require(response is not None, "HID RPC returned no completion")


async def execute(args, context):
    from grpclib.client import Channel
    from idb.grpc.idb_grpc import CompanionServiceStub
    from idb.grpc import idb_pb2 as pb

    socket = Path(args.socket)
    info = socket.lstat()
    require(stat.S_ISSOCK(info.st_mode), "Expected private companion socket")
    channel = Channel(path=str(socket))
    stub = CompanionServiceStub(channel)
    try:
        print("IDB_PHASE describe-owned-target", flush=True)
        target = await stub.describe(pb.TargetDescriptionRequest(fetch_diagnostics=False), timeout=10)
        require(target.companion.udid == context["deviceUDID"] and
                target.target_description.udid == context["deviceUDID"], "Companion targets another device")
        dimensions = target.target_description.screen_dimensions
        screen = dict(widthPoints=dimensions.width_points, heightPoints=dimensions.height_points,
                      widthPixels=dimensions.width, heightPixels=dimensions.height, density=dimensions.density)
        validate_screen(screen)
        if args.mode == "preflight":
            print("IDB_PHASE empty-hid-client-initialization", flush=True)
            await send_stream(stub, [], 15)
            return dict(version=1, ok=True, deviceUDID=context["deviceUDID"], screen=screen,
                        streamEvents=0, noInputPosted=True, rpcCompleted=True,
                        target=dict(name=target.target_description.name,
                                    state=target.target_description.state,
                                    osVersion=target.target_description.os_version,
                                    architecture=target.target_description.architecture),
                        guestRegistration="unobserved", guestDelivery="unobserved")

        require(screen == context.get("screen"), "Device geometry changed since preflight")
        request_path = Path(args.request)
        raw, request = read_json(request_path)
        admitted, _ = read_json(request_path.parent / "admission.json")
        point, duration = validate_request(raw, admitted, request, context, time.time() * 1000)
        os.kill(context["pid"], 0)
        print("IDB_PHASE admitted-contact-before-final-expiry-check", flush=True)
        write_once(request_path.parent / "consumed.json",
                   dict(version=1, requestID=request["requestID"], nonce=context["nonce"],
                        pid=context["pid"], deviceUDID=context["deviceUDID"],
                        requestSha256=hashlib.sha256(raw).hexdigest()))
        # Keep no stdout/file writes or host delays between the submitted events.
        # Source/driver admission and request revocation are checked immediately before RPC.
        current_raw, _ = read_json(request_path)
        require(current_raw == raw, "Request revoked or replaced")
        current_socket = socket.lstat()
        require(stat.S_ISSOCK(current_socket.st_mode) and
                (current_socket.st_dev, current_socket.st_ino) == (info.st_dev, info.st_ino),
                "Companion socket replaced")
        remaining = (request["expiresAt"] - time.time() * 1000) / 1000
        require(remaining > 0, "Request expired before submission")
        started = time.monotonic_ns()
        await send_stream(stub, events_for(request, pb), remaining)
        completed = time.monotonic_ns()
        require(time.time() * 1000 < request["expiresAt"], "RPC completed after request expiry")
        return dict(version=1, ok=True, backend="idb", nonce=context["nonce"],
                    requestID=request["requestID"], pid=context["pid"], deviceUDID=context["deviceUDID"],
                    point=point, duration=duration, streamEvents=3, rpcCompleted=True,
                    rpcStartedNS=str(started), rpcCompletedNS=str(completed),
                    guestDelivery="requires-native-proof")
    finally:
        channel.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("preflight", "contact"))
    parser.add_argument("--socket", required=True)
    parser.add_argument("--context", required=True)
    parser.add_argument("--request")
    parser.add_argument("--response", required=True)
    args = parser.parse_args()
    result = {"version": 1, "ok": False, "mode": args.mode, "guestDelivery": "unproven"}
    try:
        _, context = read_json(args.context)
        validate_context(context, contact=args.mode == "contact")
        if args.mode == "contact":
            require(args.request is not None, "Missing admitted request")
            require(Path(args.request).parent.resolve() == Path(args.response).parent.resolve(),
                    "Response is outside the request capability")
        result = asyncio.run(execute(args, context))
    except Exception as error:
        result["error"] = type(error).__name__ + ": " + str(error)
    write_once(Path(args.response), result)
    print(json.dumps({"mode": args.mode, "ok": result["ok"], "error": result.get("error")}), flush=True)
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
