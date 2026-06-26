

def detect_and_set_32_bit_arch(env):
    arch = env.get("arch", "")
    special_32_bit_archs = {
        # This is required because programs compiled for HPPA
        # are usually 32-bit (because the 64-bit userland was
        # never finalised).
        "hppa",
        "ppc" # If used by the abbreviation instead of "ppc32"
    }
    
    if arch.endswith("32") or arch in special_32_bit_archs:
        env.Append(CPPDEFINES=["IS_32_BIT"])