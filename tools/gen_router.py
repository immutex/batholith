#!/usr/bin/env python3
"""Generate router tables from the pinned vanilla worldgen JSON.

Source of truth: misode/mcmeta tag 1.21.4-data-json, the data generator output of
the official server jar. Re-run after bumping the pin:

    python tools/gen_router.py

Writes include/batholith/router_data.hxx and src/router_data.cxx.
"""

from __future__ import annotations

import json
import pathlib
import urllib.request

PIN = "1.21.4-data-json"
BASE = f"https://raw.githubusercontent.com/misode/mcmeta/{PIN}/data/minecraft/worldgen"
ROOT = pathlib.Path(__file__).resolve().parent.parent
CACHE = ROOT / ".spec"

NOISES = [
    "aquifer_barrier",
    "aquifer_fluid_level_floodedness",
    "aquifer_fluid_level_spread",
    "aquifer_lava",
    "calcite",
    "cave_cheese",
    "cave_entrance",
    "cave_layer",
    "continentalness",
    "erosion",
    "gravel",
    "ice",
    "jagged",
    "noodle",
    "noodle_ridge_a",
    "noodle_ridge_b",
    "noodle_thickness",
    "offset",
    "ore_gap",
    "ore_vein_a",
    "ore_vein_b",
    "ore_veininess",
    "packed_ice",
    "pillar",
    "pillar_rareness",
    "pillar_thickness",
    "powder_snow",
    "ridge",
    "spaghetti_2d",
    "spaghetti_2d_elevation",
    "spaghetti_2d_modulator",
    "spaghetti_2d_thickness",
    "spaghetti_3d_1",
    "spaghetti_3d_2",
    "spaghetti_3d_rarity",
    "spaghetti_3d_thickness",
    "spaghetti_roughness",
    "spaghetti_roughness_modulator",
    "surface",
    "surface_swamp",
    "temperature",
    "vegetation",
]

BLENDED = "base_3d_noise"

KIND = {
    "constant": "constant",
    "y_clamped_gradient": "y_clamped_gradient",
    "noise": "noise",
    "shifted_noise": "shifted_noise",
    "old_blended_noise": "old_blended_noise",
    "shift_a": "shift_a",
    "shift_b": "shift_b",
    "add": "add",
    "mul": "mul",
    "min": "min",
    "max": "max",
    "sub": "sub",
    "div": "div",
    "abs": "abs",
    "square": "square",
    "cube": "cube",
    "negate": "negate",
    "half_negative": "half_negative",
    "quarter_negative": "quarter_negative",
    "clamp": "clamp",
    "squeeze": "squeeze",
    "lerp": "lerp",
    "spline": "spline",
    "range_choice": "range_choice",
    "weird_scaled_sampler": "weird_scaled_sampler",
    "interpolated": "interpolated",
    "flat_cache": "flat_cache",
    "cache": "cache",
}

OUTPUTS = [
    "final_density",
    "fluid_level_floodedness",
    "fluid_level_spread",
    "lava",
    "temperature",
    "vegetation",
    "continents",
    "erosion",
    "depth",
]

UNARY = ("abs", "square", "cube", "negate", "half_negative", "quarter_negative", "squeeze",
         "flat_cache", "interpolated", "cache")
BINARY = ("add", "mul", "min", "max", "sub", "div")

# Fresh-world folds. blend_density only differs from its argument when new chunks
# border terrain from an older generator, and beardifier is driven by structures.
# batholith generates neither, so both collapse: blend_alpha and blend_offset are
# 0, and every lerp(blend_alpha, first, second) therefore yields first.
FOLD_IDENTITY = {"blend_density"}
FOLD_ZERO = {"beardifier", "blend_alpha", "blend_offset"}

# cache_2d / flat_cache key on (x, z); cache_once and cache key on the full position.
KIND_ALIAS = {"cache_2d": "flat_cache", "cache_once": "cache"}

RARITY_VARIANT = {"type_1": 1, "type_2": 2}


def fetch(rel: str) -> dict:
    CACHE.mkdir(exist_ok=True)
    local = CACHE / (rel.replace("/", "__") + ".json")
    if not local.exists():
        with urllib.request.urlopen(f"{BASE}/{rel}.json", timeout=30) as response:
            local.write_bytes(response.read())
    return json.loads(local.read_text())


def strip(name: str) -> str:
    return name.removeprefix("minecraft:")


