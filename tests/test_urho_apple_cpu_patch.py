#!/usr/bin/env python3
"""Offline regression tests for the Urho3D Apple Silicon compatibility patch."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("apple_cpu_patch", ROOT / "tools/patch_urho_apple_cpu.py")
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

# Small stand-in for the four sections of Urho3D 1.9.0 ProcessUtils.cpp.
# It has no platform SDK headers, so we can compile all of the preprocessor
# branches on any Unix host without pretending to have a macOS SDK installed.
SOURCE = r'''
using size_t = __SIZE_TYPE__;
int mode = 0;
int sysctlbyname(const char* name, void* value, size_t* size, void*, size_t)
{
    if (mode == 1) return -1;
    *static_cast<int*>(value) = mode == 2 ? 0 : (name[3] == 'p' ? 6 : 12);
    *size = sizeof(int);
    return 0;
}
#if defined(IOS)
#elif defined(TVOS)
#elif !defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <LibCpuId/libcpuid.h>
#endif
#if defined(IOS)
#elif defined(__linux__)
struct CpuCoreCount { unsigned numPhysicalCores_; unsigned numLogicalCores_; };
static void GetCPUData(CpuCoreCount* data) { data->numPhysicalCores_ = 2; data->numLogicalCores_ = 4; }
#elif !defined(__EMSCRIPTEN__) && !defined(TVOS)
static void GetCPUData(struct cpu_id_t* data) { cpu_identify(nullptr, data); }
#endif
unsigned GetNumPhysicalCPUs()
{
#if defined(IOS)
    return 3;
#elif defined(TVOS)
    return 4;
#elif defined(__linux__)
    struct CpuCoreCount data{};
    GetCPUData(&data);
    return data.numPhysicalCores_;
#elif defined(__EMSCRIPTEN__)
    return 1;
#else
    struct cpu_id_t data;
    GetCPUData(&data);
    return (unsigned)data.num_cores;
#endif
}
unsigned GetNumLogicalCPUs()
{
#if defined(IOS)
    return 5;
#elif defined(TVOS)
    return 6;
#elif defined(__linux__)
    struct CpuCoreCount data{};
    GetCPUData(&data);
    return data.numLogicalCores_;
#elif defined(__EMSCRIPTEN__)
    return 1;
#else
    struct cpu_id_t data;
    GetCPUData(&data);
    return (unsigned)data.num_logical_cpus;
#endif
}
int main()
{
#if defined(IOS)
    return GetNumPhysicalCPUs() != 3 || GetNumLogicalCPUs() != 5;
#elif defined(TVOS)
    return GetNumPhysicalCPUs() != 4 || GetNumLogicalCPUs() != 6;
#elif defined(__linux__)
    return GetNumPhysicalCPUs() != 2 || GetNumLogicalCPUs() != 4;
#elif defined(__EMSCRIPTEN__)
    return GetNumPhysicalCPUs() != 1 || GetNumLogicalCPUs() != 1;
#elif defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    if (GetNumPhysicalCPUs() != 6 || GetNumLogicalCPUs() != 12) return 1;
    mode = 1;
    if (GetNumPhysicalCPUs() != 1 || GetNumLogicalCPUs() != 1) return 2;
    mode = 2;
    return GetNumPhysicalCPUs() != 1 || GetNumLogicalCPUs() != 1;
#else
    return GetNumPhysicalCPUs() != 7 || GetNumLogicalCPUs() != 14;
#endif
}
'''


class AppleCpuPatchTests(unittest.TestCase):
    def test_idempotent_and_rejects_partial_layout(self):
        result = patch.apply_patch(SOURCE)
        self.assertNotEqual(result, SOURCE)
        self.assertEqual(patch.apply_patch(result), result)
        self.assertEqual(result.count('"hw.physicalcpu"'), 1)
        self.assertEqual(result.count('"hw.logicalcpu"'), 1)
        self.assertEqual(patch.apply_patch("unrelated source"), "unrelated source")
        with self.assertRaises(ValueError):
            patch.apply_patch(SOURCE.replace("#elif !defined(__linux__) && !defined(__EMSCRIPTEN__)", "#elif 1"))

    def test_real_urho_source_when_provided(self):
        upstream = os.environ.get("CHIKENGUN_URHO_TEST_SOURCE")
        if not upstream:
            self.skipTest("no Urho3D source was provided")
        original = (Path(upstream) / "Source/Urho3D/Core/ProcessUtils.cpp").read_text(encoding="utf-8")
        updated = patch.apply_patch(original)
        self.assertNotEqual(updated, original, "Urho3D 1.9.0 should match the compatibility patch")
        self.assertEqual(patch.apply_patch(updated), updated)

    @unittest.skipUnless(os.name == "posix" and shutil.which("c++"), "requires a Unix C++ compiler")
    def test_preprocessor_branches_and_cpu_count_fallbacks(self):
        cases = {
            "linux": ["-D__linux__"],
            "apple_arm64": ["-D__APPLE__", "-D__arm64__"],
            "apple_aarch64": ["-D__APPLE__", "-D__aarch64__"],
            "apple_intel": ["-D__APPLE__"],
            "ios": ["-D__APPLE__", "-D__arm64__", "-DIOS"],
            "tvos": ["-D__APPLE__", "-D__arm64__", "-DTVOS"],
            "web": ["-D__EMSCRIPTEN__"],
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "test.cpp"
            source.write_text(patch.apply_patch(SOURCE), encoding="utf-8")
            header = root / "LibCpuId/libcpuid.h"
            header.parent.mkdir()
            header.write_text("struct cpu_id_t { int num_cores; int num_logical_cpus; };\n"
                              "inline void cpu_identify(void*, cpu_id_t* d) { d->num_cores = 7; d->num_logical_cpus = 14; }\n")
            for name, defines in cases.items():
                with self.subTest(platform=name):
                    # Only Intel is allowed to find LibCpuId; ARM must not include it.
                    includes = ["-I", temp] if name == "apple_intel" else []
                    executable = root / name
                    command = ["c++", "-std=c++11", "-U__linux__", "-U__APPLE__", "-U__arm64__", "-U__aarch64__"]
                    subprocess.run(command + defines + includes + [str(source), "-o", str(executable)], check=True)
                    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
