import common_compiler_flags
from SCons.Tool import clang, clangxx
from SCons.Variables import BoolVariable


def options(opts):
    opts.Add(BoolVariable("use_llvm", "Use the LLVM compiler - only effective when targeting Linux", False))


def exists(env):
    return True


def generate(env):
    if env["use_llvm"]:
        clang.generate(env)
        clangxx.generate(env)
    elif env.use_hot_reload:
        # Required for extensions to truly unload.
        env.Append(CXXFLAGS=["-fno-gnu-unique"])

    env.Append(CCFLAGS=["-fPIC", "-Wwrite-strings"])
    env.Append(LINKFLAGS=["-Wl,-R,'$$ORIGIN'"])

    if env["arch"] == "x86_64":
        # -m64 and -m32 are x86-specific already, but it doesn't hurt to
        # be clear and also specify -march=x86-64. Similar with 32-bit.
        env.Append(CCFLAGS=["-m64", "-march=x86-64"])
        env.Append(LINKFLAGS=["-m64", "-march=x86-64"])
    elif env["arch"] == "x86_32":
        env.Append(CCFLAGS=["-m32", "-march=i686"])
        env.Append(LINKFLAGS=["-m32", "-march=i686"])
    elif env["arch"] == "arm64":
        env.Append(CCFLAGS=["-march=armv8-a"])
        env.Append(LINKFLAGS=["-march=armv8-a"])
    elif env["arch"] == "arm32":
        env.Append(CCFLAGS=["-march=armv7-a"])
        env.Append(LINKFLAGS=["-march=armv7-a"])
    elif env["arch"] == "rv64":
        env.Append(CCFLAGS=["-march=rv64gc"])
        env.Append(LINKFLAGS=["-march=rv64gc"])
    elif env["arch"] == "loongarch64":
        env.Append(CCFLAGS=["-march=loongarch64", "-mabi=lp64d"])
        env.Append(LINKFLAGS=["-march=loongarch64", "-mabi=lp64d"])
    elif env["arch"] == "sparc64":
        env.Append(CCFLAGS=["-mcpu=ultrasparc", "-m64"])
        env.Append(LINKFLAGS=["-mcpu=ultrasparc", "-m64"])
    elif env["arch"] == "mips64":
        env.Append(
            CCFLAGS=[
                "-march=mips3",
                "-mabi=64",
                "-mlong-calls",
                "-mxgot",
                "-ffunction-sections",
                "-fdata-sections",
                "-fPIC",
            ]
        )
        env.Append(CCFLAGS=["-fno-inline", "-fno-inline-functions"])
        env.Append(LINKFLAGS=["-march=mips3", "-mabi=64"])
    elif env["arch"] == "alpha":
        env.Append(
            CCFLAGS=[
                "-mcpu=ev56",
                "-mieee",
                "-mbuild-constants",
                "-mlarge-data",
                "-mtrap-precision=i"
            ]
        )
        env.Append(LINKFLAGS=["-Wl,--no-relax", "-mlarge-data"])

    env.Append(CPPDEFINES=["LINUX_ENABLED", "UNIX_ENABLED"])

    # Refer to https://github.com/godotengine/godot/blob/master/platform/linuxbsd/detect.py
    if env["lto"] == "auto":
        env["lto"] = "full"

    common_compiler_flags.generate(env)