class Builder:
    def __init__(self, noise_index: dict[str, int]) -> None:
        self.noise_index = noise_index
        self.nodes: list[dict] = []
        self.points: list[dict] = []
        self.by_name: dict[str, int] = {}

    def blank(self, kind: str) -> dict:
        self.nodes.append({"kind": kind, "children": [-1, -1, -1], "noise": -1,
                           "first_point": -1, "point_count": 0, "variant": 0,
                           "params": [0.0, 0.0, 0.0, 0.0]})
        return self.nodes[-1]

    def emit(self, node) -> int:
        if isinstance(node, bool):
            raise ValueError("boolean density function")
        if isinstance(node, (int, float)):
            slot = self.blank("constant")
            slot["params"][0] = float(node)
            return len(self.nodes) - 1

        kind_name = strip(node["type"])
        if kind_name in FOLD_IDENTITY:
            return self.ref(node["argument"])
        if kind_name in FOLD_ZERO:
            slot = self.blank("constant")
            return len(self.nodes) - 1

        kind = KIND[KIND_ALIAS.get(kind_name, kind_name)]
        slot = self.blank(kind)
        index = len(self.nodes) - 1

        if kind == "y_clamped_gradient":
            slot["params"] = [float(node["from_y"]), float(node["to_y"]),
                              float(node["from_value"]), float(node["to_value"])]
        elif kind == "noise":
            slot["noise"] = self.noise_index[strip(node["noise"])]
            slot["params"][0] = float(node["xz_scale"])
            slot["params"][1] = float(node["y_scale"])
        elif kind == "old_blended_noise":
            slot["noise"] = 0
        elif kind in ("shift_a", "shift_b"):
            slot["noise"] = self.noise_index["offset"]
        elif kind == "shifted_noise":
            slot["noise"] = self.noise_index[strip(node["noise"])]
            slot["params"][0] = float(node["xz_scale"])
            slot["params"][1] = float(node["y_scale"])
            slot["children"][0] = self.ref(node["shift_x"])
            slot["children"][1] = self.ref(node["shift_y"])
            slot["children"][2] = self.ref(node["shift_z"])
        elif kind in BINARY:
            slot["children"][0] = self.ref(node["argument1"])
            slot["children"][1] = self.ref(node["argument2"])
        elif kind in UNARY:
            slot["children"][0] = self.ref(node["argument"])
        elif kind == "clamp":
            slot["children"][0] = self.ref(node["input"])
            slot["params"][0] = float(node["min"])
            slot["params"][1] = float(node["max"])
        elif kind == "lerp":
            slot["children"][0] = self.ref(node["alpha"])
            slot["children"][1] = self.ref(node["first"])
            slot["children"][2] = self.ref(node["second"])
        elif kind == "range_choice":
            slot["children"][0] = self.ref(node["input"])
            slot["children"][1] = self.ref(node["when_in_range"])
            slot["children"][2] = self.ref(node["when_out_of_range"])
            slot["params"][0] = float(node["min_inclusive"])
            slot["params"][1] = float(node["max_exclusive"])
        elif kind == "weird_scaled_sampler":
            slot["children"][0] = self.ref(node["input"])
            slot["noise"] = self.noise_index[strip(node["noise"])]
            slot["variant"] = RARITY_VARIANT[node["rarity_value_mapper"]]
        elif kind == "spline":
            slot["children"][0] = self.ref(node["spline"]["coordinate"])
            slot["first_point"] = len(self.points)
            slot["point_count"] = self.walk_points(node["spline"]["points"])
        else:
            raise ValueError(f"unhandled kind {kind}")
        return index

    def walk_points(self, points) -> int:
        ordered = sorted(points, key=lambda p: float(p["location"]))
        first = len(self.points)
        for point in ordered:
            self.points.append({"location": float(point["location"]),
                                "derivative": float(point["derivative"]), "value": -1})
        for offset, point in enumerate(ordered):
            value = point["value"]
            if isinstance(value, dict) and "points" in value:
                child = self.emit({"type": "minecraft:spline", "spline": value})
            else:
                child = self.ref(value)
            self.points[first + offset]["value"] = child
        return len(ordered)

    def ref(self, node) -> int:
        if isinstance(node, str):
            name = strip(node)
            if name in self.by_name:
                return self.by_name[name]
            index = self.emit(fetch(f"density_function/{name}"))
            self.by_name[name] = index
            return index
        return self.emit(node)


