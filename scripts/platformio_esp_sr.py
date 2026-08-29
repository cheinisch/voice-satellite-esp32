# Jarvis ESP32-S3 WakeNet model integration for pioarduino.
#
# Arduino-ESP32 3.3.x ships ESP_SR and a prebuilt srmodels.bin for ESP32-S3.
# When wake-word support is enabled in include/local_config.h, add that model
# binary as an extra flash image at the Jarvis model partition offset.

Import("env")

from pathlib import Path
import re

PIOENV = env.subst("$PIOENV")
if PIOENV == "waveshare-1_85c":
    project_dir = Path(env.subst("$PROJECT_DIR"))
    local_config = project_dir / "include" / "local_config.h"
    enabled = False

    if local_config.is_file():
        text = local_config.read_text(encoding="utf-8", errors="ignore")
        match = re.search(
            r"^\s*#\s*define\s+JARVIS_WAKEWORD_ENABLED\s+([^\s/]+)",
            text,
            re.MULTILINE,
        )
        if match:
            value = match.group(1).strip().lower()
            enabled = value not in ("0", "false", "off", "no")

    if enabled:
        platform = env.PioPlatform()
        libs_dir = platform.get_package_dir("framework-arduinoespressif32-libs")
        if not libs_dir:
            raise RuntimeError("Jarvis ESP-SR: framework-arduinoespressif32-libs nicht gefunden")

        model = Path(libs_dir) / "esp32s3" / "esp_sr" / "srmodels.bin"
        if not model.is_file():
            raise RuntimeError(
                "Jarvis ESP-SR: srmodels.bin fehlt: %s" % model
            )

        max_model_bytes = 0x3E0000
        model_size = model.stat().st_size
        if model_size > max_model_bytes:
            raise RuntimeError(
                "Jarvis ESP-SR: Modell ist zu gross (%d > %d Bytes)"
                % (model_size, max_model_bytes)
            )

        # Must run as a PRE extra_script: the Espressif32 uploader later turns
        # FLASH_EXTRA_IMAGES into one esptool write_flash command.
        env.Append(FLASH_EXTRA_IMAGES=[("0xC10000", str(model))])
        print(
            "Jarvis ESP-SR: Wakeword aktiv; srmodels.bin (%d Bytes) wird bei 0xC10000 mitgeflasht."
            % model_size
        )
    else:
        print("Jarvis ESP-SR: Wakeword aus; Modell-Upload wird uebersprungen.")
