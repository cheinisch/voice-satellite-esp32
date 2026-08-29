$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
$envName = "waveshare-1_85c"

Write-Host "=== BUILD ==="
& $pio run -e $envName

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build fehlgeschlagen."
    exit $LASTEXITCODE
}

Write-Host "=== UPLOAD ==="
& $pio run -e $envName -t upload

if ($LASTEXITCODE -ne 0) {
    Write-Error "Upload fehlgeschlagen."
    exit $LASTEXITCODE
}

Write-Host "=== SERIAL MONITOR ==="
& $pio device monitor -b 115200