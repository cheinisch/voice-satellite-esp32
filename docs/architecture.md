# Architektur

`jarvis-satellite-esp32` ist ein separates Embedded-Repository.

```text
src/core + src/network + src/protocol
                 │
          Board Interface
        ┌────────┴────────┐
        │                 │
generic-esp32s3     waveshare-1.85c
```

Der gemeinsame Core darf keine konkreten GPIOs kennen. Neue Hardware wird als eigenes PlatformIO-Library-Verzeichnis unter `boards/` ergänzt.
