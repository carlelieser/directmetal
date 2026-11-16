import re

BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT = re.compile(r"//[^\n]*")
PREPROCESSOR_LINE = re.compile(r"^[ \t]*#[^\n]*(?:\\\n[^\n]*)*", re.MULTILINE)
IDENTIFIER = re.compile(r"[A-Za-z_]\w*")


def strip_comments(text: str) -> str:
    without_blocks = BLOCK_COMMENT.sub(" ", text)
    without_lines = LINE_COMMENT.sub("", without_blocks)
    return PREPROCESSOR_LINE.sub("", without_lines)


def matching_close(text: str, open_index: int, pair: str = "{}") -> int:
    opener, closer = pair
    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == opener:
            depth += 1
        elif text[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"unbalanced {pair} starting at offset {open_index}")


def block_after(text: str, start: int) -> tuple[str, int]:
    """Returns the body of the first {...} block at or after start, and the end offset."""
    open_index = text.index("{", start)
    close_index = matching_close(text, open_index)
    return text[open_index + 1 : close_index], close_index + 1


def remove_balanced_groups(text: str, pair: str = "()") -> str:
    opener, _ = pair
    pieces = []
    cursor = 0
    while (open_index := text.find(opener, cursor)) != -1:
        pieces.append(text[cursor:open_index])
        cursor = matching_close(text, open_index, pair) + 1
    pieces.append(text[cursor:])
    return "".join(pieces)


def remove_macro_calls(text: str, macro_pattern: re.Pattern) -> str:
    pieces = []
    cursor = 0
    while match := macro_pattern.search(text, cursor):
        open_index = text.index("(", match.end() - 1)
        pieces.append(text[cursor : match.start()])
        cursor = matching_close(text, open_index, "()") + 1
    pieces.append(text[cursor:])
    return "".join(pieces)


def enum_members(body: str) -> list[str]:
    members = []
    for entry in body.split(","):
        match = IDENTIFIER.search(entry)
        if match:
            members.append(match.group(0))
    return members


def unique(names: list[str]) -> list[str]:
    return list(dict.fromkeys(names))
