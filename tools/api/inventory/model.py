from dataclasses import dataclass, field


@dataclass
class Member:
    name: str
    macos: str | None = None
    is_deprecated: bool = False


@dataclass
class Interface:
    name: str
    base: str | None
    source: str
    members: list[Member] = field(default_factory=list)


@dataclass
class Enumeration:
    name: str
    source: str
    members: list[str]


@dataclass
class Inventory:
    group: str
    interfaces: dict[str, Interface] = field(default_factory=dict)
    enums: dict[str, Enumeration] = field(default_factory=dict)
    structs: dict[str, str] = field(default_factory=dict)
    functions: dict[str, str] = field(default_factory=dict)

    def add_interface(self, interface: Interface) -> None:
        existing = self.interfaces.get(interface.name)
        if existing is None:
            self.interfaces[interface.name] = interface
            return
        known = {member.name for member in existing.members}
        existing.members.extend(m for m in interface.members if m.name not in known)
        existing.base = existing.base or interface.base

    def method_count(self) -> int:
        return sum(len(interface.members) for interface in self.interfaces.values())
