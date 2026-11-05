"""Host-only tests for Android TLOU title/update safety; no game dump needed."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / "tlou_bionic.py"
spec = importlib.util.spec_from_file_location("tlou_bionic", MODULE)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def make_sfo(path, title="CUSA00552", version="01.11"):
    entries = (("TITLE_ID", title), ("APP_VER", version))
    keys = b"".join(k.encode() + b"\0" for k, _ in entries)
    vals = b"".join(v.encode() + b"\0" for _, v in entries)
    kstart = 20 + len(entries) * 16
    vstart = kstart + len(keys)
    buf = bytearray(b"\0PSF" + struct.pack("<4I", 0x101, kstart, vstart, len(entries)))
    ko = vo = 0
    for k, v in entries:
        buf.extend(struct.pack("<HHIII", ko, 0x0204, len(v)+1, len(v)+1, vo))
        ko += len(k)+1
        vo += len(v)+1
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(buf + keys + vals)


def make_game(root, *, title="CUSA00552", ver="01.11", include_libc=True):
    root.mkdir(parents=True, exist_ok=True)
    (root / "eboot.bin").write_bytes(b"test game")
    make_sfo(root / "sce_sys/param.sfo", title, ver)
    if include_libc:
        (root / "sce_module").mkdir(exist_ok=True)
        (root / "sce_module/libc.prx").write_bytes(b"libc")
    return root


class TestTLOUBionic(unittest.TestCase):
    def test_title_and_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = make_game(Path(tmp)/"game")
            self.assertEqual(m.require_game(base)["APP_VER"], "01.11")
            other = make_game(Path(tmp)/"other", title="CUSA01715")
            with self.assertRaisesRegex(m.PortError, "Wrong PS4 title"):
                m.require_game(other)

    def test_update_overlay_never_mutates_game(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            base = make_game(t/"game", ver="01.00")
            update = make_game(t/"patch", ver="01.11", include_libc=False)
            (update / "eboot.bin").write_bytes(b"update eboot")
            (base / "dvdroot_ps4").mkdir()
            (base / "dvdroot_ps4/resource.bin").write_bytes(b"resource")
            view = m.build_view(base, update, t/"work")
            self.assertEqual((view/"eboot.bin").read_bytes(), b"update eboot")
            self.assertEqual((view/"sce_module/libc.prx").read_bytes(), b"libc")
            self.assertEqual((view/"dvdroot_ps4/resource.bin").read_bytes(), b"resource")
            self.assertEqual((base/"eboot.bin").read_bytes(), b"test game")
            self.assertTrue((view/"eboot.bin").is_symlink())
            self.assertTrue((view/"dvdroot_ps4").is_symlink())

    def test_invalid_sfo_and_missing_libc(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = make_game(Path(tmp)/"game", include_libc=False)
            with self.assertRaisesRegex(m.PortError, "Missing sce_module/libc.prx"):
                m.require_game(base)
            (base/"sce_sys/param.sfo").write_bytes(b"garbage")
            with self.assertRaises(m.PortError):
                m.read_sfo(base/"sce_sys/param.sfo")

    def test_output_must_not_be_inside_game(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = make_game(Path(tmp)/"game")
            with self.assertRaisesRegex(m.PortError, "outside the game"):
                m.prepare(Path(tmp), base, None, base / "out")

    def test_existing_signature_gates_patches(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(__file__).resolve().parents[2]
            if not (source / "tools/probe_tlou.py").is_file():
                self.skipTest("full repository not mounted in isolated module test")
            patcher = m.load_patch_module(source)
            elf = bytearray(0x300)
            struct.pack_into("<Q", elf, 0x20, 0x40)
            struct.pack_into("<HH", elf, 0x36, 56, 1)
            struct.pack_into("<IIQQQQQQ", elf, 0x40, 1, 5, 0x200, 0x1000, 0, 0x20, 0x20, 0x1000)
            elf[0x200:0x202] = bytes.fromhex("ABCD")
            input_path = Path(tmp) / "eboot.elf"
            output_path = Path(tmp) / "patches.bin"
            input_path.write_bytes(elf)
            patches = {"good": [(0x1000, "ABCD", "EF12")], "bad": [(0x1002, "DEAD", "BEAD")]}
            result = patcher.image_patches(input_path, output_path, patches)
            self.assertEqual(result, output_path)
            raw = output_path.read_bytes()
            self.assertEqual(raw[:8], b"G3PATCH2")
            self.assertEqual(struct.unpack_from("<Q", raw, 16)[0], 1)
            self.assertEqual(struct.unpack_from("<QQ", raw, 24), (0x1000, 2))
            self.assertEqual(raw[40:], bytes.fromhex("EF12"))

    def test_update_resources_are_never_silently_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = make_game(Path(tmp)/"game")
            update = make_game(Path(tmp)/"patch", include_libc=False)
            (update/"dvdroot_ps4").mkdir()
            with self.assertRaisesRegex(m.PortError, "native app0 overlay"):
                m.prepare(Path(tmp), base, update, Path(tmp)/"out")


if __name__ == "__main__":
    unittest.main()
