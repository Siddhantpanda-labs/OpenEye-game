#!/usr/bin/env python3
"""
Sync shader sources from EngineSource/assets -> Engine/assets and compile changed Vulkan GLSL to SPIR-V.
Output .spv files are written next to synced sources under Engine/assets.
"""
from __future__ import annotations

import argparse
from typing import Dict, Iterable, List, Tuple
import json
import os
import shutil
import subprocess
import sys

# Extensions that glslc can compile (stage inferred from extension)
GLSL_EXTENSIONS = (".vert", ".frag", ".comp", ".rgen", ".rchit", ".rmiss", ".rahit", ".rcall", ".mesh", ".task")
STATE_FILE_NAME = "vulkan_shader_compile_state.json"

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


def _read_file_mtime_ns(path: str) -> int:
    return os.stat(path).st_mtime_ns


def _make_state_file_path(source_root: str) -> str:
    # Requirement: keep state JSON under EngineSource/assets/shaders.
    return os.path.join(os.path.abspath(source_root), STATE_FILE_NAME)


def load_state(path: str) -> Dict[str, Dict[str, int]]:
    if not os.path.isfile(path):
        return {"version": 1, "files": {}}
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict):
            return {"version": 1, "files": {}}
        files = data.get("files", {})
        if not isinstance(files, dict):
            files = {}
        return {"version": 1, "files": files}
    except (OSError, json.JSONDecodeError):
        return {"version": 1, "files": {}}


def save_state(path: str, state: Dict[str, Dict[str, int]]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2, sort_keys=True)
        f.write("\n")


def sync_shaders(source_root: str, dest_root: str, changed_relative_paths: List[str], quiet: bool, force_copy: bool = False) -> List[str]:
    changed: List[str] = []
    source_root = os.path.abspath(source_root)
    dest_root = os.path.abspath(dest_root)
    for rel in changed_relative_paths:
        src = os.path.join(source_root, rel)
        if not os.path.isfile(src):
            continue
        dst = os.path.join(dest_root, rel)
        if force_copy or _needs_copy(src, dst):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            if not quiet:
                print(f"  sync {rel}")
        elif not quiet:
            print(f"  keep {rel}")
        # Compile all state-detected changed shaders, even if copy wasn't required.
        changed.append(dst)
    return changed


def compile_files(glslc: str, files: Iterable[str], root: str, quiet: bool) -> Tuple[int, int, List[str]]:
    compiled = 0
    failed = 0
    successful_sources: List[str] = []
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
            successful_sources.append(src)
        except subprocess.CalledProcessError as e:
            failed += 1
            print(f"Error compiling {src}:", file=sys.stderr)
            if e.stderr:
                sys.stderr.buffer.write(e.stderr)
            print(file=sys.stderr)
    return compiled, failed, successful_sources


def compute_changed_vulkan_relative_paths(source_root: str, state_files: Dict[str, int]) -> List[str]:
    source_root = os.path.abspath(source_root)
    changed: List[str] = []
    for src in _iter_glsl_files(source_root):
        if not _is_vulkan_shader_path(src):
            continue
        rel = os.path.relpath(src, source_root).replace("\\", "/")
        mtime_ns = _read_file_mtime_ns(src)
        if mtime_ns > int(state_files.get(rel, 0)):
            changed.append(rel)
    return changed


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
        "--reset-state",
        action="store_true",
        help="Reset persisted Vulkan shader compile state before detecting changes",
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
    state_file = _make_state_file_path(source_root)
    state = {"version": 1, "files": {}} if args.reset_state else load_state(state_file)
    state_files: Dict[str, int] = state["files"]

    changed_rel = compute_changed_vulkan_relative_paths(source_root, state_files)
    changed_files = sync_shaders(source_root, runtime_root, changed_rel, args.quiet, force_copy=args.reset_state)
    if not args.quiet:
        print(f"State file:   {state_file}")
        print(f"Detected {len(changed_rel)} changed Vulkan source(s) from state.")
        print(f"Synced {len(changed_files)} changed shader source(s).")

    compile_set = [p for p in changed_files if _is_glsl(p) and _is_vulkan_shader_path(p)]

    # Always compile changed files; if none changed, skip compilation by default.
    if not compile_set:
        if not args.quiet:
            print("No changed Vulkan shader sources to compile.")
        # Still persist/reset state if requested.
        save_state(state_file, state)
        if not args.quiet:
            print(f"Updated compile state: {state_file}")
        return 0

    total_compiled, total_failed, successful_sources = compile_files(glslc, compile_set, runtime_root, args.quiet)

    if not args.quiet:
        print(f"Compiled {total_compiled} shader(s).")

    # Update state for successfully compiled shaders; failed ones remain stale and will be retried.
    successful_rels = {
        os.path.relpath(src, runtime_root).replace("\\", "/") for src in successful_sources
    }
    for rel in successful_rels:
        src = os.path.join(source_root, rel)
        if os.path.isfile(src):
            state_files[rel] = _read_file_mtime_ns(src)
    save_state(state_file, state)
    if not args.quiet:
        print(f"Updated compile state: {state_file}")
    if total_failed:
        print(f"Failed: {total_failed}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
