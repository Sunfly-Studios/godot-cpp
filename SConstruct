#!/usr/bin/env python

import os

EnsureSConsVersion(4, 0)


try:
    Import("env")
except Exception:
    # Default tools with no platform defaults to gnu toolchain.
    # We apply platform specific toolchains via our custom tools.
    env = Environment(tools=["default"], PLATFORM="")

env.PrependENVPath("PATH", os.getenv("PATH"))

# Custom options and profile flags.
customs = ["custom.py"]
try:
    customs += Import("customs")
except Exception:
    pass
profile = ARGUMENTS.get("profile", "")
if profile:
    if os.path.isfile(profile):
        customs.append(profile)
    elif os.path.isfile(profile + ".py"):
        customs.append(profile + ".py")
opts = Variables(customs, ARGUMENTS)
cpp_tool = Tool("godotcpp", toolpath=["tools"])
cpp_tool.options(opts, env)
opts.Update(env)

Help(opts.GenerateHelpText(env))

# Detect and print a warning listing unknown SCons variables to ease troubleshooting.
unknown = opts.UnknownVariables()
if unknown:
    print("WARNING: Unknown SCons variables were passed and will be ignored:")
    for item in unknown.items():
        print("    " + item[0] + "=" + item[1])

scons_cache_path = os.environ.get("SCONS_CACHE")
if scons_cache_path is not None:
    # Make the cache path unique for each platform and architecture
    platform_arch = env["PLATFORM"] + "." + env["arch"]
    platform_cache_path = os.path.join(scons_cache_path, platform_arch)
    if not os.path.exists(platform_cache_path):
        os.makedirs(platform_cache_path)
    
    CacheDir(platform_cache_path)
    Decider("MD5")

# Catches x86_32, arm32, ppc32, and wasm32.
# The most common ones.
# Other architectures like HPPA must set this manually.
# TODO: Maybe automate this.
if env["arch"].endswith("32"):
    env.Append(CPPDEFINES=["IS_32_BIT"])

cpp_tool.generate(env)
library = env.GodotCPP()

Return("env")
