#!/usr/bin/env bash
set -euo pipefail
ENVIRONMENT="${1:-waveshare-1_85c}"
pio run -e "$ENVIRONMENT" -t upload
