#!/usr/bin/env python3
"""
Compile all BGFX shaders under assets/shaders/bgfx/ with shaderc and write
.bin files to an output directory (e.g. build/bin/Debug/assets/shaders/bgfx/<renderer>/).

Usage:
  python scripts/compile_bgfx_shaders.py [options]

Options:
  --src-dir DIR     Root of BGFX shader sources (default: assets/shaders/bgfx)
  --bgfx-include    Path to bgfx src for #include <bgfx_shader.sh> (default: thirdparty/bgfx.cmake/bgfx/src)
  --shaderc PATH    Path to shaderc executable (default: look in PATH or build tree)
  --out-dir DIR     Output root; .bin go to <out-dir>/shaders/bgfx/<renderer>/ (default: build/bin/Debug/assets)
  --renderer R      vulkan | gl | d3d11 | all (default: vulkan)
  --platform P      windows | linux | osx (default: from OS)
  --dry-run         Print commands only, do not run
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

# Defaults relative to repo root (script is in scripts/)
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SRC_DIR = REPO_ROOT / "assets" / "shaders" / "bgfx"
DEFAULT_BGFX_INCLUDE = REPO_ROOT / "thirdparty" / "bgfx.cmake" / "bgfx" / "src"
DEFAULT_OUT_DIR = REPO_ROOT / "build" / "bin" / "Debug" / "assets"

RENDERER_PROFILES = {
    "vulkan": "spirv",
    "gl": "140",
    "d3d11": "s_5_0",
}


def find_shaderc(shaderc_path: str | None, build_dir: Path) -> Path | None:
    if shaderc_path:
        p = Path(shaderc_path)
        if p.is_file():
            return p
        return None
    # Check PATH
    import shutil
    in_path = shutil.which("shaderc")
    if in_path:
        return Path(in_path)
    # Common build locations
    for base in (build_dir, REPO_ROOT / "build"):
        for pattern in ("**/shaderc.exe", "**/shaderc"):
            for p in base.glob(pattern):
                if p.is_file():
                    return p
    return None


def collect_shader_pairs(src_dir: Path) -> list[tuple[str, Path, Path, Path]]:
    """Collect (name, vert_path, frag_path, varying_path)."""
    pairs = []
    for vert_path in src_dir.rglob("*.vert"):
        frag_path = vert_path.with_suffix(".frag")
        if not frag_path.is_file():
            continue
        # varying.def.sc in same directory
        varying_path = vert_path.parent / "varying.def.sc"
        if not varying_path.is_file():
            varying_path = src_dir / "varying.def.sc"
        if not varying_path.is_file():
            continue
        name = vert_path.stem
        pairs.append((name, vert_path, frag_path, varying_path))
    return pairs


def platform_from_os() -> str:
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "osx"
    return "linux"


def main() -> int:
    ap = argparse.ArgumentParser(description="Compile BGFX shaders with shaderc")
    ap.add_argument("--src-dir", type=Path, default=DEFAULT_SRC_DIR, help="BGFX shader source root")
    ap.add_argument("--bgfx-include", type=Path, default=DEFAULT_BGFX_INCLUDE, help="Path to bgfx/src for includes")
    ap.add_argument("--shaderc", type=str, default=None, help="Path to shaderc executable")
    ap.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR, help="Output root (shaders go under <out>/shaders/bgfx/<renderer>/)")
    ap.add_argument("--renderer", choices=("vulkan", "gl", "d3d11", "all"), default="vulkan", help="Backend to compile for")
    ap.add_argument("--platform", choices=("windows", "linux", "osx"), default=None, help="Target platform (default: from OS)")
    ap.add_argument("--dry-run", action="store_true", help="Print commands only")
    args = ap.parse_args()

    src_dir = args.src_dir.resolve()
    bgfx_include = args.bgfx_include.resolve()
    out_dir = args.out_dir.resolve()
    platform = args.platform or platform_from_os()

    if not src_dir.is_dir():
        print(f"Error: source dir not found: {src_dir}", file=sys.stderr)
        return 1
    if not bgfx_include.is_dir():
        print(f"Error: bgfx include dir not found: {bgfx_include}", file=sys.stderr)
        return 1

    build_dir = REPO_ROOT / "build"
    shaderc_exe = find_shaderc(args.shaderc, build_dir)
    if not shaderc_exe:
        print("Error: shaderc not found. Build BGFX with BGFX_BUILD_TOOLS_SHADER=ON or set --shaderc.", file=sys.stderr)
        return 1

    pairs = collect_shader_pairs(src_dir)
    if not pairs:
        print(f"No .vert/.frag pairs found under {src_dir}", file=sys.stderr)
        return 1

    renderers = list(RENDERER_PROFILES.keys()) if args.renderer == "all" else [args.renderer]
    failed = []

    for renderer in renderers:
        profile = RENDERER_PROFILES[renderer]
        renderer_out = out_dir / "shaders" / "bgfx" / renderer
        if not args.dry_run:
            renderer_out.mkdir(parents=True, exist_ok=True)

        for name, vert_path, frag_path, varying_path in pairs:
            vs_out = renderer_out / f"{name}_vs.bin"
            fs_out = renderer_out / f"{name}_fs.bin"
            try:
                vert_rel = vert_path.relative_to(REPO_ROOT)
                frag_rel = frag_path.relative_to(REPO_ROOT)
                varying_rel = varying_path.relative_to(REPO_ROOT)
            except ValueError:
                vert_rel = vert_path
                frag_rel = frag_path
                varying_rel = varying_path

            for stage, path_rel, out_file in (
                ("v", vert_rel, vs_out),
                ("f", frag_rel, fs_out),
            ):
                cmd = [
                    str(shaderc_exe),
                    "-f", str(path_rel),
                    "-o", str(out_file),
                    "--type", stage,
                    "--platform", platform,
                    "-p", profile,
                    "-i", str(bgfx_include),
                    "--varyingdef", str(varying_rel),
                ]
                print(" ".join(cmd))
                if not args.dry_run:
                    try:
                        subprocess.run(cmd, cwd=REPO_ROOT, check=True)
                    except subprocess.CalledProcessError as e:
                        failed.append((path_rel, e))

    if failed:
        for path, err in failed:
            print(f"Failed: {path}", file=sys.stderr)
        return 1
    print(f"Compiled {len(pairs)} shader(s) for {', '.join(renderers)} -> {out_dir / 'shaders' / 'bgfx'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
