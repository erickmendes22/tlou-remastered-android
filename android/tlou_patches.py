"""Verified CUSA00552 executable profiles; every change still requires original bytes.

The eboot with entry 0x56d9f0 was supplied by the user on 2026-11-03.
Its profile is selected by complete SELF SHA-256, not an assumed APP_VER.
No player binary or game content is included here.
"""
import hashlib
import struct

BASE_SELF_SHA256 = "e5f0aa322f60bd90637cc10d83de08c77885b1921084021c630e04ed3d6febf7"


def rip_instruction(prefix, address, target):
    return (bytes.fromhex(prefix) + struct.pack("<i", target - address - 8)).hex()


BASE_PATCHES = {
    # Missing third component-transform source: the observed crash dereferences
    # NULL+0x20 at 0xf6beb0. Return before the prologue only for absent source;
    # valid input executes the original displaced prologue and original body.
    "null-component-transforms": [
        (0xF6BE63, "90"*13, "4885c974fa554889e54157eb06"),
        (0xF6BE70, "554889e54157", "e9eeffffff90"),
        (0xF6BE54, "4883c4185b415c415d415e415f5dc3", None),
        (0xF6BE76, "415641554154534883ec184889cb4989d6", None),
        (0xF6BEB0, "8b3383feff4c89f8", None),
        (0x92C942, "488b5330488b8a800000004883c260", None),
    ],
    # WaitOnContextDone's compare and diagnostic both read 20 seconds rather than
    # 2. Reuse existing immutable constants; do not alter a shared constant pool.
    "frame-timeout": [
        (0x99285A, "c5f82e05b6e47900", rip_instruction("c5f82e05", 0x99285A, 0xEFDD88)),
        (0x992907, "c5fb100511e47900", rip_instruction("c5fb1005", 0x992907, 0x107CB00)),
        (0xEFDD88, "0000a041", None),
        (0x107CB00, "0000000000003440", None),
        (0x992862, "775a", None),  # retains the real timeout failure branch
    ],
    # Worker spin -> existing condition-variable wait after 1000 iterations;
    # keep the submit path broadcasting that same condition variable.
    "job-sleep": [
        (0xAACA3A, "7504", "eb04"),
        (0xAACA65, "7422", "9090"),
        (0xAABF91, "742d", "9090"),
        (0xAACA36, "807f5900", None),
        (0xAACA40, "4981f8e8030000", None),
        (0xAACA61, "80785900", None),
        (0xAACA75, "ba0a000000", None),
        (0xAABF8D, "807f5900", None),
        (0xAABFAF, "e86ca6ffff", None),
    ],
}


def select(eboot, fallback):
    digest = hashlib.sha256(eboot.read_bytes()).hexdigest()
    if digest == BASE_SELF_SHA256:
        print("Perfil TLOU verificado: entrada 0x56d9f0; patches próprios do executável.", flush=True)
        return dict(BASE_PATCHES)
    return fallback

# Adapted from Bachata S4+ P36 performance XMLs. Addresses are ELF-relative
# (Bachata guest base 0x400000 removed). Entire instruction guards were checked
# against the supplied SELF. Effect toggles below retain their full instruction guards.
PERFORMANCE_EFFECTS = {'ssao': [[9724771, '01', '00'], [9724765, 'c7834b01000001010101', None]],
 'motion_blur': [[9724898, '01', '00'],
                 [9725988, '01', '00'],
                 [9724892, 'c6836d01000001', None],
                 [9725982, 'c6830e04000001', None]],
 'depth_of_field': [[9724926, '01', '00'], [9724920, 'c6837101000001', None]],
 'bloom': [[9725092, 'c783b401000001010101', 'c783b401000001000101']],
 'sunlight_shadows': [[9370914, '0f8583030000', 'e98403000090'],
                      [9370900, '488d05d595fa0080b89c000000000f8583030000', None]],
 'subsurface': [[9381008, '0f84320b0000', 'e9330b000090'],
                [9380998, '488b0080b8942e0000000f84320b0000', None]]}
