import re
from pathlib import Path

from .model import Enumeration, Interface, Inventory, Member
from .source_text import block_after, enum_members, strip_comments, unique

DIRECTX_GROUPS = ["d3d9", "d3d10", "d3d11", "d3d12", "dxgi", "common"]
CPP_QUOTE = re.compile(r'cpp_quote\("((?:[^"\\]|\\.)*)"\)')
DECLARE_INTERFACE = re.compile(
    r"DECLARE_INTERFACE(?:_IID)?_\(\s*(\w+)\s*,\s*(\w+)\s*(?:,\s*\"[^\"]*\")?\s*\)"
)
MIDL_INTERFACE = re.compile(r'MIDL_INTERFACE\("[^"]*"\)\s*(\w+)\s*:\s*public\s+(\w+)\s*\{')
IDL_INTERFACE = re.compile(r"\binterface\s+(\w+)\s*(?::\s*(\w+)\s*)?\{")
STDMETHOD = re.compile(r"STDMETHOD_?\(\s*(?:[^,()]+,\s*)?(\w+)\s*\)")
CPP_METHOD = re.compile(r"STDMETHODCALLTYPE\s+(\w+)\s*\(")
IDL_METHOD = re.compile(r"(\w+)\s*\(")
TYPEDEF_ENUM = re.compile(r"typedef\s+enum\s+(\w+)?\s*\{")
TYPEDEF_STRUCT = re.compile(r"typedef\s+struct\s+(\w+)?\s*\{")
EXPORTED_FUNCTION = re.compile(r"\b(?:WINAPI|__stdcall)\s+(\w+)\s*\(")
ROOT_METHODS = {"IUnknown": ["QueryInterface", "AddRef", "Release"]}
TRAILING_NAME = re.compile(r"\s*(\w+)")


def unquote_idl(text: str) -> str:
    return CPP_QUOTE.sub(lambda match: match.group(1).replace('\\"', '"'), text)


def idl_method_names(body: str) -> list[str]:
    names = []
    for statement in body.split(";"):
        without_attributes = re.sub(r"\[[^\]]*\]", " ", statement)
        match = IDL_METHOD.search(without_attributes)
        if match:
            names.append(match.group(1))
    return names


def interface_methods(style: str, body: str) -> list[str]:
    if style == "declare":
        return STDMETHOD.findall(body)
    if style == "midl":
        return CPP_METHOD.findall(body)
    return idl_method_names(body)


def parse_interfaces(text: str, source: str, inventory: Inventory) -> None:
    styles = {
        "declare": DECLARE_INTERFACE,
        "midl": MIDL_INTERFACE,
        "idl": IDL_INTERFACE,
    }
    for style, pattern in styles.items():
        for match in pattern.finditer(text):
            name, base = match.group(1), match.group(2)
            body, _ = block_after(text, match.start())
            members = [Member(method) for method in unique(interface_methods(style, body))]
            inventory.add_interface(Interface(name, base, source, members))


def typedef_name(text: str, block_end: int, tag: str | None) -> str | None:
    match = TRAILING_NAME.match(text, block_end)
    if match:
        return match.group(1)
    return tag


def parse_enums_and_structs(text: str, source: str, inventory: Inventory) -> None:
    for match in TYPEDEF_ENUM.finditer(text):
        body, end = block_after(text, match.start())
        name = typedef_name(text, end, match.group(1))
        if name and name not in inventory.enums:
            inventory.enums[name] = Enumeration(name, source, enum_members(body))
    for match in TYPEDEF_STRUCT.finditer(text):
        _, end = block_after(text, match.start())
        name = typedef_name(text, end, match.group(1))
        if name and not name.endswith("Vtbl"):
            inventory.structs.setdefault(name, source)


def parse_directx_file(path: Path, inventory: Inventory) -> None:
    raw = path.read_text(encoding="utf-8", errors="replace")
    text = strip_comments(unquote_idl(raw))
    source = path.name
    parse_interfaces(text, source, inventory)
    parse_enums_and_structs(text, source, inventory)
    for function in EXPORTED_FUNCTION.findall(text):
        inventory.functions.setdefault(function, source)


def ancestor_methods(inventory: Inventory, base: str | None) -> set[str]:
    names: set[str] = set()
    while base is not None:
        names.update(ROOT_METHODS.get(base, []))
        parent = inventory.interfaces.get(base)
        if parent is None:
            break
        names.update(member.name for member in parent.members)
        base = parent.base
    return names


def drop_inherited_methods(inventory: Inventory) -> None:
    inherited = {
        name: ancestor_methods(inventory, interface.base)
        for name, interface in inventory.interfaces.items()
    }
    for name, interface in inventory.interfaces.items():
        interface.members = [
            member for member in interface.members if member.name not in inherited[name]
        ]


def parse_directx_group(group_dir: Path) -> Inventory:
    inventory = Inventory(group_dir.name)
    for path in sorted(group_dir.iterdir()):
        if path.suffix in {".h", ".idl"}:
            parse_directx_file(path, inventory)
    drop_inherited_methods(inventory)
    return inventory
