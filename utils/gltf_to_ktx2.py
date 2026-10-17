#!/usr/bin/env python3
"""
gltf_to_ktx2.py

Converts every texture referenced by a .gltf file to uncompressed KTX2 with the
KTX-Software `ktx create` tool, then rewrites the glTF to reference the .ktx2 files
through KHR_texture_basisu.

`ktx create` is only asked to do two things: generate mipmaps and assign the
transfer function (sRGB for colour textures, linear for data textures). No
block compression (ETC1S/UASTC) and no supercompression (Zstd/ZLIB) is applied.

`ktx create` only accepts PNG, EXR or raw input, so every source image (JPEG,
PNG, WebP, data URIs, bufferView images) is first normalized with Pillow to a
plain 8-bit RGB/RGBA PNG.

Requirements
  - KTX-Software 4.3 or newer, `ktx` on PATH (or pass --ktx)
  - Pillow:  pip install pillow

Examples
  python gltf_to_ktx2.py scene.gltf
  python gltf_to_ktx2.py scene.gltf -o out/scene.gltf -j 4
  python gltf_to_ktx2.py scene.gltf --keep-fallback
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import math
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote, unquote

from PIL import Image, ImageOps

EXT = "KHR_texture_basisu"
LANCZOS = getattr(Image, "Resampling", Image).LANCZOS

# Material-extension texture slots that hold colour (sRGB) data.
# Every other texture slot in glTF holds linear data.
COLOUR_EXT_SLOTS = {
    "diffuseTexture",                   # KHR_materials_pbrSpecularGlossiness
    "specularGlossinessTexture",        # KHR_materials_pbrSpecularGlossiness
    "specularColorTexture",             # KHR_materials_specular
    "sheenColorTexture",                # KHR_materials_sheen
    "diffuseTransmissionColorTexture",  # KHR_materials_diffuse_transmission
}

GL_REPEAT, GL_MIRRORED_REPEAT = 10497, 33648


def warn(msg: str) -> None:
    print(f"warning: {msg}", file=sys.stderr)


# --------------------------------------------------------------------------- #
# ktx tool detection
# --------------------------------------------------------------------------- #
@dataclass
class KtxCaps:
    exe: str
    version: str
    assign_tf: str     # "--assign-tf" (4.4+) or "--assign-oetf" (4.3)


def detect_ktx(exe: str | None) -> KtxCaps:
    path = exe or shutil.which("ktx")
    if not path:
        sys.exit("error: 'ktx' not found on PATH. Install KTX-Software 4.3+ or pass --ktx.")
    ver = subprocess.run([path, "--version"], capture_output=True, text=True)
    hlp = subprocess.run([path, "create", "--help"], capture_output=True, text=True)
    help_text = hlp.stdout + hlp.stderr
    if "--format" not in help_text:
        sys.exit(f"error: '{path} create' does not look like KTX-Software 4.3+.")
    return KtxCaps(
        exe=path,
        version=(ver.stdout or ver.stderr).strip(),
        assign_tf="--assign-tf" if "--assign-tf" in help_text else "--assign-oetf",
    )


# --------------------------------------------------------------------------- #
# glTF inspection
# --------------------------------------------------------------------------- #
def classify_textures(gltf: dict) -> dict[int, str]:
    """Return texture index -> 'colour' | 'normal' | 'linear'."""
    usage: dict[int, set[str]] = {}

    def mark(info, kind):
        if isinstance(info, dict) and isinstance(info.get("index"), int):
            usage.setdefault(info["index"], set()).add(kind)

    for mat in gltf.get("materials", []):
        pbr = mat.get("pbrMetallicRoughness", {})
        mark(pbr.get("baseColorTexture"), "colour")
        mark(pbr.get("metallicRoughnessTexture"), "linear")
        mark(mat.get("normalTexture"), "normal")
        mark(mat.get("occlusionTexture"), "linear")
        mark(mat.get("emissiveTexture"), "colour")
        for ext in mat.get("extensions", {}).values():
            if not isinstance(ext, dict):
                continue
            for slot, info in ext.items():
                if not slot.endswith("Texture"):
                    continue
                if slot in COLOUR_EXT_SLOTS:
                    mark(info, "colour")
                elif "normal" in slot.lower():
                    mark(info, "normal")
                else:
                    mark(info, "linear")

    kinds = {}
    for i in range(len(gltf.get("textures", []))):
        used = usage.get(i, set())
        if "colour" in used and len(used) > 1:
            warn(f"texture {i} is used as both colour and data {sorted(used)}; encoding as sRGB colour")
        if not used or "colour" in used:
            kinds[i] = "colour"
        elif "normal" in used:
            kinds[i] = "normal"
        else:
            kinds[i] = "linear"
    return kinds


def mip_wrap(gltf: dict, tex: dict) -> str:
    """Map the texture's sampler wrap mode to ktx create --mipmap-wrap."""
    samplers = gltf.get("samplers", [])
    s = samplers[tex["sampler"]] if "sampler" in tex and tex["sampler"] < len(samplers) else {}
    modes = {s.get("wrapS", GL_REPEAT), s.get("wrapT", GL_REPEAT)}
    if GL_REPEAT in modes:
        return "wrap"
    if GL_MIRRORED_REPEAT in modes:
        return "reflect"
    return "clamp"


