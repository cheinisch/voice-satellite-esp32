# Neues ESP32-Board hinzufügen

1. Neues Verzeichnis unter `boards/<profil>/` anlegen.
2. `library.json` hinzufügen.
3. Eine Klasse von `Board` ableiten.
4. Eine `AudioIO`-Implementierung bereitstellen.
5. Das Board in `src/board_factory.cpp` hinter einem Build-Define registrieren.
6. Neues PlatformIO-Environment ergänzen.

Hardwaredetails bleiben vollständig im Board-Verzeichnis.
