"""E.1.2: Movie Information Authoring -- desktop metadata model.

Real player support audited (source/main.c) before writing this: the
player reads an optional ".nfo" sidecar (hfix60_load_nfo(), ~line 7478)
and displays it as up to two 19-character lines (38 characters total,
hard-truncated, no wrapping) in the file browser's preview panel
(g_hfix58_preview.synopsis1/synopsis2) -- NOT a dedicated "Movie
Information" screen, and nothing else (title/year/genre/rating/director/
cast) is loaded or displayed anywhere in the player. The "poster" sidecar
mentioned in the same comment is the existing .cover/.preview.cover
artwork, already handled by ProjectArtwork.cover/preview_cover -- not this
module's concern.

Scope of this phase, per explicit instruction: desktop model, authoring
UI, MediaProbe import, previews, and project persistence only. This
module never writes a .nfo file and never touches source/ -- exporting a
real sidecar from the `synopsis` field is a real, separately-scoped later
step (not yet implemented), so every synopsis-related preview here is
labelled as showing what the player *would* show if that export existed,
not a claim that it does anything today.

Only `synopsis` corresponds to any real, currently-loaded player data.
Every other field is desktop-only metadata for the user's own reference
(and future export/sharing), with no current runtime effect -- this is
stated explicitly in the UI, not left implicit.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field, asdict
from typing import Any

from .media_probe import MediaProbeResult

# Real runtime constants, cited from source/main.c's hfix60_load_nfo():
# clean[] is capped at 128 bytes; two display lines of 19 chars each are
# taken via snprintf("%.19s", ...) -- everything beyond the 38th collapsed
# character is silently dropped by the player, never wrapped further.
SYNOPSIS_LINE_CHARS = 19
SYNOPSIS_MAX_CHARS = SYNOPSIS_LINE_CHARS * 2

PRESENTATION_FIELDS = (
    "title", "original_title", "release_year", "displayed_runtime",
    "synopsis", "short_synopsis", "genre", "content_rating", "director",
    "cast_text", "production_studio", "languages", "custom_notes", "edition_name",
)

# Field -> ordered list of container tag names to try, most-specific first.
# Conservative and explicit: a field is only ever populated from a tag that
# actually exists in this list -- nothing is guessed or synthesized from
# unrelated tags.
_IMPORT_TAG_SOURCES: dict[str, tuple[str, ...]] = {
    "title": ("title", "show"),
    "genre": ("genre",),
    "release_year": ("date", "year", "creation_time"),
    "short_synopsis": ("description", "comment"),
    "synopsis": ("synopsis",),
    "production_studio": ("studio", "network"),
    "director": ("director",),
    "languages": ("language",),
}

_YEAR_RE = re.compile(r"(1[89]\d{2}|20\d{2})")


@dataclass
class MovieInformation:
    title: str | None = None
    original_title: str | None = None
    release_year: str | None = None
    displayed_runtime: str | None = None
    synopsis: str | None = None
    short_synopsis: str | None = None
    genre: str | None = None
    content_rating: str | None = None
    director: str | None = None
    cast_text: str | None = None
    production_studio: str | None = None
    languages: str | None = None
    custom_notes: str | None = None
    edition_name: str | None = None

    # Provenance per presentation field: "imported" | "manual". Absent key
    # means unset/never touched. Never applies to technical facts -- those
    # are never stored here at all (see module docstring / MediaProbeResult).
    field_provenance: dict[str, str] = field(default_factory=dict)

    # Last value imported per field, kept only to support "reset to source
    # value" without a second probe. Never displayed directly.
    last_import: dict[str, str] = field(default_factory=dict)
    last_import_source_sha256: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)

    @staticmethod
    def from_dict(d: dict[str, Any]) -> "MovieInformation":
        # Additive/forward-compatible: an old project (or one with no
        # movie_info key at all) supplies {} here and every field falls
        # back to its dataclass default -- no schema bump required.
        kwargs = {k: d.get(k) for k in PRESENTATION_FIELDS}
        kwargs["field_provenance"] = dict(d.get("field_provenance") or {})
        kwargs["last_import"] = dict(d.get("last_import") or {})
        kwargs["last_import_source_sha256"] = d.get("last_import_source_sha256")
        return MovieInformation(**kwargs)


def mark_manual(info: MovieInformation, field_name: str) -> None:
    """Call whenever the user hand-edits a field (e.g. on every UI change),
    so a later probe/import never silently overwrites their edit and
    'reset to source value' knows this field now has somewhere to reset
    *from* only if it was previously imported."""
    if field_name not in PRESENTATION_FIELDS:
        raise ValueError(f"not a presentation field: {field_name}")
    info.field_provenance[field_name] = "manual"


def _first_matching_tag(tags: dict[str, str], names: tuple[str, ...]) -> str | None:
    lower = {k.lower(): v for k, v in tags.items()}
    for name in names:
        value = lower.get(name)
        if value:
            return value
    return None


def _extract_year(raw_date_tag: str) -> str | None:
    m = _YEAR_RE.search(raw_date_tag)
    return m.group(1) if m else None


def import_from_probe(info: MovieInformation, probe: MediaProbeResult, *, overwrite: bool = False) -> list[str]:
    """Populates presentation fields from probe.container_tags -- only
    fields with a real, present tag are touched; nothing is invented.
    By default (overwrite=False) never clobbers a field the user has
    manually edited since its last import. Returns the list of field names
    actually populated, for UI transparency ("imported: title, genre").
    Mutates `info` in place -- callers own the working-copy/Cancel
    discipline (see movie_info_dialog.py)."""
    populated: list[str] = []
    tags = probe.container_tags
    for field_name, tag_names in _IMPORT_TAG_SOURCES.items():
        if not overwrite and info.field_provenance.get(field_name) == "manual":
            continue
        raw_value = _first_matching_tag(tags, tag_names)
        if not raw_value:
            continue
        value = raw_value.strip()
        if not value:
            continue
        if field_name == "release_year":
            year = _extract_year(value)
            if not year:
                continue
            value = year
        setattr(info, field_name, value)
        info.field_provenance[field_name] = "imported"
        info.last_import[field_name] = value
        populated.append(field_name)
    if populated:
        info.last_import_source_sha256 = probe.identity.sha256
    return populated


def reset_field_to_probed(info: MovieInformation, field_name: str) -> bool:
    """Restores a field to its last-imported value. Returns False (no-op)
    if this field was never imported -- callers should show 'no source
    value available for this field' rather than silently doing nothing."""
    if field_name not in info.last_import:
        return False
    setattr(info, field_name, info.last_import[field_name])
    info.field_provenance[field_name] = "imported"
    return True