def is_ktx2_image(img: dict) -> bool:
    return img.get("mimeType") == "image/ktx2" or str(img.get("uri", "")).lower().endswith(".ktx2")


class Sources:
    """Reads image bytes from file URIs, data URIs or bufferViews."""

    def __init__(self, gltf: dict, base_dir: Path):
        self.gltf, self.base, self._buffers = gltf, base_dir, {}

    @staticmethod
    def _data_uri(uri: str) -> bytes:
        header, payload = uri.split(",", 1)
        return base64.b64decode(payload) if ";base64" in header else unquote(payload).encode("latin-1")

    def _buffer(self, idx: int) -> bytes:
        if idx not in self._buffers:
            uri = self.gltf["buffers"][idx].get("uri")
            if uri is None:
                raise ValueError("buffer has no uri (GLB binary chunk); use a .gltf input")
            self._buffers[idx] = (self._data_uri(uri) if uri.startswith("data:")
                                  else (self.base / unquote(uri)).read_bytes())
        return self._buffers[idx]

    def read(self, img: dict) -> bytes:
        if "uri" in img:
            uri = img["uri"]
            return self._data_uri(uri) if uri.startswith("data:") else (self.base / unquote(uri)).read_bytes()
        if "bufferView" in img:
            bv = self.gltf["bufferViews"][img["bufferView"]]
            off = bv.get("byteOffset", 0)
            return self._buffer(bv["buffer"])[off:off + bv["byteLength"]]
        raise ValueError("image has neither uri nor bufferView")


# --------------------------------------------------------------------------- #
# Image preparation (Pillow)
# --------------------------------------------------------------------------- #
def target_size(w: int, h: int, mode: str) -> tuple[int, int]:
    if mode == "none":
        return w, h
    if mode == "pot":
        f = lambda v: 1 << max(2, round(math.log2(v)))
    else:  # mult4
        f = lambda v: max(4, int(round(v / 4)) * 4)
    return f(w), f(h)


def to_png(data: bytes, dst: Path, resize: str) -> tuple[bool, tuple[int, int], tuple[int, int]]:
    """Normalize any Pillow-readable image to an 8-bit RGB/RGBA PNG."""
    with Image.open(io.BytesIO(data)) as src:
        # ktx create can only y-flip, so bake EXIF orientation in here.
        im = ImageOps.exif_transpose(src)
        if im.mode.startswith("I;16") or im.mode == "I":
            im = im.convert("I").point(lambda v: v * (1 / 256)).convert("L")
        has_alpha = im.mode in ("RGBA", "LA", "PA", "RGBa", "La") or (
            im.mode == "P" and "transparency" in im.info)
        im = im.convert("RGBA" if has_alpha else "RGB")
        if has_alpha and im.getchannel("A").getextrema() == (255, 255):
            im, has_alpha = im.convert("RGB"), False  # fully opaque alpha is wasted bits
        orig = im.size
        new = target_size(*orig, resize)
        if new != orig:
            im = im.resize(new, LANCZOS)
        im.info.clear()  # no ICC/gamma chunks; the transfer function is assigned explicitly
        im.save(dst, "PNG", compress_level=1)
    return has_alpha, orig, new


# --------------------------------------------------------------------------- #
# Conversion jobs
# --------------------------------------------------------------------------- #
@dataclass
class Job:
    image_index: int
    kind: str
    wrap: str
    out_rel: str
    has_alpha: bool = False
    orig_size: tuple[int, int] = (0, 0)
    new_size: tuple[int, int] = (0, 0)
    bytes_in: int = 0
    bytes_out: int = 0
    error: str | None = None
    new_image_index: int | None = None


