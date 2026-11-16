"""Threshold gates such as `d3d11:implemented>=90%` or `d3d11:missing<=0`."""

import operator
import re
from dataclasses import dataclass

from .errors import StatusError
from .report import METRICS, GroupStatus

REQUIREMENT = re.compile(
    r"(?P<group>\w+):(?P<metric>\w+)(?P<comparison>>=|<=)(?P<limit>\d+(?:\.\d+)?)(?P<percent>%?)"
)
COMPARISONS = {">=": operator.ge, "<=": operator.le}


@dataclass(frozen=True)
class Requirement:
    text: str
    group: str
    metric: str
    comparison: str
    limit: float
    is_percentage: bool


def parse_requirement(text: str) -> Requirement:
    match = REQUIREMENT.fullmatch(text)
    if match is None:
        raise StatusError(
            f"parse requirement '{text}': expected GROUP:METRIC>=N, <=N, >=N% or <=N%"
        )
    if match["metric"] not in METRICS:
        raise StatusError(f"parse requirement '{text}': metric must be one of {', '.join(METRICS)}")
    return Requirement(
        text,
        match["group"],
        match["metric"],
        match["comparison"],
        float(match["limit"]),
        match["percent"] == "%",
    )


def measured(requirement: Requirement, status: GroupStatus) -> float:
    count = status.count(requirement.metric)
    if not requirement.is_percentage:
        return count
    return 100 * count / len(status.methods) if status.methods else 0.0


def failures(requirements: list[Requirement], statuses: list[GroupStatus]) -> list[str]:
    """Returns one message per unmet requirement."""
    by_group = {status.group: status for status in statuses}
    messages = []
    for requirement in requirements:
        status = by_group.get(requirement.group)
        if status is None:
            raise StatusError(
                f"check requirement '{requirement.text}': unknown group '{requirement.group}'"
            )
        value = measured(requirement, status)
        if not COMPARISONS[requirement.comparison](value, requirement.limit):
            actual = f"{value:.1f}%" if requirement.is_percentage else f"{value:g}"
            messages.append(f"requirement unmet: {requirement.text} (actual {actual})")
    return messages
