#!/usr/bin/env bash
set -e
GUI_ROOT="$(cd "$(dirname "$0")" && pwd)"
exec "$GUI_ROOT/.venv/Scripts/python.exe" -m mivf_gui