# Complete 32-write profiles from Bachata S4+ 0.2.7:
# patches/bachata-official/TheLastOfUsRemastered.xml (CUSA00556).
# XML addresses minus 0x400000; every original span also matches our CUSA00552 SELF.
# Includes custom setter, saved-config fallback, scene consumers, native output
# and movie viewport/AA separation. The shorter CUSA00552_Performance XML is incomplete.
PERFORMANCE_RESOLUTIONS = {
    "720": [
        (0x910740, "c5f828d889770889570cc747140000000089771889571c4189f0c4c1fa2ac089d2c4e1fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee024489c0c1e80344894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e946530e00660f1f440000", "be00050000bad0020000c5f828d889770889570cc747140000000089771889571c4189f0c4c17a2ac0c5fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee02b8a000000044894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e940530e00"),
        (0x9104f2, "83fe030f848400000083fe020f85f6000000c7470800040000c7470c58020000c7471400000000c7471800040000c7471c58020000", "89d1be00050000bad0020000b80000803fc5f96ec0e934020000909090909090909090909090909090909090909090909090909090"),
        (0x9103bb, "80070000", "00050000"),
        (0x9103c2, "38040000", "d0020000"),
        (0x9103d0, "80070000", "00050000"),
        (0x9103d7, "38040000", "d0020000"),
        (0x9103e5, "80070000", "00050000"),
        (0x9103ec, "38040000", "d0020000"),
        (0x910401, "f0000000", "a0000000"),
        (0x910408, "0e010000", "b4000000"),
        (0x91042b, "80070000", "00050000"),
        (0x910432, "38040000", "d0020000"),
        (0x910b22, "80070000", "00050000"),
        (0x910b29, "38040000", "d0020000"),
        (0x910b3e, "80070000", "00050000"),
        (0x910b45, "38040000", "d0020000"),
        (0x910b53, "80070000", "00050000"),
        (0x910b5a, "38040000", "d0020000"),
        (0x910b6f, "f0000000", "a0000000"),
        (0x910b76, "0e010000", "b4000000"),
        (0x910b99, "80070000", "00050000"),
        (0x910ba0, "38040000", "d0020000"),
        (0x91adbc, "488d054dfcef008b5808895d8c448b600c44896588", "bb00050000895d8c41bcd002000044896588909090"),
        (0x92ef1a, "c78500ffffff11000000c744241001000000c744240800000000c70424070000004c89f64489e14189d841b93d000000e8c1d30900", "6a118f8500ffffff6a018f4424106a008f4424086a078f04244c89f6b98007000041b8380400006a3d4159e8c6d309009090909090"),
        (0x92ef7d, "c785f8feffff11000000c744241001000000c744240800000000c7042407000000488d95f8feffff4c89ee4489e14589f041b931000000e857d30900", "6a118f85f8feffff6a018f4424106a008f4424086a078f0424488d95f8feffff4c89eeb9c003000041b81c0200006a314159e85cd309009090909090"),
        (0x92efc0, "c785f0feffff11000000c744241001000000c744240800000000c7042407000000488d95f0feffff4c89ee89d94c8b85d0fcffff41b93d000000e811d30900", "6a118f85f0feffff6a018f4424106a008f4424086a078f0424488d95f0feffff4c89eeb98007000041b8380400006a3d4159e819d309009090909090909090"),
        (0x92f00d, "c785e8feffff11000000c744241001000000c744240800000000c70424070000004c89ee4489e14589f041b931000000e8ced20900", "6a118f85e8feffff6a018f4424106a008f4424086a078f04244c89eeb9c003000041b81c0200006a314159e8d3d209009090909090"),
        (0x91f6e8, "488d0521b3ef00448b7824448b6028", "41bf8007000041bc38040000909090"),
        (0x910580, "470890040000c7470c91020000c747", "be80070000ba38040000e9610e0800"),
        (0x8e441c, "e8cfcf0a00", "e85fc10200"),
        (0x91a37a, "488d058f06f0008b4824c4e1fa2ad18b4028c4e1fa2ad8", "b980070000c4e1fa2ad1b838040000c4e1fa2ad8909090"),
        (0x9d4023, "8b7228c4e1fa2ac6c5fa100da96c7600c5f828d1c5ea5ec08b5224c4e1fa2acac5ea5ec9", "bed6b9723ac5f96ec6ba8988083ac5f96ecac5fa10159f6c760090909090909090909090"),
    ],
    "540": [
        (0x910740, "c5f828d889770889570cc747140000000089771889571c4189f0c4c1fa2ac089d2c4e1fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee024489c0c1e80344894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e946530e00660f1f440000", "bec0030000ba1c020000c5f828d889770889570cc747140000000089771889571c4189f0c4c17a2ac0c5fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee02b87800000044894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e940530e00"),
        (0x9104f2, "83fe030f848400000083fe020f85f6000000c7470800040000c7470c58020000c7471400000000c7471800040000c7471c58020000", "89d1bec0030000ba1c020000b80000803fc5f96ec0e934020000909090909090909090909090909090909090909090909090909090"),
        (0x9103bb, "80070000", "c0030000"),
        (0x9103c2, "38040000", "1c020000"),
        (0x9103d0, "80070000", "c0030000"),
        (0x9103d7, "38040000", "1c020000"),
        (0x9103e5, "80070000", "c0030000"),
        (0x9103ec, "38040000", "1c020000"),
        (0x910401, "f0000000", "78000000"),
        (0x910408, "0e010000", "87000000"),
        (0x91042b, "80070000", "c0030000"),
        (0x910432, "38040000", "1c020000"),
        (0x910b22, "80070000", "c0030000"),
        (0x910b29, "38040000", "1c020000"),
        (0x910b3e, "80070000", "c0030000"),
        (0x910b45, "38040000", "1c020000"),
        (0x910b53, "80070000", "c0030000"),
        (0x910b5a, "38040000", "1c020000"),
        (0x910b6f, "f0000000", "78000000"),
        (0x910b76, "0e010000", "87000000"),
        (0x910b99, "80070000", "c0030000"),
        (0x910ba0, "38040000", "1c020000"),
        (0x91adbc, "488d054dfcef008b5808895d8c448b600c44896588", "bbc0030000895d8c41bc1c02000044896588909090"),
        (0x92ef1a, "c78500ffffff11000000c744241001000000c744240800000000c70424070000004c89f64489e14189d841b93d000000e8c1d30900", "6a118f8500ffffff6a018f4424106a008f4424086a078f04244c89f6b98007000041b8380400006a3d4159e8c6d309009090909090"),
        (0x92ef7d, "c785f8feffff11000000c744241001000000c744240800000000c7042407000000488d95f8feffff4c89ee4489e14589f041b931000000e857d30900", "6a118f85f8feffff6a018f4424106a008f4424086a078f0424488d95f8feffff4c89eeb9c003000041b81c0200006a314159e85cd309009090909090"),
        (0x92efc0, "c785f0feffff11000000c744241001000000c744240800000000c7042407000000488d95f0feffff4c89ee89d94c8b85d0fcffff41b93d000000e811d30900", "6a118f85f0feffff6a018f4424106a008f4424086a078f0424488d95f0feffff4c89eeb98007000041b8380400006a3d4159e819d309009090909090909090"),
        (0x92f00d, "c785e8feffff11000000c744241001000000c744240800000000c70424070000004c89ee4489e14589f041b931000000e8ced20900", "6a118f85e8feffff6a018f4424106a008f4424086a078f04244c89eeb9c003000041b81c0200006a314159e8d3d209009090909090"),
        (0x91f6e8, "488d0521b3ef00448b7824448b6028", "41bf8007000041bc38040000909090"),
        (0x910580, "470890040000c7470c91020000c747", "be80070000ba38040000e9610e0800"),
        (0x8e441c, "e8cfcf0a00", "e85fc10200"),
        (0x91a37a, "488d058f06f0008b4824c4e1fa2ad18b4028c4e1fa2ad8", "b980070000c4e1fa2ad1b838040000c4e1fa2ad8909090"),
        (0x9d4023, "8b7228c4e1fa2ac6c5fa100da96c7600c5f828d1c5ea5ec08b5224c4e1fa2acac5ea5ec9", "bed6b9723ac5f96ec6ba8988083ac5f96ecac5fa10159f6c760090909090909090909090"),
    ],
    "360": [
        (0x910740, "c5f828d889770889570cc747140000000089771889571c4189f0c4c1fa2ac089d2c4e1fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee024489c0c1e80344894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e946530e00660f1f440000", "be80020000ba68010000c5f828d889770889570cc747140000000089771889571c4189f0c4c17a2ac0c5fa2acac5f259d3c5fa115f20c5ea5ed089d6c1ee02b85000000044894724895728c5fa115f2cc5fa115710894740897744c7476800007042c7476c000000004489473889573c894f74c7475032000000c7475416000000c74758cd040000c7475cb9020000c747489c040000c7474ca4020000c74760ce040000c74764ba020000e940530e00"),
        (0x9104f2, "83fe030f848400000083fe020f85f6000000c7470800040000c7470c58020000c7471400000000c7471800040000c7471c58020000", "89d1be80020000ba68010000b80000803fc5f96ec0e934020000909090909090909090909090909090909090909090909090909090"),
        (0x9103bb, "80070000", "80020000"),
        (0x9103c2, "38040000", "68010000"),
        (0x9103d0, "80070000", "80020000"),
        (0x9103d7, "38040000", "68010000"),
        (0x9103e5, "80070000", "80020000"),
        (0x9103ec, "38040000", "68010000"),
        (0x910401, "f0000000", "50000000"),
        (0x910408, "0e010000", "5a000000"),
        (0x91042b, "80070000", "80020000"),
        (0x910432, "38040000", "68010000"),
        (0x910b22, "80070000", "80020000"),
        (0x910b29, "38040000", "68010000"),
        (0x910b3e, "80070000", "80020000"),
        (0x910b45, "38040000", "68010000"),
        (0x910b53, "80070000", "80020000"),
        (0x910b5a, "38040000", "68010000"),
        (0x910b6f, "f0000000", "50000000"),
        (0x910b76, "0e010000", "5a000000"),
        (0x910b99, "80070000", "80020000"),
        (0x910ba0, "38040000", "68010000"),
        (0x91adbc, "488d054dfcef008b5808895d8c448b600c44896588", "bb80020000895d8c41bc6801000044896588909090"),
        (0x92ef1a, "c78500ffffff11000000c744241001000000c744240800000000c70424070000004c89f64489e14189d841b93d000000e8c1d30900", "6a118f8500ffffff6a018f4424106a008f4424086a078f04244c89f6b98007000041b8380400006a3d4159e8c6d309009090909090"),
        (0x92ef7d, "c785f8feffff11000000c744241001000000c744240800000000c7042407000000488d95f8feffff4c89ee4489e14589f041b931000000e857d30900", "6a118f85f8feffff6a018f4424106a008f4424086a078f0424488d95f8feffff4c89eeb9c003000041b81c0200006a314159e85cd309009090909090"),
        (0x92efc0, "c785f0feffff11000000c744241001000000c744240800000000c7042407000000488d95f0feffff4c89ee89d94c8b85d0fcffff41b93d000000e811d30900", "6a118f85f0feffff6a018f4424106a008f4424086a078f0424488d95f0feffff4c89eeb98007000041b8380400006a3d4159e819d309009090909090909090"),
        (0x92f00d, "c785e8feffff11000000c744241001000000c744240800000000c70424070000004c89ee4489e14589f041b931000000e8ced20900", "6a118f85e8feffff6a018f4424106a008f4424086a078f04244c89eeb9c003000041b81c0200006a314159e8d3d209009090909090"),
        (0x91f6e8, "488d0521b3ef00448b7824448b6028", "41bf8007000041bc38040000909090"),
        (0x910580, "470890040000c7470c91020000c747", "be80070000ba38040000e9610e0800"),
        (0x8e441c, "e8cfcf0a00", "e85fc10200"),
        (0x91a37a, "488d058f06f0008b4824c4e1fa2ad18b4028c4e1fa2ad8", "b980070000c4e1fa2ad1b838040000c4e1fa2ad8909090"),
        (0x9d4023, "8b7228c4e1fa2ac6c5fa100da96c7600c5f828d1c5ea5ec08b5224c4e1fa2acac5ea5ec9", "bed6b9723ac5f96ec6ba8988083ac5f96ecac5fa10159f6c760090909090909090909090"),
    ],
}


