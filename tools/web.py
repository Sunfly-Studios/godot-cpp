import common_compiler_flags
from SCons.Util import WhereIs


def exists(env):
    return WhereIs("emcc") is not None


def generate(env):
    if env["arch"] not in ("wasm32", "wasm64"):
        print("Only wasm32 or wasm64 supported on web. Exiting.")
        env.Exit(1)

    flip_wasm64_requirement = 1 if env["arch"] == "wasm64" else 0

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

    # Build as side module (shared library).
    env.Append(CCFLAGS=["-sSIDE_MODULE=1"])
    env.Append(LINKFLAGS=["-sSIDE_MODULE=1"])

    # Enable or disable WebAssembly BigInt <-> i64 conversion
    # for wasm32 or wasm64.
    env.Append(LINKFLAGS=[f"-sWASM_BIGINT={flip_wasm64_requirement}"])
    env.Append(CCFLAGS=[f"-sMEMORY64={flip_wasm64_requirement}"])
    env.Append(LINKFLAGS=[f"-sMEMORY64={flip_wasm64_requirement}"])

    # Thread support (via SharedArrayBuffer).
    if env["threads"]:
        env.Append(CCFLAGS=["-sUSE_PTHREADS=1"])
        env.Append(LINKFLAGS=["-sUSE_PTHREADS=1"])

    # Still keep WASM enabled because it is a hard requirement
    # for Godot.
    env.Append(LINKFLAGS=["-sWASM=1"])

    if env["arch"] == "wasm32":
        # Force emscripten longjmp mode.
        env.Append(CCFLAGS=["-sSUPPORT_LONGJMP='emscripten'"])
        env.Append(LINKFLAGS=["-sSUPPORT_LONGJMP='emscripten'"])

        # Enable LEGACY_VM_SUPPORT for older JS engines.
        env.Append(LINKFLAGS=["-sLEGACY_VM_SUPPORT=1"])

        # Add polyfill for older browsers.
        env.Append(LINKFLAGS=["-sPOLYFILL_OLD_MATH_FUNCTIONS=1"])

        if env["threads"]:
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
    else: # wasm64
        env.Append(CCFLAGS=["-sSUPPORT_LONGJMP='wasm'"])
        env.Append(LINKFLAGS=["-sSUPPORT_LONGJMP='wasm'"])

        env.Append(LINKFLAGS=["-sMIN_FIREFOX_VERSION=134"])
        env.Append(LINKFLAGS=["-sMIN_CHROME_VERSION=133"])
        print_warning("`wasm64` is experimental. Is only supported on Firefox 134 and Chrome 133 and later.")

    env.Append(CPPDEFINES=["WEB_ENABLED", "UNIX_ENABLED"])

    # Refer to https://github.com/godotengine/godot/blob/master/platform/web/detect.py
    if env["lto"] == "auto":
        env["lto"] = "full"

    common_compiler_flags.generate(env)
