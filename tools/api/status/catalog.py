"""The DirectX API surface, keyed as `Interface::Method` or a bare exported function name."""

import json
from dataclasses import dataclass, field
from pathlib import Path

from inventory.directx_parser import DIRECTX_GROUPS, ROOT_METHODS

from .errors import StatusError

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


@dataclass
class Catalog:
    entries: dict[str, list[Entry]] = field(default_factory=dict)
    members: dict[str, list[str]] = field(default_factory=dict)
    bases: dict[str, str | None] = field(default_factory=dict)
    keys: set[str] = field(default_factory=set)

    def add(self, entry: Entry) -> None:
        self.entries.setdefault(entry.group, []).append(entry)
        self.keys.add(entry.key)
        if entry.owner is not None:
            self.members.setdefault(entry.owner, []).append(entry.name)

    def unknown_reason(self, key: str) -> str | None:
        """Returns why `key` names no API entry, or None when it does."""
        if key in self.keys:
            return None
        owner, separator, name = key.partition(SEPARATOR)
        if not separator:
            return f"unknown exported function '{key}'"
        if owner not in self.bases:
            return f"unknown interface '{owner}' in '{key}'"
        declaring = self.declaring_interface(self.bases[owner], name)
        if declaring is None:
            return f"'{owner}' declares no method '{name}'"
        return f"'{key}' is inherited; use '{declaring}{SEPARATOR}{name}'"

    def declaring_interface(self, interface: str | None, name: str) -> str | None:
        while interface is not None:
            if name in self.members.get(interface, []):
                return interface
            interface = self.bases.get(interface)
        return None


def read_inventory(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise StatusError(f"load inventory {path}: {error}") from error


def add_group(catalog: Catalog, group: str, document: dict) -> None:
    for function in sorted(document["functions"]):
        catalog.add(Entry(group, None, function))
    for name in sorted(document["interfaces"]):
        interface = document["interfaces"][name]
        catalog.bases[name] = interface["base"]
        for member in interface["members"]:
            catalog.add(Entry(group, name, member["name"]))


def load_catalog(inventory_dir: Path) -> Catalog:
    catalog = Catalog()
    for group in DIRECTX_GROUPS:
        add_group(catalog, group, read_inventory(inventory_dir / f"{group}.json"))
    for root, methods in ROOT_METHODS.items():
        catalog.bases[root] = None
        for method in methods:
            catalog.add(Entry(IMPLICIT_GROUP, root, method))
    return catalog
