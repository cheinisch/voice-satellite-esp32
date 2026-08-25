# Versionierung und Builds

`jarvis-satellite-esp32` verwendet zwei unabhängige Werte:

- `VERSION`: SemVer der Firmware, z. B. `0.1.0`
- `BUILD`: global fortlaufende numerische Buildnummer, z. B. `42`

Beide Dateien liegen im Repository-Root und sind die einzige Quelle für diese Werte. Beim PlatformIO-Build liest `scripts/platformio_version.py` beide Dateien und setzt:

```cpp
JARVIS_SATELLITE_VERSION
JARVIS_SATELLITE_BUILD
```

Dadurch erscheinen Version und Build in der seriellen Startausgabe und im `jarvis.voice.v1`-Handshake, ohne dass `include/build_info.h` manuell synchronisiert werden muss.

## Normale Builds

`.github/workflows/build-number.yml` reagiert auf Pushes, führt den eigentlichen Build aber nur auf dem Default-Branch aus.

Bei einem normalen Push auf den Default-Branch:

1. `BUILD` wird um eins erhöht.
2. GitHub Actions committet die neue `BUILD`-Datei zurück.
3. `generic-esp32s3` wird kompiliert.
4. `waveshare-1_85c` wird kompiliert.

Pull Requests werden durch `.github/workflows/build.yml` validiert, verändern aber keine Versions- oder Builddateien.

Der Buildnummer-Workflow benötigt `contents: write`. Branch Protection/Rulesets müssen den Bot-Commit erlauben, wenn der Default-Branch geschützt ist.

## Release erstellen

Releases werden nicht mehr zuerst manuell unter **GitHub Releases** angelegt. Stattdessen wird im Repository ausgeführt:

```text
Actions
→ Create firmware release
→ Run workflow
```

Der Workflow bietet folgende Auswahl:

```text
patch   0.1.0 → 0.1.1
minor   0.1.0 → 0.2.0
major   0.1.0 → 1.0.0
custom  explizite höhere MAJOR.MINOR.PATCH-Version
```

Optional kann das erzeugte GitHub Release als `Prerelease` markiert werden.

`custom` akzeptiert bewusst nur SemVer im Format `MAJOR.MINOR.PATCH` und muss größer als die aktuelle Version sein.

## Release-Ablauf

`.github/workflows/create-release.yml` führt den kompletten Release-Prozess aus:

1. Default-Branch auschecken.
2. Neue Version aus `VERSION` berechnen.
3. `BUILD` um eins erhöhen.
4. Prüfen, dass `v<VERSION>` noch nicht existiert.
5. Generic-Firmware kompilieren.
6. Waveshare-Firmware kompilieren.
7. Waveshare-Releasepaket und Prüfsummen erstellen.
8. `VERSION` und `BUILD` als Release-Commit speichern.
9. annotierten Tag `v<VERSION>` auf genau diesem Commit erstellen.
10. Commit und Tag pushen.
11. GitHub Release mit automatisch generierten Release Notes erstellen.
12. Firmwaredateien direkt als Release Assets hochladen.

Der Firmware-Build findet **vor** dem Push des Release-Commits statt. Ein Compiler-/Packaging-Fehler verändert deshalb weder den Default-Branch noch den Release-Tag.

Ein Release-Commit erhöht `BUILD` selbst. Der Push dieses Commits mit dem GitHub-Token soll keinen zweiten Buildnummer-Lauf erzeugen.

## Release-Artefakte

Erzeugt werden unter anderem:

- `...-app.bin` – reine Applikations-Firmware
- `...-factory.bin` – zusammengeführtes Image ab Flash-Offset `0x0`
- `...zip` – Paket mit Bootloader, Partitionstabelle, App, Factory-Image, Manifest und Flash-Hinweisen
- `SHA256SUMS.txt`

Beispiel:

```text
jarvis-satellite-esp32-waveshare-esp32-s3-touch-lcd-1.85c-v0.2.0-build47-app.bin
jarvis-satellite-esp32-waveshare-esp32-s3-touch-lcd-1.85c-v0.2.0-build47-factory.bin
jarvis-satellite-esp32-waveshare-esp32-s3-touch-lcd-1.85c-v0.2.0-build47.zip
SHA256SUMS.txt
```

## GitHub-Berechtigungen

Die Workflows für Buildnummer und Release verwenden:

```yaml
permissions:
  contents: write
```

Falls für den Default-Branch Branch Protection oder ein Ruleset aktiv ist, muss GitHub Actions die Release- und Buildnummer-Commits sowie das Erstellen von Tags durchführen dürfen.
