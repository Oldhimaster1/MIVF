import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("encode_mivf", ROOT / "encode_mivf.py")
enc = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = enc
spec.loader.exec_module(enc)

def test_parser_preserves_legacy_stream_defaults():
    ns = enc.build_parser().parse_args(["in.mkv", "out.mivf"])
    assert ns.video_stream == 0
    assert ns.audio_stream == 0

def test_parser_accepts_explicit_relative_stream_indexes():
    ns = enc.build_parser().parse_args(["in.mkv", "out.mivf", "--video-stream", "2", "--audio-stream", "3"])
    assert ns.video_stream == 2
    assert ns.audio_stream == 3

def test_ffmpeg_video_mapping_uses_selected_relative_index(monkeypatch, tmp_path):
    captured = {}
    class Dummy:
        stdout = object()
    monkeypatch.setattr(enc, "mivf_ffmpeg_path", lambda: "ffmpeg")
    monkeypatch.setattr(enc.subprocess, "Popen", lambda argv, **kw: captured.setdefault("argv", argv) or Dummy())
    settings = enc.EncodeSettings(video_stream=2)
    enc.start_ffmpeg_raw_pipe(tmp_path / "in.mkv", settings)
    i = captured["argv"].index("-map")
    assert captured["argv"][i+1] == "0:v:2"

def test_ffmpeg_audio_mapping_uses_selected_relative_index(monkeypatch, tmp_path):
    captured = {}
    class Dummy: pass
    monkeypatch.setattr(enc, "mivf_ffmpeg_path", lambda: "ffmpeg")
    def fake(argv, **kw):
        captured["argv"] = argv
        return Dummy()
    monkeypatch.setattr(enc.subprocess, "Popen", fake)
    enc.start_ffmpeg_audio_pipe(tmp_path / "in.mkv", 16000, 1, audio_stream=4)
    i = captured["argv"].index("-map")
    assert captured["argv"][i+1] == "0:a:4"

def test_recovery_fingerprint_includes_stream_selection(tmp_path):
    source = tmp_path / "in.mkv"
    source.write_bytes(b"source")
    a = enc.e0_settings_fingerprint(source, enc.EncodeSettings(video_stream=0, audio_stream=0))
    b = enc.e0_settings_fingerprint(source, enc.EncodeSettings(video_stream=1, audio_stream=2))
    assert a["video_stream"] == 0 and a["audio_stream"] == 0
    assert b["video_stream"] == 1 and b["audio_stream"] == 2
    assert a != b
