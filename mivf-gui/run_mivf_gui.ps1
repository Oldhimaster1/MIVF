$ErrorActionPreference = "Stop"
$GuiRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = Join-Path $GuiRoot ".venv\Scripts\python.exe"

if (-not (Test-Path -LiteralPath $Python)) {
    throw "MIVF GUI Python was not found: $Python"
}

& $Python -m mivf_gui
exit $LASTEXITCODE
