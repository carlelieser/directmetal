import re
from pathlib import Path

from .model import Enumeration, Interface, Inventory, Member
from .source_text import (
    block_after,
    enum_members,
    remove_balanced_groups,
    remove_macro_calls,
    strip_comments,
)

CONTAINER = re.compile(
    r"@(protocol|interface)\s+(\w+)\s*(?:\(\s*\w*\s*\))?\s*(?::\s*(\w+))?\s*(?:<([^>]*)>)?"
)
END = re.compile(r"@end\b")
NS_ENUM = re.compile(r"typedef\s+NS_(?:ENUM|OPTIONS|ERROR_ENUM)\s*\(\s*\w+\s*,\s*(\w+)\s*\)\s*\{")
MACRO_CALL = re.compile(r"\b(?:[A-Z][A-Z0-9_]+|__attribute__|__\w+)\s*\(")
TRAILING_MACRO = re.compile(r"(?:\s+[A-Z][A-Z0-9_]+)+\s*$")
SELECTOR_PART = re.compile(r"(\w+)\s*:")
LAST_IDENTIFIER = re.compile(r"(\w+)\s*$")
FIRST_IDENTIFIER = re.compile(r"(\w+)")
MACOS_VERSION = re.compile(r"\bmacos\(\s*([\d.]+)")
C_FUNCTION = re.compile(r"\bMTL_EXTERN\b[^;{}@]*?\b(MTL\w+)\s*\(")
IVAR_BLOCK = re.compile(r"\{[^{}]*\}")
FORWARD_DECLARATION = re.compile(r"\s*[;,]")


def selector(statement: str) -> str | None:
    signature = remove_balanced_groups(remove_macro_calls(statement, MACRO_CALL))
    signature = TRAILING_MACRO.sub("", signature.lstrip("-+ \t\n"))
    parts = SELECTOR_PART.findall(signature)
    if parts:
        return "".join(f"{part}:" for part in parts)
    match = FIRST_IDENTIFIER.search(signature)
    return match.group(1) if match else None


def property_name(statement: str) -> str | None:
    declaration = remove_macro_calls(statement[len("@property") :], MACRO_CALL)
    declaration = TRAILING_MACRO.sub("", remove_balanced_groups(declaration).rstrip())
    match = LAST_IDENTIFIER.search(declaration)
    return match.group(1) if match else None


def member_from(statement: str) -> Member | None:
    stripped = statement.strip()
    if stripped.startswith("@property"):
        name = property_name(stripped)
    elif stripped[:1] in {"-", "+"}:
        name = selector(stripped)
    else:
        return None
    if name is None:
        return None
    version = MACOS_VERSION.search(stripped)
    is_deprecated = "DEPRECATED" in stripped or "API_UNAVAILABLE(macos" in stripped
    return Member(name, version.group(1) if version else None, is_deprecated)


def container_members(body: str) -> list[Member]:
    members = []
    for statement in IVAR_BLOCK.sub(";", body).split(";"):
        member = member_from(statement)
        if member is not None:
            members.append(member)
    return members


def parse_containers(text: str, source: str, inventory: Inventory) -> None:
    for match in CONTAINER.finditer(text):
        header_end = match.end()
        if FORWARD_DECLARATION.match(text, header_end):
            continue
        end = END.search(text, header_end)
        if end is None:
            raise ValueError(f"unterminated @{match.group(1)} {match.group(2)} in {source}")
        _, name, superclass, protocols = match.groups()
        base = superclass or (protocols.split(",")[0].strip() if protocols else None)
        members = container_members(text[header_end : end.start()])
        inventory.add_interface(Interface(name, base, source, members))


def parse_metal_file(path: Path, framework: str, inventory: Inventory) -> None:
    text = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    source = f"{framework}/{path.name}"
    parse_containers(text, source, inventory)
    for match in NS_ENUM.finditer(text):
        body, _ = block_after(text, match.start())
        cleaned = remove_macro_calls(body, MACRO_CALL)
        inventory.enums.setdefault(
            match.group(1), Enumeration(match.group(1), source, enum_members(cleaned))
        )
    for function in C_FUNCTION.findall(text):
        inventory.functions.setdefault(function, source)


def parse_metal_group(group_dir: Path) -> Inventory:
    inventory = Inventory(group_dir.name)
    for framework_dir in sorted(path for path in group_dir.iterdir() if path.is_dir()):
        for path in sorted(framework_dir.glob("*.h")):
            parse_metal_file(path, framework_dir.name, inventory)
    return inventory
