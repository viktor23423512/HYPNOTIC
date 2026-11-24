The three additional UI fonts use the SIL Open Font License 1.1. The original font binaries and unmodified licences are included here; the app embeds rasterized glyph runs from `ui_font_data.h`.

Sources:
- Atkinson Hyperlegible: https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegible
- JetBrains Mono: https://github.com/JetBrains/JetBrainsMono
- Rajdhani: https://github.com/google/fonts/tree/main/ofl/rajdhani

Regenerate on Windows with Python, NumPy and `python tools/generate_ui_fonts.py`. Fonts are loaded privately only during generation and are never installed into Windows. The PS4 does not parse TTF files at runtime.
