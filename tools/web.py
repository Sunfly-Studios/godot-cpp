import common_compiler_flags
from SCons.Util import WhereIs


def exists(env):
    return WhereIs("emcc") is not None


def generate(env):
    if env["arch"] not in ("wasm32"):
        print("Only wasm32 supported on web. Exiting.")
        env.Exit(1)

    # Emscripten toolchain
    env["CC"] = "emcc"
    env["CXX"] = "em++"
    env["AR"] = "emar"
    env["RANLIB"] = "emranlib"

    # Use TempFileMunge since some AR invocations are too long for cmd.exe.
    # Use POSIX-style paths, required with TempFileMunge.
    env["ARCOM_POSIX"] = env["ARCOM"].replace("$TARGET", "$TARGET.posix").replace("$SOURCES", "$SOURCES.posix")
    env["ARCOM"] = "${TEMPFILE(ARCOM_POSIX)}"

    # All intermediate files are just object files.
    env["OBJSUFFIX"] = ".o"
    env["SHOBJSUFFIX"] = ".o"

    # Static libraries clang-style.
    env["LIBPREFIX"] = "lib"
    env["LIBSUFFIX"] = ".a"

    # Shared library as wasm.
    env["SHLIBSUFFIX"] = ".wasm"

    # Thread support (via SharedArrayBuffer).
    if env["threads"]:
        env.Append(CCFLAGS=["-sUSE_PTHREADS=1"])
        env.Append(LINKFLAGS=["-sUSE_PTHREADS=1"])

        # Lowest browsers that can be targeted because these have
        # DedicatedWorkerGlobalScope.name parameter for threading support
        env.Append(LINKFLAGS=["-sMIN_FIREFOX_VERSION=79"])
        env.Append(LINKFLAGS=["-sMIN_CHROME_VERSION=75"])
        env.Append(LINKFLAGS=["-sMIN_SAFARI_VERSION=150000"])
    else:
        # These are the browsers that provide enough
        # _practical_ support. As in, what can Emscripten
        # realistically support.
        # While lower browsers could be targeted, those came
        # long before WASM became a standard, which was in 2017.
        # Except for Firefox. I set 55 (2017) but it didn't support
        # ReadableStreaming until 65 from 2019.
        env.Append(LINKFLAGS=["-sMIN_FIREFOX_VERSION=65"])
        env.Append(LINKFLAGS=["-sMIN_CHROME_VERSION=70"])
        env.Append(LINKFLAGS=["-sMIN_SAFARI_VERSION=120200"])

    # Build as side module (shared library).
    env.Append(CCFLAGS=["-sSIDE_MODULE=1"])
    env.Append(LINKFLAGS=["-sSIDE_MODULE=1"])

    # Disable WebAssembly BigInt <-> i64 conversion.
    env.Append(LINKFLAGS=["-sWASM_BIGINT=0"])

    # Force emscripten longjmp mode.
    env.Append(CCFLAGS=["-sSUPPORT_LONGJMP='emscripten'"])
    env.Append(LINKFLAGS=["-sSUPPORT_LONGJMP='emscripten'"])

    # Enable LEGACY_VM_SUPPORT for older JS engines,
    # but still keep WASM enabled because it is a hard requirement
    # for Godot.
    env.Append(LINKFLAGS=["-sLEGACY_VM_SUPPORT=1"])
    env.Append(LINKFLAGS=["-sWASM=1"])

    # Add polyfill for older browsers.
    env.Append(LINKFLAGS=["-sPOLYFILL_OLD_MATH_FUNCTIONS=1"])

    env.Append(CPPDEFINES=["WEB_ENABLED", "UNIX_ENABLED"])

    # Refer to https://github.com/godotengine/godot/blob/master/platform/web/detect.py
    if env["lto"] == "auto":
        env["lto"] = "full"

    common_compiler_flags.generate(env)
