"""Combines the catalog, source markers and call logs into per-method status."""

from dataclasses import dataclass

from .catalog import Catalog, Entry
from .markers import IMPLEMENTED, STUB, Marker

MISSING = "missing"
STATES = (IMPLEMENTED, STUB, MISSING)
TESTED = "tested"
METRICS = (*STATES, TESTED)


@dataclass(frozen=True)
class MethodStatus:
    entry: Entry
    state: str
    is_tested: bool
    location: str | None


@dataclass(frozen=True)
class GroupStatus:
    group: str
    methods: list[MethodStatus]

    def count(self, metric: str) -> int:
        if metric == TESTED:
            return sum(method.is_tested for method in self.methods)
        return sum(method.state == metric for method in self.methods)

    def counts(self) -> dict[str, int]:
        return {metric: self.count(metric) for metric in METRICS}


def strongest_markers(markers: list[Marker]) -> dict[str, Marker]:
    """Keeps one marker per key; any stub marker outranks implementations of the same key."""
    chosen: dict[str, Marker] = {}
    for marker in markers:
        current = chosen.get(marker.key)
        if current is None or (marker.kind == STUB and current.kind != STUB):
            chosen[marker.key] = marker
    return chosen


def method_status(entry: Entry, marker: Marker | None, calls: set[str]) -> MethodStatus:
    is_tested = entry.key in calls
    if marker is None:
        return MethodStatus(entry, MISSING, is_tested, None)
    return MethodStatus(entry, marker.kind, is_tested, marker.location)


def build_report(catalog: Catalog, markers: list[Marker], calls: set[str]) -> list[GroupStatus]:
    chosen = strongest_markers(markers)
    return [
        GroupStatus(
            group, [method_status(entry, chosen.get(entry.key), calls) for entry in entries]
        )
        for group, entries in catalog.groups()
    ]