def selected_options(options):
    options = options or {}
    if not isinstance(options, dict):
        raise ValueError("Configuração de patches inválida")
    resolution = options.get("resolution", "original")
    if not isinstance(resolution, str) or resolution not in {"original", "720", "540", "360"}:
        raise ValueError("Resolução não suportada")
    selected = {}
    if resolution != "original":
        selected["resolution-" + resolution] = PERFORMANCE_RESOLUTIONS[resolution]
    for name, changes in PERFORMANCE_EFFECTS.items():
        if type(options.get(name, False)) is not bool:
            raise ValueError("Opção de patch inválida: " + name)
        if options.get(name, False):
            selected["disable-" + name] = changes
    return selected


def write_selected(source, prepared, options):
    """Atomically publish launch patches; never change the player's SELF or linked image."""
    import importlib.util
    import json
    from pathlib import Path
    prepared, source = Path(prepared), Path(source)
    requested = selected_options(options)
    game_self = prepared / "tlou-view/eboot.bin"
    if not game_self.is_file():
        if requested:
            raise ValueError("Esta pasta preparada não contém o eboot original para validar os patches")
        return
    if hashlib.sha256(game_self.read_bytes()).hexdigest() != BASE_SELF_SHA256:
        if requested:
            raise ValueError("Patches disponíveis apenas para o eboot CUSA00552 1.00 validado; restaure o perfil Original")
        return
    spec = importlib.util.spec_from_file_location("tlou_patch_writer", source / "tools/probe_tlou.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    elf_path = prepared / "eboot.elf"
    if not elf_path.is_file():
        # Imported prepared folders may contain only the linked image. Rebuild
        # the inspection ELF from the validated SELF without relinking the game.
        spec = importlib.util.spec_from_file_location("tlou_owned_self", source / "gow3_pc/scripts/prepare.py")
        inspector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(inspector)
        raw, _, _, _, _ = inspector.parse_self(game_self.read_bytes())
        inspection_temp = prepared / "eboot.elf.new"
        inspection_temp.write_bytes(raw)
        inspection_temp.replace(elf_path)
    elf = elf_path.read_bytes()
    phoff = struct.unpack_from("<Q", elf, 0x20)[0]
    size, count = struct.unpack_from("<HH", elf, 0x36)
    segments = [struct.unpack_from("<IIQQQQQQ", elf, phoff + i*size) for i in range(count)]
    def read(at, length):
        for kind, _, offset, start, _, filesz, _, _ in segments:
            if kind == 1 and start <= at and at + length <= start + filesz:
                return elf[offset + at-start:offset + at-start+length]
        return b""
    patches = dict(BASE_PATCHES)
    patches.update(requested)
    for name, changes in patches.items():
        for at, expected, _ in changes:
            if read(at, len(bytes.fromhex(expected))) != bytes.fromhex(expected):
                raise ValueError("Assinatura divergente no patch " + name + " em " + hex(at))
    temp = prepared / "patches.bin.new"
    module.image_patches(elf_path, temp, patches)
    temp.replace(prepared / "patches.bin")
    report = {"profile": BASE_SELF_SHA256, "selected": list(requested),
              "resolution": (options or {}).get("resolution", "original"), "validated": True}
    (prepared / "patch-validation.json").write_text(json.dumps(report, indent=2)+"\n")
    print("Patches validados: " + (", ".join(requested) or "Original; correções de estabilidade"), flush=True)
