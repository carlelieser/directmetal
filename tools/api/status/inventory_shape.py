"""Checks that an inventory JSON document has the shape generate_inventory.py writes."""

TOP_LEVEL_FIELDS = ("functions", "interfaces")
INTERFACE_FIELDS = ("base", "members")


def member_problem(name: str, members: object) -> str | None:
    if not isinstance(members, list):
        return f"interface '{name}' 'members' must be a list"
    for index, member in enumerate(members):
        has_name = isinstance(member, dict) and isinstance(member.get("name"), str)
        if not has_name:
            return f"interface '{name}' member {index} needs a string 'name'"
    return None


def interface_problem(name: str, interface: object) -> str | None:
    if not isinstance(interface, dict):
        return f"interface '{name}' must be an object"
    for field in INTERFACE_FIELDS:
        if field not in interface:
            return f"interface '{name}' missing '{field}'"
    base = interface["base"]
    if base is not None and not isinstance(base, str):
        return f"interface '{name}' base must be a string or null"
    return member_problem(name, interface["members"])


def shape_problem(document: object) -> str | None:
    """Returns what is wrong with an inventory document, or None when it is well formed."""
    if not isinstance(document, dict):
        return "top level must be an object"
    for field in TOP_LEVEL_FIELDS:
        if field not in document:
            return f"missing '{field}'"
        if not isinstance(document[field], dict):
            return f"'{field}' must be an object"
    for name, interface in document["interfaces"].items():
        problem = interface_problem(name, interface)
        if problem is not None:
            return problem
    return None
