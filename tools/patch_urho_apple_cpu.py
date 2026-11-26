#!/usr/bin/env python3
"""Patch Urho3D 1.9.0 to count Apple Silicon CPUs without x86-only libcpuid.

The old Urho3D build does not build/install LibCpuId for ARM, but its macOS
ProcessUtils.cpp still includes it. Apple exposes the same counts via sysctl.
The patch is deliberately limited to Apple ARM; Intel macOS is unchanged.
"""
from pathlib import Path
import sys


MARKER = "CHIKENGUN_APPLE_SILICON_CPU"
MAC_ARM = "defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))"


def apply_patch(source):
    if MARKER in source:
        return source

    include_guard = "#elif !defined(__linux__) && !defined(__EMSCRIPTEN__)"
    helper_guard = "#elif !defined(__EMSCRIPTEN__) && !defined(TVOS)"
    replacements = [
        (include_guard, include_guard + " && !(" + MAC_ARM + ") // " + MARKER),
        (helper_guard, helper_guard + " && !(" + MAC_ARM + ")"),
    ]
    for field, sysctl_name in (
        ("numPhysicalCores_", "hw.physicalcpu"),
        ("numLogicalCores_", "hw.logicalcpu"),
    ):
        original = (
            "#elif defined(__linux__)\n"
            "    struct CpuCoreCount data{};\n"
            "    GetCPUData(&data);\n"
            f"    return data.{field};"
        )
        replacement = (
            f"#elif {MAC_ARM}\n"
            "    int count = 1;\n"
            "    size_t length = sizeof(count);\n"
            f'    if (sysctlbyname("{sysctl_name}", &count, &length, nullptr, 0) != 0 || count < 1)\n'
            "        return 1;\n"
            "    return (unsigned)count;\n"
            + original
        )
        replacements.append((original, replacement))

    counts = [source.count(old) for old, _ in replacements]
    if not any(counts):
        # The user may have requested a different Urho version with another layout.
        print("[Urho3D] Source does not match the 1.9.0 Apple CPU patch; leaving it unchanged.")
        return source
    if any(count != 1 for count in counts):
        raise ValueError(f"Unexpected Urho3D ProcessUtils.cpp layout (match counts: {counts})")
    for old, new in replacements:
        source = source.replace(old, new, 1)
    return source


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: patch_urho_apple_cpu.py <Urho3D source root>")
    path = Path(sys.argv[1]) / "Source/Urho3D/Core/ProcessUtils.cpp"
    original = path.read_text(encoding="utf-8")
    patched = apply_patch(original)
    if patched != original:
        path.write_text(patched, encoding="utf-8")
        print("[Urho3D] Apple Silicon CPU detection patched using sysctl.")


if __name__ == "__main__":
    main()
