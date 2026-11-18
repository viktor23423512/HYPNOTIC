# Building the 0.77 source snapshot

This publishes the player and renderer source used for 0.77, with the shared OpenOrbis sample files moved inside the repository. It is not yet a validated one-command build from a fresh checkout.

## Prerequisites

The original development build used Windows, OpenOrbis PS4Toolchain, LLVM 23.1.4, Python 3, Git, CMake, Ninja, GNU make and the Python packages used by Mesa code generation. Install prerequisites yourself; this repository does not install tools or modify the SDK.

The version-pinned source dependencies are listed in ../source-dependencies.json. Run `python tools/fetch_source_dependencies.py` to list them, or add `--fetch` to retrieve them into the expected directories. The PSBC patch preserves the tracked local compiler changes. Dependencies retain their original licenses.

## Current build limitations

1. libjbc licensing remains unresolved. Its source and local patch are deliberately omitted; see ../LICENSES/README.md. The unchanged filesystem integration still references it.
2. No SDK/system module binaries, prebuilt libraries, tool executables, sample music, community preset pack or reference-image test assets are bundled. Obtain any runtime modules from your own appropriate OpenOrbis installation. Community content needs its own redistribution review.
3. The archived dependency build helpers still refer to the original project-local `build-tools` layout. Those tools are not included. They also need the OpenOrbis compatibility/code-generation setup used by PSBC. Making that setup reproducible from a clean checkout is outstanding.
4. The root Makefile from the original sample was stale and is not included as a supported build route. `graphics/build.bat` is the actual 0.77 player build recipe.

## Original build order

After resolving the above dependencies: build OpenGNM, then PSBC, projectM-Eval, HLSLParser and glslang static libraries. The historical Python helpers are in tests/milkdrop_ps4. Check the glslang output directory against the archive paths in graphics/build.bat; the development helper used a relocated build directory. Supply the runtime assets and generated compatibility headers before packaging.

Set `OO_PS4_TOOLCHAIN` to your SDK root and put LLVM on PATH. From graphics, the original invocation pattern is `build.bat obj-integration HYPNOTIC ..`. The script compiles the player, signs the executable and builds the PKG; it must not be treated as ready to run until the missing build dependencies above have been supplied.

No clean-checkout compile or PS4 retest has been claimed for this source export. Runtime rendering/audio logic was not changed by source publication. Release 0.77 also has an unresolved report of the console powering off when closing the application; a causal crash log has not been captured.
