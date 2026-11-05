#!/usr/bin/env python3
"""On-device preparation with cache invalidation and atomic success publication."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
from tlou_bionic import prepare, require_game
from tlou_patches import write_selected

REVISION = 2


def fingerprint(game):
    require_game(game)
    digest = hashlib.sha256()
    digest.update(f"{REVISION}:{game.resolve()}".encode())
    paths = [game / "eboot.bin", game / "sce_sys/param.sfo"]
    paths += sorted((game / "sce_module").rglob("*.prx"))
    for path in paths:
        digest.update(str(path.relative_to(game)).encode())
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    return digest.hexdigest()


def ensure(source, game, out, options=None):
    game, out = game.resolve(), out.resolve()
    if out == game or out.is_relative_to(game) or game.is_relative_to(out):
        raise ValueError("Preparation directory must be separate from the game")
    key = fingerprint(game)
    marker = out / "device-ready.json"
    current = {}
    if marker.is_file():
        try:
            current = json.loads(marker.read_text())
        except (ValueError, OSError):
            pass
    if (current.get("fingerprint") == key and
            (out / "boot-linked.bin").is_file() and
            (out / "content.bin").is_file() and (out / "tlou-view").is_dir()):
        if options is not None:
            write_selected(source, out, options)
        print("Jogo já preparado; arquivos verificados.", flush=True)
        return out
    out.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".prepare-", dir=out.parent))
    backup = out.with_name(out.name + ".previous")
    try:
        print("Preparando executável, módulos e conteúdo do TLOU…", flush=True)
        prepare(source, game, None, stage)
        if options is not None:
            write_selected(source, stage, options)
        metadata = require_game(game)
        (stage / "device-ready.json").write_text(json.dumps({"fingerprint": key, "game": str(game),
            "title_id": metadata.get("TITLE_ID"), "version": metadata.get("APP_VER")}))
        if backup.exists():
            shutil.rmtree(backup)
        if out.exists():
            out.rename(backup)
        try:
            stage.rename(out)
        except OSError:
            if backup.exists():
                backup.rename(out)
            raise
        if backup.exists():
            shutil.rmtree(backup)
        print("Preparação concluída. Pronto para jogar.", flush=True)
        return out
    finally:
        if stage.exists():
            shutil.rmtree(stage)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--options", type=Path)
    parser.add_argument("--patch-only", action="store_true")
    args = parser.parse_args()
    options = json.loads(args.options.read_text()) if args.options else None
    source = Path(__file__).resolve().parents[1]
    if args.patch_only:
        write_selected(source, args.out, options)
    else:
        ensure(source, args.game, args.out, options)
