# License scope

HYPNOTIC-owned code in this source snapshot is released under GNU GPL version 3 (see LICENSE). Existing third-party notices and license terms are preserved and are not replaced by that declaration.

- OpenOrbis graphics/common code: GPLv3; originally credited to Specter.
- projectM-derived parsing, noise, wave, audio and final-composite code: LGPL-2.1; see projectM-LGPL-2.1.txt and assets/misc/projectM-components.txt.
- projectM-Eval: MIT; assets/misc/projectm-eval-LICENSE.md.
- OpenGNM, PSBC/Mesa, glslang and HLSLParser: upstream/per-file terms; assets/misc/THIRD_PARTY_NOTICES.txt and the pinned source repositories.
- dr_libs/stb: original notices retained in graphics/third_party and assets/misc/AUDIO-LICENSES.txt.
- Noto Sans JP Katakana masks: SIL OFL 1.1; graphics/third_party/NotoSansJP-OFL.txt. No font installer is bundled.
- Atkinson Hyperlegible, JetBrains Mono and Rajdhani font files and derived glyph masks: SIL OFL 1.1; see graphics/fonts and assets/licenses/fonts.
- Community MilkDrop presets and textures are separately copyrighted. The GPL license does not grant rights to them. Preset files are omitted from this source upload; provenance identifies the originals.

## Unresolved external dependency: libjbc

The development build uses bucanero/ps4-libjbc, revision 835fe016ff0ae5dd89b9249f39cc0fe093fd07dd, with local integration changes. That upstream revision contains no explicit license file. No libjbc source, binary or derived patch is included in this upload, and no GPL permission is asserted for it. Redistribution permission or a suitably licensed replacement must be resolved for a complete distributable build. Merely being publicly readable on GitHub does not resolve the missing license.
