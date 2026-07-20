from mivf_gui.theme_plan import _manifest_text  # Phase C.5a: manifest-building moved here, shared with PackagePlan


def test_runtime_manifest_uses_phase_c_grammar_and_bare_asset_names():
    project = {"theme": {"accent_rgb": [70, 120, 210], "outline_rgb": [255, 255, 255]}}
    names = {
        "dashboard_bg": "anime.dashboard_bg.mivfasset",
        "rewind": "anime.rewind.mivfasset",
        "play_pause": "anime.play_pause.mivfasset",
        "fast_forward": "anime.fast_forward.mivfasset",
        # Phase C.3.1: role key must be "movie_menu_back" to match
        # theme_export_c3.py's LEG dict / control_recipe.py's SPECS -- see
        # theme_export.py's _manifest_text() controls tuple comment.
        "movie_menu_back": "anime.menu_back.mivfasset",
    }
    manifest = _manifest_text(project, names)
    assert "MIVFTHEME_SCHEMA=1" in manifest
    assert "PALETTE_ACCENT=70,120,210" in manifest
    assert "PALETTE_OUTLINE=255,255,255" in manifest
    assert "DASHBOARD_BG=anime.dashboard_bg" in manifest
    assert ".mivfasset" not in manifest
    assert "ACCENT_R=" not in manifest
    assert "REWIND_IDLE_ASSET=" not in manifest
    for control in ("REWIND", "PLAY_PAUSE", "FAST_FORWARD", "BACK"):
        assert f"CONTROL={control}" in manifest
    assert manifest.count("CONTROL.END") == 8
    assert "CONTROL.STATE=IDLE" in manifest
    assert "CONTROL.STATE=FOCUSED" in manifest
