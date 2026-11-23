"""Decodes tightly packed hex readbacks into per-pixel channel values."""

import math
import struct
from dataclasses import dataclass

from sources import ReferenceToolError


@dataclass(frozen=True)
class PixelLayout:
    # struct format of one pixel, little-endian.
    packing: str
    # Bit widths of channels packed into one integer, lowest bits first; empty when each struct
    # field is a channel.
    bit_fields: tuple[int, ...] = ()
    # struct format reading the same bytes as unsigned integers; set for float formats.
    bit_packing: str | None = None

    @property
    def size(self) -> int:
        return struct.calcsize(self.packing)


PIXEL_LAYOUTS = {
    "R8G8B8A8_UNORM": PixelLayout("<4B"),
    "R8G8B8A8_UNORM_SRGB": PixelLayout("<4B"),
    "B8G8R8A8_UNORM": PixelLayout("<4B"),
    "R10G10B10A2_UNORM": PixelLayout("<I", (10, 10, 10, 2)),
    "R16G16B16A16_FLOAT": PixelLayout("<4e", bit_packing="<4H"),
    "R32G32B32A32_FLOAT": PixelLayout("<4f", bit_packing="<4I"),
    "R32G32B32A32_UINT": PixelLayout("<4I"),
}

Pixel = tuple[float | int, ...]


def layout_of(format_name: str) -> PixelLayout:
    layout = PIXEL_LAYOUTS.get(format_name)
    if layout is None:
        raise ReferenceToolError(f"no pixel layout for format {format_name}")
    return layout


def unpack_bit_fields(value: int, bit_fields: tuple[int, ...]) -> Pixel:
    channels = []
    for width in bit_fields:
        channels.append(value & ((1 << width) - 1))
        value >>= width
    return tuple(channels)


def unpack(format_name: str, readback: str, is_bit_pattern: bool) -> list[Pixel]:
    layout = layout_of(format_name)
    try:
        data = bytes.fromhex(readback)
    except ValueError as error:
        raise ReferenceToolError(f"readback of format {format_name} is not hex: {error}") from error
    if len(data) % layout.size != 0:
        raise ReferenceToolError(
            f"readback of format {format_name} has {len(data)} bytes, "
            f"not a multiple of the {layout.size}-byte pixel"
        )
    packing = layout.bit_packing if is_bit_pattern and layout.bit_packing else layout.packing
    pixels = list(struct.iter_unpack(packing, data))
    if layout.bit_fields:
        return [unpack_bit_fields(fields[0], layout.bit_fields) for fields in pixels]
    return pixels


def decode_pixels(format_name: str, readback: str) -> list[Pixel]:
    """Decodes channel values: integers, or floats for float formats."""
    return unpack(format_name, readback, False)


def bit_patterns(format_name: str, readback: str) -> list[Pixel]:
    """Decodes channels as unsigned integers, so -0.0 and +0.0, or two NaN payloads, differ."""
    return unpack(format_name, readback, True)


def channel_difference(expected: float | int, actual: float | int) -> float:
    is_both_nan = isinstance(expected, float) and math.isnan(expected) and math.isnan(actual)
    if is_both_nan or expected == actual:
        return 0.0
    if math.isnan(expected) or math.isnan(actual):
        return math.inf
    return abs(expected - actual)
