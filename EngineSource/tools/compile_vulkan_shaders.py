#!/usr/bin/env python3
"""
Sync shader sources from EngineSource/assets -> Engine/assets and compile changed Vulkan GLSL to SPIR-V.
Output .spv files are written next to synced sources under Engine/assets.
"""
from __future__ import annotations

import argparse
from typing import Iterable, List, Tuple
import os
import shutil
import subprocess
import sys

# Extensions that glslc can compile (stage inferred from extension)
GLSL_EXTENSIONS = (".vert", ".frag", ".comp", ".rgen", ".rchit", ".rmiss", ".rahit", ".rcall", ".mesh", ".task")

def find_glslc() -> str | None:
    # Prefer VULKAN_SDK/bin, then PATH
    sdk = os.environ.get("VULKAN_SDK")
    if sdk:
        candidate = os.path.join(sdk, "bin", "glslc.exe" if sys.platform == "win32" else "glslc")
        if os.path.isfile(candidate):
            return candidate
    return shutil.which("glslc")

def _is_glsl(path: str) -> bool:
    _base, ext = os.path.splitext(path)
    return ext.lower() in GLSL_EXTENSIONS


def _iter_glsl_files(root: str) -> Iterable[str]:
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            src = os.path.join(dirpath, name)
            if _is_glsl(src):
                yield src


def _is_vulkan_shader_path(path: str) -> bool:
    normalized = path.replace("\\", "/").lower()
    return "/vulkan/" in normalized


def _needs_copy(src: str, dst: str) -> bool:
    if not os.path.isfile(dst):
        return True
    sst = os.stat(src)
    dstst = os.stat(dst)
    # Copy if source is newer or size differs (safe + cheap).
    if sst.st_size != dstst.st_size:
        return True
    return sst.st_mtime_ns > dstst.st_mtime_ns


def sync_shaders(source_root: str, dest_root: str, quiet: bool) -> List[str]:
    changed: List[str] = []
    source_root = os.path.abspath(source_root)
    dest_root = os.path.abspath(dest_root)
    for src in _iter_glsl_files(source_root):
        if not _is_vulkan_shader_path(src):
            continue
        rel = os.path.relpath(src, source_root)
        dst = os.path.join(dest_root, rel)
        if not _needs_copy(src, dst):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        changed.append(dst)
        if not quiet:
            print(f"  sync {rel}")
    return changed


def compile_files(glslc: str, files: Iterable[str], root: str, quiet: bool) -> Tuple[int, int]:
    compiled = 0
    failed = 0
    root = os.path.abspath(root)
    for src in files:
        out = src + ".spv"
        if not quiet:
            print(f"  {os.path.relpath(src, root)} -> {os.path.relpath(out, root)}")
        try:
            subprocess.run(
                [glslc, src, "-o", out],
                check=True,
                capture_output=True,
                cwd=root,
            )
            compiled += 1
        except subprocess.CalledProcessError as e:
            failed += 1
            print(f"Error compiling {src}:", file=sys.stderr)
            if e.stderr:
                sys.stderr.buffer.write(e.stderr)
            print(file=sys.stderr)
    return compiled, failed


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sync EngineSource shader sources to Engine/assets and compile changed Vulkan GLSL to SPIR-V."
    )
    engine_source = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # .../EngineSource
    project_root = os.path.dirname(engine_source)
    engine_runtime = os.path.join(project_root, "Engine")
    source_assets = os.path.join(engine_source, "assets", "shaders")
    runtime_assets = os.path.join(engine_runtime, "assets", "shaders")
    parser.add_argument(
        "--source-root",
        default=source_assets,
        help="Shader source root under EngineSource (default: EngineSource/assets/shaders)",
    )
    parser.add_argument(
        "--runtime-root",
        default=runtime_assets,
        help="Runtime shader root under Engine (default: Engine/assets/shaders)",
    )
    parser.add_argument(
        "--compile-all",
        action="store_true",
        help="Compile all Vulkan GLSL files in runtime root after sync (default: changed only)",
    )
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print errors")
    args = parser.parse_args()

    glslc = find_glslc()
    if not glslc:
        print("Error: glslc not found. Install Vulkan SDK or add glslc to PATH.", file=sys.stderr)
        return 1

    source_root = os.path.abspath(args.source_root)
    runtime_root = os.path.abspath(args.runtime_root)
    if not os.path.isdir(source_root):
        print(f"Error: source shader root not found: {source_root}", file=sys.stderr)
        return 1
    if not os.path.isdir(os.path.dirname(runtime_root)):
        print(f"Error: runtime shader parent directory not found: {os.path.dirname(runtime_root)}", file=sys.stderr)
        return 1
    os.makedirs(runtime_root, exist_ok=True)

    if not args.quiet:
        print(f"Sync source:  {source_root}")
        print(f"Sync target:  {runtime_root}")
    changed_files = sync_shaders(source_root, runtime_root, args.quiet)
    if not args.quiet:
        print(f"Synced {len(changed_files)} changed shader source(s).")

    if args.compile_all:
        compile_set = [p for p in _iter_glsl_files(runtime_root) if _is_vulkan_shader_path(p)]
    else:
        compile_set = [p for p in changed_files if _is_glsl(p) and _is_vulkan_shader_path(p)]

    # Always compile changed files; if none changed, skip compilation by default.
    if not compile_set:
        if not args.quiet:
            print("No changed Vulkan shader sources to compile.")
        return 0

    total_failed = 0
    total_compiled, total_failed = compile_files(glslc, compile_set, runtime_root, args.quiet)

    if not args.quiet:
        print(f"Compiled {total_compiled} shader(s).")
    if total_failed:
        print(f"Failed: {total_failed}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