# --- synopsis / text-fit validation (the one field with real runtime reach) -

def collapse_whitespace_ascii(text: str) -> str:
    """Mirrors hfix60_load_nfo()'s exact collapsing algorithm: any run of
    space/tab/CR/LF becomes one space, leading/collapsed-only text stays
    empty. Byte-for-byte behavior match, not an approximation, so the
    desktop preview shows exactly what the player would show."""
    out: list[str] = []
    prev_space = False
    for ch in text:
        if ch in (" ", "\t", "\r", "\n"):
            if not prev_space and out:
                out.append(" ")
                prev_space = True
        else:
            out.append(ch)
            prev_space = False
    return "".join(out)


def synopsis_preview(text: str | None) -> dict[str, Any]:
    """What the real player's browser preview panel would show for this
    synopsis text, if it were ever exported to a .nfo sidecar (not yet
    implemented) -- collapsed exactly like hfix60_load_nfo(), split into
    its real two 19-character lines, with an explicit truncation flag."""
    collapsed = collapse_whitespace_ascii(text or "")
    line1 = collapsed[:SYNOPSIS_LINE_CHARS]
    line2 = collapsed[SYNOPSIS_LINE_CHARS:SYNOPSIS_MAX_CHARS]
    truncated = len(collapsed) > SYNOPSIS_MAX_CHARS
    return {"line1": line1, "line2": line2, "truncated": truncated,
            "collapsed_length": len(collapsed), "max_length": SYNOPSIS_MAX_CHARS}


# --- desktop preview summaries ---------------------------------------------

def concise_summary(info: MovieInformation) -> str:
    parts = [info.title or "(untitled)"]
    if info.release_year:
        parts.append(f"({info.release_year})")
    if info.displayed_runtime:
        parts.append(f"- {info.displayed_runtime}")
    if info.genre:
        parts.append(f"- {info.genre}")
    return " ".join(parts)


def detailed_summary(info: MovieInformation) -> str:
    lines = [concise_summary(info)]
    if info.original_title and info.original_title != info.title:
        lines.append(f"Original title: {info.original_title}")
    if info.director:
        lines.append(f"Director: {info.director}")
    if info.cast_text:
        lines.append(f"Cast: {info.cast_text}")
    if info.production_studio:
        lines.append(f"Studio: {info.production_studio}")
    if info.content_rating:
        lines.append(f"Rating: {info.content_rating}")
    if info.languages:
        lines.append(f"Languages: {info.languages}")
    if info.edition_name:
        lines.append(f"Edition: {info.edition_name}")
    if info.short_synopsis:
        lines.append(f"\n{info.short_synopsis}")
    if info.synopsis:
        lines.append(f"\nSynopsis: {info.synopsis}")
    if info.custom_notes:
        lines.append(f"\nNotes: {info.custom_notes}")
    return "\n".join(lines)
