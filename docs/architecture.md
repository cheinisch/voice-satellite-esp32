# Architektur

`voice-satellite-esp32` ist ein separates Embedded-Repository.

```text
src/core + src/network + src/protocol
                 │
          Board Interface
        ┌────────┴────────┐
        │                 │
generic-esp32s3     waveshare-1.85c
```

Der gemeinsame Core darf keine konkreten GPIOs kennen. Neue Hardware wird als eigenes PlatformIO-Library-Verzeichnis unter `boards/` ergänzt.

## PlatformIO board isolation

Board profiles under `boards/` are exposed as PlatformIO libraries. Because the Library Dependency Finder can discover includes inside conditional preprocessor branches, each PlatformIO environment explicitly ignores all non-selected board libraries via `lib_ignore`. This keeps hardware-specific dependencies isolated and prevents a generic build from compiling Waveshare-only code (and vice versa).
