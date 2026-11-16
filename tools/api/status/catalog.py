"""The DirectX API surface, keyed as `Interface::Method` or a bare exported function name."""

import json
from dataclasses import dataclass
from pathlib import Path

from inventory.directx_parser import DIRECTX_GROUPS, ROOT_METHODS

from .errors import StatusError
from .inventory_shape import shape_problem

IMPLICIT_GROUP = "common"
SEPARATOR = "::"


@dataclass(frozen=True)
class Entry:
    group: str
    owner: str | None
    name: str

    @property
    def key(self) -> str:
        return self.name if self.owner is None else f"{self.owner}{SEPARATOR}{self.name}"


class Catalog:
    """Read-only after load_catalog builds it."""

    def __init__(self) -> None:
        self._entries: dict[str, list[Entry]] = {}
        self._members: dict[str, list[str]] = {}
        self._bases: dict[str, str | None] = {}
        self._keys: set[str] = set()

    def _add_group(self, group: str) -> None:
        self._entries.setdefault(group, [])

    def _add_interface(self, name: str, base: str | None) -> None:
        self._bases[name] = base
        self._members.setdefault(name, [])

    def _add(self, entry: Entry) -> None:
        self._entries.setdefault(entry.group, []).append(entry)
        self._keys.add(entry.key)
        if entry.owner is not None:
            self._members[entry.owner].append(entry.name)

    def groups(self) -> list[tuple[str, tuple[Entry, ...]]]:
        return [(group, tuple(entries)) for group, entries in self._entries.items()]

    def interface_keys(self, name: str) -> list[str] | None:
        """Returns the keys of the methods `name` declares, or None when it is no interface."""
        if name not in self._bases:
            return None
        return [f"{name}{SEPARATOR}{method}" for method in self._members[name]]

    def unknown_reason(self, key: str) -> str | None:
        """Returns why `key` names no API entry, or None when it does."""
        if key in self._keys:
            return None
        owner, separator, name = key.partition(SEPARATOR)
        if not separator:
            return f"unknown exported function '{key}'"
        if owner not in self._bases:
            return f"unknown interface '{owner}' in '{key}'"
        declaring = self._declaring_interface(self._bases[owner], name)
        if declaring is None:
            return f"'{owner}' declares no method '{name}'"
        return f"'{key}' is inherited; use '{declaring}{SEPARATOR}{name}'"

    def _declaring_interface(self, interface: str | None, name: str) -> str | None:
        while interface is not None:
            if name in self._members.get(interface, []):
                return interface
            interface = self._bases.get(interface)
        return None


def read_inventory(path: Path) -> dict:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise StatusError(f"load inventory {path}: {error}") from error
    problem = shape_problem(document)
    if problem is not None:
        raise StatusError(f"load inventory {path}: {problem}")
    return document


def add_group(catalog: Catalog, group: str, document: dict) -> None:
    catalog._add_group(group)
    for function in sorted(document["functions"]):
        catalog._add(Entry(group, None, function))
    for name in sorted(document["interfaces"]):
        interface = document["interfaces"][name]
        catalog._add_interface(name, interface["base"])
        for member in interface["members"]:
            catalog._add(Entry(group, name, member["name"]))


def load_catalog(inventory_dir: Path) -> Catalog:
    catalog = Catalog()
    for group in DIRECTX_GROUPS:
        add_group(catalog, group, read_inventory(inventory_dir / f"{group}.json"))
    for root, methods in ROOT_METHODS.items():
        catalog._add_interface(root, None)
        for method in methods:
            catalog._add(Entry(IMPLICIT_GROUP, root, method))
    return catalog
