"""Combines the catalog, source markers, call logs and exclusions into per-method status."""

from dataclasses import dataclass, replace

from .catalog import Catalog, Entry
from .errors import StatusError
from .markers import IMPLEMENTED, STUB, Marker

MISSING = "missing"
EXCLUDED = "excluded"
TESTED = "tested"
METRICS = (IMPLEMENTED, STUB, MISSING, TESTED)


@dataclass(frozen=True)
class MethodStatus:
    entry: Entry
    state: str
    is_tested: bool
    location: str | None
    exclusion_reason: str | None = None


@dataclass(frozen=True)
class GroupStatus:
    group: str
    methods: list[MethodStatus]

    def in_scope(self) -> list[MethodStatus]:
        return [method for method in self.methods if method.state != EXCLUDED]

    def count(self, metric: str) -> int:
        """Counts in-scope methods for a metric, or excluded methods for `excluded`."""
        if metric == EXCLUDED:
            return len(self.methods) - len(self.in_scope())
        if metric == TESTED:
            return sum(method.is_tested for method in self.in_scope())
        return sum(method.state == metric for method in self.in_scope())

    def counts(self) -> dict[str, int]:
        return {metric: self.count(metric) for metric in (*METRICS, EXCLUDED)}


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


def excluded_status(method: MethodStatus, reason: str) -> MethodStatus:
    if method.state == IMPLEMENTED:
        raise StatusError(
            f"apply exclusions: '{method.entry.key}' is implemented at {method.location} "
            f"but listed out of scope ({reason})"
        )
    return replace(method, state=EXCLUDED, exclusion_reason=reason)


def apply_exclusions(statuses: list[GroupStatus], exclusions: dict[str, str]) -> list[GroupStatus]:
    """Marks excluded methods; an excluded method may be stubbed but not implemented."""
    return [
        GroupStatus(
            status.group,
            [
                excluded_status(method, exclusions[method.entry.key])
                if method.entry.key in exclusions
                else method
                for method in status.methods
            ],
        )
        for status in statuses
    ]
