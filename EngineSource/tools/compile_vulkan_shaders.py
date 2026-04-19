#!/usr/bin/env python3
"""
Compile Vulkan GLSL shaders to SPIR-V with glslc.
Output .spv files are written next to each source file (e.g. lighting.vert -> lighting.vert.spv).
"""
from __future__ import annotations

import argparse
from typing import Tuple
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

def compile_tree(glslc: str, root: str, quiet: bool) -> Tuple[int, int]:
    compiled = 0
    failed = 0
    root = os.path.abspath(root)
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            _base, ext = os.path.splitext(name)
            if ext.lower() not in GLSL_EXTENSIONS:
                continue
            src = os.path.join(dirpath, name)
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
    parser = argparse.ArgumentParser(description="Compile Vulkan GLSL to SPIR-V; .spv written next to sources.")
    engine_source = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    default_legacy = os.path.join(engine_source, "assets", "shaders", "vulkan")
    default_rendering = os.path.join(engine_source, "assets", "shaders", "rendering", "vulkan")
    parser.add_argument(
        "--root",
        default=None,
        help="Single root to compile (default: legacy vulkan + rendering/vulkan if directories exist)",
    )
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print errors")
    args = parser.parse_args()

    glslc = find_glslc()
    if not glslc:
        print("Error: glslc not found. Install Vulkan SDK or add glslc to PATH.", file=sys.stderr)
        return 1

    if args.root:
        roots = [os.path.abspath(args.root)]
    else:
        roots = []
        for r in (default_legacy, default_rendering):
            if os.path.isdir(r):
                roots.append(os.path.abspath(r))

    if not roots:
        print("Error: no shader root directory found.", file=sys.stderr)
        return 1

    total_compiled = 0
    total_failed = 0
    for root in roots:
        if not args.quiet:
            print(f"Root: {root}")
        c, f = compile_tree(glslc, root, args.quiet)
        total_compiled += c
        total_failed += f

    if not args.quiet:
        print(f"Compiled {total_compiled} shader(s).")
    if total_failed:
        print(f"Failed: {total_failed}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
