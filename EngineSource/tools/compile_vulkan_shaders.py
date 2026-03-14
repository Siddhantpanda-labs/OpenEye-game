#!/usr/bin/env python3
"""
Compile Vulkan GLSL shaders to SPIR-V with glslc.
Output .spv files are written next to each source file (e.g. lighting.vert -> lighting.vert.spv).
"""
from __future__ import annotations

import argparse
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

def main() -> int:
    parser = argparse.ArgumentParser(description="Compile Vulkan GLSL to SPIR-V; .spv written next to sources.")
    default_root = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "shaders", "vulkan")
    parser.add_argument("--root", default=default_root, help="Root directory to search for .vert/.frag/etc. (default: EngineSource/assets/shaders/vulkan)")
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print errors")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print(f"Error: root is not a directory: {root}", file=sys.stderr)
        return 1

    glslc = find_glslc()
    if not glslc:
        print("Error: glslc not found. Install Vulkan SDK or add glslc to PATH.", file=sys.stderr)
        return 1

    compiled = 0
    failed = 0
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            base, ext = os.path.splitext(name)
            if ext.lower() not in GLSL_EXTENSIONS:
                continue
            src = os.path.join(dirpath, name)
            out = src + ".spv"
            if not args.quiet:
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

    if not args.quiet:
        print(f"Compiled {compiled} shader(s).")
    if failed:
        print(f"Failed: {failed}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