def build_cmd(caps: KtxCaps, job: Job, png: Path, out: Path) -> list[str]:
    srgb = job.kind == "colour"
    channels = "R8G8B8A8" if job.has_alpha else "R8G8B8"
    return [caps.exe, "create",
            "--format", f"{channels}_{'SRGB' if srgb else 'UNORM'}",
            # Assigning (not converting) the TF stops ktx create from doing an
            # implicit sRGB->linear conversion on data textures.
            caps.assign_tf, "srgb" if srgb else "linear",
            "--generate-mipmap", "--mipmap-wrap", job.wrap,
            str(png), str(out)]


def run_job(job: Job, sources: Sources, gltf: dict, caps: KtxCaps,
            out_dir: Path, tmp: Path, a: argparse.Namespace) -> Job:
    try:
        data = sources.read(gltf["images"][job.image_index])
        job.bytes_in = len(data)
        png = tmp / f"img{job.image_index}_{job.kind}.png"
        job.has_alpha, job.orig_size, job.new_size = to_png(data, png, a.resize)
        out = out_dir / job.out_rel
        out.parent.mkdir(parents=True, exist_ok=True)
        r = subprocess.run(build_cmd(caps, job, png, out), capture_output=True, text=True)
        if r.returncode != 0:
            job.error = (r.stderr or r.stdout).strip().splitlines()[-1:] or [f"exit {r.returncode}"]
            job.error = f"ktx create exit {r.returncode}: {job.error[0]}"
        else:
            if r.stderr.strip():
                warn(f"{job.out_rel}: {r.stderr.strip()}")
            job.bytes_out = out.stat().st_size
    except Exception as e:  # keep going; the texture is left untouched
        job.error = f"{type(e).__name__}: {e}"
    return job


def output_name(gltf: dict, img_idx: int, kind: str, used: set[str]) -> str:
    img = gltf["images"][img_idx]
    uri = img.get("uri")
    if uri and not uri.startswith("data:"):
        stem = Path(unquote(uri)).with_suffix("").as_posix()
    else:
        base = img.get("name") or f"image_{img_idx}"
        stem = "textures/" + re.sub(r"[^A-Za-z0-9._-]+", "_", Path(base).stem)
    name, n = f"{stem}.ktx2", 1
    while name.lower() in used:
        name = f"{stem}_{kind}{'' if n == 1 else n}.ktx2"
        n += 1
    used.add(name.lower())
    return name


def compact_images(gltf: dict) -> list[dict]:
    """Drop images no texture references any more; returns the removed ones."""
    textures = gltf.get("textures", [])
    refs = set()
    for tex in textures:
        if "source" in tex:
            refs.add(tex["source"])
        for ext in tex.get("extensions", {}).values():
            if isinstance(ext, dict) and isinstance(ext.get("source"), int):
                refs.add(ext["source"])
    old = gltf.get("images", [])
    remap, kept = {}, []
    for i, img in enumerate(old):
        if i in refs:
            remap[i] = len(kept)
            kept.append(img)
    for tex in textures:
        if "source" in tex:
            tex["source"] = remap[tex["source"]]
        for ext in tex.get("extensions", {}).values():
            if isinstance(ext, dict) and isinstance(ext.get("source"), int):
                ext["source"] = remap[ext["source"]]
    gltf["images"] = kept
    return [img for i, img in enumerate(old) if i not in refs]


def copy_external(uri: str | None, src_dir: Path, dst_dir: Path) -> None:
    if not uri or uri.startswith("data:"):
        return
    src, dst = src_dir / unquote(uri), dst_dir / unquote(uri)
    if src.exists() and src.resolve() != dst.resolve():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Convert glTF textures to uncompressed, mipmapped KTX2.")
    p.add_argument("input", type=Path, help="input .gltf")
    p.add_argument("-o", "--output", type=Path, help="output .gltf (default: <input>_ktx2.gltf)")
    p.add_argument("--resize", choices=["none", "mult4", "pot"], default="none",
                   help="none = keep size (default), mult4 = nearest multiple of 4, pot = nearest power of two")
    p.add_argument("--keep-fallback", action="store_true",
                   help="keep original images as fallback; KHR_texture_basisu stays optional")
    p.add_argument("-j", "--jobs", type=int, default=1, help="textures to encode in parallel")
    p.add_argument("--ktx", help="path to the ktx executable")
    return p.parse_args()