def render_header() -> str:
    enumerators = "".join(f"    {name},\n" for name in OUTPUTS)
    return f"""#pragma once

#include <cstdint>
#include <span>

#include <batholith/density.hxx>

namespace batholith {{

struct noise_definition {{
    const char* name;
    bool blended;
    int first_octave;
    std::span<const double> amplitudes;
    double xz_scale;
    double y_scale;
    double xz_factor;
    double y_factor;
    double smear;
}};

struct router_data {{
    std::span<const noise_definition> noises;
    std::span<const density_node> nodes;
    std::span<const spline_point> points;
    std::span<const int> outputs;
}};

enum class router_output : std::uint8_t {{
{enumerators}}};

[[nodiscard]] const router_data& overworld_router_data() noexcept;

}}  // namespace batholith
"""


def render_source(noise_defs, builder, outputs) -> str:
    lines = ["#include <batholith/router_data.hxx>", "", "namespace batholith {",
             "namespace {", ""]
    for index, definition in enumerate(noise_defs):
        if definition.get("amplitudes") is not None:
            values = ", ".join(f"{value!r}" for value in definition["amplitudes"])
            lines.append(f"constexpr double amplitudes_{index}[] = {{{values}}};")
    lines.append("")

    lines.append("constexpr noise_definition noises[] = {")
    for index, definition in enumerate(noise_defs):
        amps = f"amplitudes_{index}" if definition.get("amplitudes") is not None else "{}"
        lines.append(
            f'    {{"{definition["name"]}", '
            f'{"true" if definition.get("blended") else "false"}, '
            f'{definition.get("first_octave", 0)}, {amps}, '
            f'{definition.get("xz_scale", 0.0)!r}, {definition.get("y_scale", 0.0)!r}, '
            f'{definition.get("xz_factor", 0.0)!r}, {definition.get("y_factor", 0.0)!r}, '
            f'{definition.get("smear", 0.0)!r}}},')
    lines.append("};")
    lines.append("")

    lines.append("constexpr density_node nodes[] = {")
    for node in builder.nodes:
        params = ", ".join(f"{value!r}" for value in node["params"])
        children = ", ".join(str(value) for value in node["children"])
        lines.append(f"    {{density_kind::{node['kind']}, {{{children}}}, {node['noise']}, "
                     f"{node['first_point']}, {node['point_count']}, {node['variant']}, "
                     f"{{{params}}}}},")
    lines.append("};")
    lines.append("")

    lines.append("constexpr spline_point points[] = {")
    for point in builder.points:
        lines.append(f"    {{{point['location']!r}, {point['derivative']!r}, {point['value']}}},")
    lines.append("};")
    lines.append("")

    lines.append("constexpr int outputs[] = {")
    for name in OUTPUTS:
        lines.append(f"    {outputs[name]},")
    lines.append("};")
    lines.append("")

    lines.append("constexpr router_data data{noises, nodes, points, outputs};")
    lines.append("")
    lines.append("}  // namespace")
    lines.append("")
    lines.append("const router_data& overworld_router_data() noexcept { return data; }")
    lines.append("")
    lines.append("}  // namespace batholith")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    settings = fetch("noise_settings/overworld")

    noise_index: dict[str, int] = {}
    noise_defs = []
    for name in NOISES:
        definition = fetch(f"noise/{name}")
        amplitudes = [float(a) for a in definition.get("amplitudes", [1.0])]
        noise_index[name] = len(noise_defs)
        noise_defs.append({"name": name, "first_octave": int(definition["firstOctave"]),
                           "amplitudes": amplitudes})

    blended = fetch(f"density_function/overworld/{BLENDED}")
    noise_defs.append({"name": BLENDED, "blended": True,
                       "xz_scale": float(blended["xz_scale"]),
                       "y_scale": float(blended["y_scale"]),
                       "xz_factor": float(blended["xz_factor"]),
                       "y_factor": float(blended["y_factor"]),
                       "smear": float(blended["smear_scale_multiplier"])})

    builder = Builder(noise_index)
    router = settings["noise_router"]
    outputs = {name: builder.ref(router[name]) for name in OUTPUTS}

    (ROOT / "include/batholith/router_data.hxx").write_text(render_header(), encoding="utf-8",
                                                            newline="\n")
    (ROOT / "src/router_data.cxx").write_text(render_source(noise_defs, builder, outputs),
                                              encoding="utf-8", newline="\n")
    normal = sum(1 for d in noise_defs if not d.get("blended"))
    print(f"noises={normal} normal + 1 blended, nodes={len(builder.nodes)}, "
          f"points={len(builder.points)}")


if __name__ == "__main__":
    main()
