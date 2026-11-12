import asyncio
import copy
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("contact", Path(__file__).with_name("ios-idb-contact.py"))
contact = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contact)


class Event:
    DOWN, UP = 0, 1
    def __new__(cls, **fields): return fields
    @staticmethod
    def HIDPressAction(**fields): return fields
    @staticmethod
    def HIDTouch(**fields): return fields
    @staticmethod
    def HIDPress(**fields): return fields
    @staticmethod
    def HIDDelay(**fields): return fields


class PB:
    HIDEvent = Event
    @staticmethod
    def Point(**fields): return fields


class Stream:
    def __init__(self, fail_at=None): self.calls, self.fail_at = [], fail_at
    async def __aenter__(self): return self
    async def __aexit__(self, *_): return False
    async def send_request(self, **fields): self.calls.append(("headers", fields))
    async def send_message(self, event):
        self.calls.append(("message", event))
        if self.fail_at == len(self.calls): raise RuntimeError("delivery unavailable")
    async def end(self): self.calls.append(("end", None))
    async def recv_message(self): self.calls.append(("receive", None)); return object()
    def open(self, **_): return self


class Stub:
    def __init__(self, stream): self.hid = stream


class ContactTests(unittest.TestCase):
    def setUp(self):
        self.context = dict(version=1, deviceUDID="00000000-0000-0000-0000-000000000001",
                            appID="example.fixture", pid=12345, nonce="a"*64,
                            stdout="/private/tmp/owned.stdout", stdoutDevice="1", stdoutInode="2",
                            screen=dict(widthPoints=390, heightPoints=844,
                                        widthPixels=1170, heightPixels=2532, density=3))
        self.request = dict(version=1, nonce="a"*64, pid=12345,
                            stdout="/private/tmp/owned.stdout", stdoutDevice="1", stdoutInode="2",
                            requestID="00000000-0000-0000-0000-000000000002",
                            test="testFilesExportBackup", controlID="start", generation=1,
                            point=dict(x=191, y=572.5000305), duration=0.20, orientation="portrait",
                            appFrame=dict(x=0, y=0, width=390, height=844),
                            createdAt=100000, expiresAt=145000)

    def check(self, request=None, context=None, now=100001):
        request = request or self.request
        raw = json.dumps(request).encode()
        return contact.validate_request(raw, raw, request, context or self.context, now)

    def test_original_fractional_points_and_both_durations(self):
        point, duration = self.check()
        self.assertEqual(point["y"], 572.5000305)
        events = contact.events_for(self.request, PB)
        self.assertEqual(len(events), 3)
        self.assertEqual(events[0]["press"]["action"]["touch"]["point"], point)
        self.assertEqual(events[2]["press"]["action"]["touch"]["point"], point)
        self.assertEqual(events[0]["press"]["direction"], 0)
        self.assertEqual(events[1], {"delay": {"duration": duration}})
        self.assertEqual(events[2]["press"]["direction"], 1)
        self.request.update(test="testFilesImportBackupAfterRelaunch", controlID="load", duration=0.05)
        self.request["point"]["y"] = 658.7
        self.assertEqual(self.check()[1], 0.05)
        self.assertEqual(contact.events_for(self.request, PB)[1]["delay"]["duration"], 0.05)

    @unittest.skipUnless(os.environ.get("RAISING_IDB_VALIDATE_PROTOBUF") == "1",
                         "Exact generated runtime is installed only in the disposable hosted venv")
    def test_exact_generated_wire_roundtrip(self):
        from idb.grpc import idb_pb2
        from idb.grpc.idb_grpc import CompanionServiceStub
        from grpclib.client import Channel
        events = contact.events_for(self.request, idb_pb2)
        decoded = [idb_pb2.HIDEvent.FromString(event.SerializeToString()) for event in events]
        self.assertEqual([event.WhichOneof("event") for event in decoded], ["press", "delay", "press"])
        self.assertEqual(decoded[0].press.action.touch.point.x, 191)
        self.assertEqual(decoded[0].press.action.touch.point.y, 572.5000305)
        self.assertEqual(decoded[2].press.action.touch.point.y, 572.5000305)
        self.assertEqual(decoded[1].delay.duration, 0.20)
        async def construct_only():
            channel = Channel(path="/private/tmp/never-connect-idb-wire-test")
            try:
                self.assertIsNotNone(CompanionServiceStub(channel).hid.open(timeout=1))
            finally: channel.close()
        asyncio.run(construct_only())

    def test_rejects_changed_identity_action_expiry_geometry(self):
        mutations = [
            ("nonce", "b"*64), ("pid", 222), ("version", 2),
            ("version", True), ("stdout", "/private/tmp/other.stdout"),
            ("stdoutDevice", "3"), ("stdoutInode", "4"),
            ("requestID", "-"*36), ("controlID", "load"), ("test", "other"),
            ("duration", 0.05), ("generation", 0), ("generation", True),
            ("orientation", "portraitUpsideDown"), ("expiresAt", 145001),
            ("createdAt", 100002), ("point", {"x": -1, "y": 30}),
            ("point", {"x": 390, "y": 30}), ("point", {"x": float("nan"), "y": 30}),
            ("appFrame", {"x": 0, "y": 0, "width": 844, "height": 390}),
            ("appFrame", {"x": 2, "y": 0, "width": 390, "height": 844}),
        ]
        for key, value in mutations:
            with self.subTest(key=key, value=value):
                request = copy.deepcopy(self.request); request[key] = value
                with self.assertRaises(contact.ContractError): self.check(request)
        with self.assertRaises(contact.ContractError): self.check(now=145000)
        raw = json.dumps(self.request).encode()
        with self.assertRaises(contact.ContractError):
            contact.validate_request(raw, raw+b" ", self.request, self.context, 100001)

    def test_rejects_invalid_screen_before_input(self):
        for key, value in [("density", 0), ("density", 2), ("widthPoints", 0),
                           ("widthPoints", 391), ("heightPixels", float("inf"))]:
            with self.subTest(key=key, value=value):
                context = copy.deepcopy(self.context); context["screen"][key] = value
                with self.assertRaises(contact.ContractError): self.check(context=context)

    def test_empty_stream_has_headers_and_zero_messages(self):
        stream = Stream()
        asyncio.run(contact.send_stream(Stub(stream), [], 15))
        self.assertEqual(stream.calls, [("headers", {"end": True}), ("receive", None)])

    def test_contact_sends_three_messages_once(self):
        stream = Stream()
        events = contact.events_for(self.request, PB)
        asyncio.run(contact.send_stream(Stub(stream), events, 1))
        self.assertEqual(stream.calls, [("message", e) for e in events]+[("end", None),("receive", None)])

    def test_failed_stream_is_not_retried(self):
        stream = Stream(fail_at=1)
        with self.assertRaises(RuntimeError):
            asyncio.run(contact.send_stream(Stub(stream), contact.events_for(self.request, PB), 1))
        self.assertEqual(len(stream.calls), 1)

    def test_exclusive_response_and_nofollow_bounded_read(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"response.json"
            contact.write_once(path, {"ok": True})
            self.assertEqual(contact.read_json(path)[1], {"ok": True})
            with self.assertRaises(FileExistsError): contact.write_once(path, {"ok": False})
            link = Path(directory)/"link"; link.symlink_to(path)
            with self.assertRaises(OSError): contact.read_json(link)
            fifo = Path(directory)/"fifo"; os.mkfifo(fifo)
            with self.assertRaises(contact.ContractError): contact.read_json(fifo)
            path.write_bytes(b"x"*(1024*1024+1))
            with self.assertRaises(contact.ContractError): contact.read_json(path)


if __name__ == "__main__": unittest.main()