def main() -> int:
    a = parse_args()
    if a.input.suffix.lower() != ".gltf":
        sys.exit("error: input must be a .gltf file (convert .glb with e.g. gltf-pipeline first).")
    out_path = (a.output or a.input.with_name(a.input.stem + "_ktx2.gltf")).resolve()
    if out_path == a.input.resolve():
        sys.exit("error: output would overwrite the input; pass a different -o.")
    in_dir, out_dir = a.input.resolve().parent, out_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    caps = detect_ktx(a.ktx)
    print(f"Using {caps.version}")

    gltf = json.loads(a.input.read_text(encoding="utf-8"))
    kinds = classify_textures(gltf)
    images = gltf.get("images", [])

    # One job per (image, colour-space) pair, so the same image used as both
    # sRGB and linear data gets two correctly tagged KTX2 files.
    jobs: dict[tuple[int, str], Job] = {}
    tex_job: dict[int, tuple[int, str]] = {}
    used_names: set[str] = set()
    for t, tex in enumerate(gltf.get("textures", [])):
        if EXT in tex.get("extensions", {}) or "source" not in tex:
            continue
        src = tex["source"]
        if is_ktx2_image(images[src]):
            continue
        kind = kinds[t]
        key = (src, kind)
        if key not in jobs:
            jobs[key] = Job(src, kind, mip_wrap(gltf, tex), output_name(gltf, src, kind, used_names))
        tex_job[t] = key

    if not jobs:
        print("Nothing to convert.")
        return 0

    sources = Sources(gltf, in_dir)
    with tempfile.TemporaryDirectory(prefix="gltf2ktx_") as tmp:
        with ThreadPoolExecutor(max_workers=max(1, a.jobs)) as pool:
            futures = [pool.submit(run_job, j, sources, gltf, caps, out_dir, Path(tmp), a)
                       for j in jobs.values()]
            for f in as_completed(futures):
                j = f.result()
                if j.error:
                    print(f"  FAILED  {j.out_rel}: {j.error}", file=sys.stderr)
                else:
                    resized = f" (resized from {j.orig_size[0]}x{j.orig_size[1]})" if j.orig_size != j.new_size else ""
                    print(f"  ok      {j.out_rel}  [{j.kind}, {j.new_size[0]}x{j.new_size[1]}"
                          f"{', alpha' if j.has_alpha else ''}]{resized}  "
                          f"{j.bytes_in / 1024:.0f} KB -> {j.bytes_out / 1024:.0f} KB")

    # Add the new KTX2 images and point textures at them.
    for j in jobs.values():
        if not j.error:
            j.new_image_index = len(images)
            images.append({"uri": quote(j.out_rel), "mimeType": "image/ktx2"})
    gltf["images"] = images

    converted = 0
    for t, key in tex_job.items():
        j = jobs[key]
        if j.error:
            continue
        tex = gltf["textures"][t]
        tex.setdefault("extensions", {})[EXT] = {"source": j.new_image_index}
        if not a.keep_fallback:
            del tex["source"]
        converted += 1

    if not a.keep_fallback:
        removed = compact_images(gltf)
        if any("bufferView" in img for img in removed):
            warn("embedded bufferView images were dropped from 'images' but their bytes remain in the buffer")

    used_ext = gltf.setdefault("extensionsUsed", [])
    if EXT not in used_ext:
        used_ext.append(EXT)
    all_converted = all("source" not in tex for tex in gltf.get("textures", []))
    if not a.keep_fallback and all_converted:
        req = gltf.setdefault("extensionsRequired", [])
        if EXT not in req:
            req.append(EXT)

    # If writing to a different folder, bring along buffers and any images still referenced.
    if out_dir != in_dir:
        for buf in gltf.get("buffers", []):
            copy_external(buf.get("uri"), in_dir, out_dir)
        for img in gltf.get("images", []):
            if not is_ktx2_image(img):
                copy_external(img.get("uri"), in_dir, out_dir)

    out_path.write_text(json.dumps(gltf, indent=2), encoding="utf-8")
    failed = sum(1 for j in jobs.values() if j.error)
    print(f"\nWrote {out_path}  ({converted} textures converted, {failed} images failed)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
