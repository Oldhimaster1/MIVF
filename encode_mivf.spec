# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['encode_mivf.py'],
    pathex=[],
    binaries=[('C:/msys64/ucrt64/bin/ffmpeg.exe', '.'), ('C:/dev/MIVF/miv2y_moflex_tier.exe', '.'), ('C:/dev/MIVF/tools/m2y2_transcode.exe', 'tools')],
    datas=[],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='encode_mivf',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    contents_directory='.',
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='encode_mivf',
)
