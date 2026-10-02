"""Write the helicopter paint channel assets and the script manifest that lists them.

A material is shared by every entity that names it, so recolouring a stock hull
material recolours every helicopter of that type. A paint channel is a prefab
variant ("twin") of a stock airframe whose hull and slotted parts name their own
copies of the paint materials; the game recolours a channel's copies and only
that helicopter changes. The copies inherit the stock materials unchanged, so a
twin looks stock until IA_HeliSkinPaint sets a livery's colours on it. Each hull
twin also carries IA_HeliPaintRigComponent, which tells the server which
channels are in use.

Everything comes from a registry, tools/heli_paint_families.json: one family
per helicopter type with its stock airframes, the parts that show paint, the
paint materials ("surfaces") and, for each surface, which parameters a livery
sets. The registry is data about prefabs, vanilla or modded; a family is added
from a survey written by the Workbench plugin IA_HeliPaintSurvey.

A surface's "paint" says what a livery does to each parameter it names:

    {"primary": gain}    a colour layer takes the livery's colour (MatPBRMulti)
    {"tint": [r, g, b]}  the material multiplies its texture by the parameter, and
                         the paint is painted into that texture: [r, g, b] is the
                         texture's own paint colour (linear), and the parameter is
                         set to the livery's colour over it (MatPBRBasic "Color")
    {"color": [r, g, b]} the same colour under every livery, such as trim
    {"value": n}         the same number under every livery

    python tools/author_heli_paint_channels.py            write the files
    python tools/author_heli_paint_channels.py --check    fail when a file is stale
    python tools/author_heli_paint_channels.py --adopt survey.json --family key
        [--name "UH-1H IROQUOIS"] [--stock-paint "Factory Olive"] [--art huey]
        [--surfaces Stem,Stem] [--parts Slot,Slot] [--textures <folder>]
                                                           add or refresh a family
    --textures <folder>  where the helicopter's texture files lie unpacked, so the
                         paint colour of a tinted surface is measured (needs Pillow
                         and lz4); without it the draft holds a placeholder
    --registry <file>  another registry (a compatibility addon keeps its own)
    --root <folder>    the addon the files are written into (default: this one)

After writing, open Workbench once so it imports the files, then run the
IA_HeliSkinAssetCheck plugin. docs/transport-pilot-progression.md has the whole
procedure for adding a helicopter.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = "tools/heli_paint_families.json"

MESH_CLASS = "MeshObject"
SLOT_CLASS = "SlotManagerComponent"
RESOURCE_CLASSES = {".emat": "EMATResourceClass", ".et": "EntityTemplateResourceClass"}
PLATFORMS = ("XBOX_ONE", "XBOX_SERIES", "PS4", "PS5", "HEADLESS")
# A file this tool wrote, by name; anything else in an output folder is left alone.
GENERATED = re.compile(r"^IA_.+_Paint\d+\.(et|emat)(\.meta)?$")
# Material classes whose paint is a set of colour layers a livery can recolour.
LAYERED_CLASSES = ("MatPBRMulti",)
# Material classes that multiply one texture by a colour: class -> (the colour, the texture).
TINTED_CLASSES = {"MatPBRBasic": ("Color", "BCRMap")}
NOT_PAINT = re.compile(r"glass|decal|light|lamp|detail", re.IGNORECASE)
# Paintable classes are used all over a helicopter, so a guess at its hull paint goes by name:
# the materials named like a hull, else the ones not named like something else.
HULL = re.compile(r"exterior|body|hull|fuselage|airframe", re.IGNORECASE)
NOT_HULL = re.compile(r"interior|cockpit|cargo|seat|tire|tyre|wheel|sensor|control|dial|console|lens|optic|rotor|blade", re.IGNORECASE)
# Draft paint colour of a tinted surface whose texture could not be measured: a dark olive.
UNMEASURED_PAINT = [0.03, 0.03, 0.02]
# The mip measured for a texture's paint colour: small enough to be one compressed chunk.
MEASURE_SIDE = 64
# Colours this many sRGB steps apart or closer count as one when looking for the commonest.
PAINT_BIN = 4


class RegistryError(Exception):
    pass


def path_of(resource):
    """A resource name without its GUID."""
    if resource.startswith("{"):
        return resource[resource.index("}") + 1:]
    return resource


def guid_of(resource):
    if not resource.startswith("{"):
        raise RegistryError("resource name without a GUID: " + resource)
    return resource[1:resource.index("}")]


def twin_stem(entry, key):
    """File name a twin of this stock file is built on: its own name unless the registry gives one."""
    return entry.get("name") or Path(path_of(entry[key])).stem


def twin_guid(family, kind, index, channel, path):
    """GUID of a generated file. The first family keeps the numbered GUIDs it shipped with."""
    prefix = family.get("legacy_guid_prefix")
    if prefix:
        if index > 9:
            raise RegistryError("legacy GUIDs hold ten entries of a kind: " + path)
        return "%s%s%d%02d" % (prefix, kind, index, channel)
    return hashlib.md5(("IA_HeliPaint:" + path).encode("utf-8")).hexdigest()[:16].upper()


def material_path(family, surface, channel):
    return "%sIA_%s_Paint%d.emat" % (family["material_dir"], twin_stem(family["surfaces"][surface], "material"), channel)


def material_guid(family, surface, channel):
    return twin_guid(family, "A", surface, channel, material_path(family, surface, channel))


def material_name(family, surface, channel):
    return "{%s}%s" % (material_guid(family, surface, channel), material_path(family, surface, channel))


def part_path(family, part, channel):
    return "%sIA_%s_Paint%d.et" % (family["prefab_dir"], twin_stem(family["parts"][part], "prefab"), channel)


def part_guid(family, part, channel):
    return twin_guid(family, "C", part, channel, part_path(family, part, channel))


def part_name(family, part, channel):
    return "{%s}%s" % (part_guid(family, part, channel), part_path(family, part, channel))


def hull_path(family, airframe, channel):
    return "%sIA_%s_Paint%d.et" % (family["prefab_dir"], twin_stem(family["airframes"][airframe], "prefab"), channel)


def hull_guid(family, airframe, channel):
    return twin_guid(family, "B", airframe, channel, hull_path(family, airframe, channel))


def entry_id(path, surface, repeat):
    """Stable id for one material entry; Workbench only needs it unique in the file."""
    key = "%s#%d" % (path, surface)
    if repeat:
        key += "#%d" % repeat
    return hashlib.md5(key.encode("utf-8")).hexdigest()[:16].upper()


def material_text(surface):
    return '%s : "%s" {\n}\n' % (surface["class"], surface["material"])


def assignments(family, path, slots, channel, indent):
    """Material entries of a twin, by surface and then in the order of the mesh's slots."""
    lines = []
    seen = {}
    order = sorted(range(len(slots)), key=lambda i: (slots[i]["surface"], i))
    for i in order:
        surface = slots[i]["surface"]
        repeat = seen.get(surface, 0)
        seen[surface] = repeat + 1
        lines.append('%sMaterialAssignClass "{%s}" {' % (indent, entry_id(path, surface, repeat)))
        lines.append('%s SourceMaterial "%s"' % (indent, slots[i]["slot"]))
        lines.append('%s AssignedMaterial "%s"' % (indent, material_name(family, surface, channel)))
        lines.append("%s}" % indent)
    return lines


def part_text(family, part, channel):
    entry = family["parts"][part]
    lines = ['%s : "%s" {' % (entry["class"], entry["prefab"])]
    lines += [' ID "%s"' % entry["id"], " components {", '  %s "{%s}" {' % (MESH_CLASS, entry["mesh_component"]), "   Materials {"]
    lines += assignments(family, part_path(family, part, channel), entry["slots"], channel, "    ")
    lines += ["   }", "  }", " }", "}"]
    return "\n".join(lines)


def hull_text(registry, family, airframe, channel):
    entry = family["airframes"][airframe]
    lines = ['%s : "%s" {' % (entry["class"], entry["prefab"])]
    lines += [' ID "%s"' % entry["id"], " components {", '  %s "{%s}" {' % (registry["rig_component"], registry["rig_component_id"]), "  }"]
    lines += ['  %s "{%s}" {' % (MESH_CLASS, entry["mesh_component"]), "   Materials {"]
    lines += assignments(family, hull_path(family, airframe, channel), entry["slots"], channel, "    ")
    lines += ["   }", "  }"]
    if entry["parts"]:
        lines += ['  %s "{%s}" {' % (SLOT_CLASS, entry["slot_component"]), "   Slots {"]
        for slot in entry["parts"]:
            lines += ["    %s %s {" % (slot["slot_class"], slot["slot"]), '     Prefab "%s"' % part_name(family, slot["part"], channel), "    }"]
        lines += ["   }", "  }"]
    lines += [" }", "}"]
    return "\n".join(lines)


def meta_text(file_guid, path):
    resource_class = RESOURCE_CLASSES[Path(path).suffix]
    lines = ["MetaFileClass {", ' Name "{%s}%s"' % (file_guid, path), " Configurations {", "  %s PC {" % resource_class, "  }"]
    for platform in PLATFORMS:
        lines += ["  %s %s : PC {" % (resource_class, platform), "  }"]
    lines += [" }", "}"]
    return "\n".join(lines)


def number(value):
    return "%g" % value


def quote(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def method_name(key):
    return "Register" + "".join(word.capitalize() for word in re.split(r"[^A-Za-z0-9]+", key) if word)


def param_line(surface, name, spec):
    """One script call that tells IA_HeliSkinPaint what a livery does to a material parameter."""
    stock = surface.get("stock", {}).get(name)
    stock_set = "true"
    if stock is None:
        stock_set = "false"

    if "value" in spec:
        if stock is None:
            stock = 0
        if isinstance(stock, list):
            raise RegistryError("%s is a colour in %s, not a number" % (name, surface["material"]))
        return "surface.AddScalar(%s, %s, %s, %s);" % (quote(name), number(spec["value"]), stock_set, number(stock))

    if stock is None:
        stock = [0, 0, 0]
    if not isinstance(stock, list):
        raise RegistryError("%s is a number in %s, not a colour" % (name, surface["material"]))
    rgb = ", ".join(number(v) for v in stock[:3])
    if "primary" in spec:
        return "surface.AddPrimary(%s, %s, %s, %s);" % (quote(name), number(spec["primary"]), stock_set, rgb)
    if "tint" in spec:
        if len(spec["tint"]) < 3 or min(spec["tint"][:3]) <= 0:
            raise RegistryError("tint of %s in %s needs the texture's paint colour, three numbers above 0" % (name, surface["material"]))
        return "surface.AddTint(%s, %s, %s, %s);" % (quote(name), ", ".join(number(v) for v in spec["tint"][:3]), stock_set, rgb)
    if "color" in spec:
        return "surface.AddColor(%s, %s, %s, %s);" % (quote(name), ", ".join(number(v) for v in spec["color"][:3]), stock_set, rgb)
    raise RegistryError("paint of %s in %s needs primary, tint, color or value" % (name, surface["material"]))


def pattern_of(path):
    """A twin's path with the channel number as a script format argument."""
    return re.sub(r"_Paint1(\.\w+)$", r"_Paint%1\1", path)


def manifest_text(registry):
    """The script file that registers every family with IA_HeliPaintChannels."""
    manifest = registry["manifest"]
    class_name = manifest.get("class", "IA_HeliPaintManifest")
    channels = range(1, registry["channel_count"] + 1)
    bar = "\t//" + "-" * 96
    lines = ["//" + "-" * 96]
    lines.append("//! Paint channel families, written by tools/author_heli_paint_channels.py from")
    lines.append("//! %s. Do not edit: change the registry and run the tool." % manifest.get("source", REGISTRY))
    lines.append("//" + "-" * 96)
    if manifest.get("modded"):
        lines += ["modded class %s" % class_name, "{", bar, "\toverride static void Register()", "\t{", "\t\tsuper.Register();"]
    else:
        lines += ["class %s" % class_name, "{", bar, "\tstatic void Register()", "\t{"]
    for family in registry["families"]:
        lines.append("\t\t%s();" % method_name(family["key"]))
    lines.append("\t}")

    for family in registry["families"]:
        swatch = family["stock_swatch"]
        lines += ["", bar, "\tprotected static void %s()" % method_name(family["key"]), "\t{"]
        lines.append("\t\tIA_HeliPaintFamily family = IA_HeliPaintChannels.AddFamily(%s, %s, %s, %s);" % (quote(family["key"]), quote(family["name"]), quote(family["stock_paint"]), quote(family["art"])))
        lines += ["\t\tif (!family)", "\t\t\treturn;", ""]
        lines.append("\t\tfamily.SetStockSwatch(%d, %d, %d);" % (swatch[0], swatch[1], swatch[2]))
        for airframe, entry in enumerate(family["airframes"]):
            guids = " ".join(hull_guid(family, airframe, channel) for channel in channels)
            lines.append("\t\tfamily.AddAirframe(%s, %s, %s);" % (quote(entry["prefab"]), quote(pattern_of(hull_path(family, airframe, 1))), quote(guids)))
        lines += ["", "\t\tIA_HeliPaintSurface surface;"]
        for surface, entry in enumerate(family["surfaces"]):
            guids = " ".join(material_guid(family, surface, channel) for channel in channels)
            lines.append("\t\tsurface = family.AddSurface(%s, %s, %s);" % (quote(entry["material"]), quote(pattern_of(material_path(family, surface, 1))), quote(guids)))
            for name, spec in entry["paint"].items():
                lines.append("\t\t" + param_line(entry, name, spec))
        lines.append("\t}")
    lines.append("}")
    return "\n".join(lines) + "\n"


def validate(registry):
    keys = set()
    paths = set()
    for family in registry["families"]:
        if family["key"] in keys:
            raise RegistryError("two families share the key " + family["key"])
        keys.add(family["key"])
        if not family["airframes"] or not family["surfaces"]:
            raise RegistryError("family %s needs an airframe and a surface" % family["key"])
        for kind, count, name in (("material", len(family["surfaces"]), material_path), ("part", len(family["parts"]), part_path), ("airframe", len(family["airframes"]), hull_path)):
            for index in range(count):
                path = name(family, index, 1)
                if path in paths:
                    raise RegistryError('two stock files give the twin %s; give one of them a "name"' % path)
                paths.add(path)
        surfaces = len(family["surfaces"])
        for entry in family["parts"] + family["airframes"]:
            if not entry["slots"]:
                raise RegistryError("no paint surface on " + entry["prefab"])
            for slot in entry["slots"]:
                if slot["surface"] < 0 or slot["surface"] >= surfaces:
                    raise RegistryError("surface out of range on " + entry["prefab"])
        for entry in family["airframes"]:
            for slot in entry["parts"]:
                if slot["part"] < 0 or slot["part"] >= len(family["parts"]):
                    raise RegistryError("part out of range on " + entry["prefab"])


def build(registry):
    """Every generated file: relative path -> text."""
    validate(registry)
    files = {}

    def add(file_guid, path, text):
        files[path] = text
        files[path + ".meta"] = meta_text(file_guid, path)

    for family in registry["families"]:
        for channel in range(1, registry["channel_count"] + 1):
            for surface in range(len(family["surfaces"])):
                add(material_guid(family, surface, channel), material_path(family, surface, channel), material_text(family["surfaces"][surface]))
            for part in range(len(family["parts"])):
                add(part_guid(family, part, channel), part_path(family, part, channel), part_text(family, part, channel))
            for airframe in range(len(family["airframes"])):
                add(hull_guid(family, airframe, channel), hull_path(family, airframe, channel), hull_text(registry, family, airframe, channel))
    files[registry["manifest"]["path"]] = manifest_text(registry)
    return files


def leftovers(registry, files, root):
    """Files an earlier run wrote into an output folder that this registry no longer produces."""
    found = []
    folders = set()
    for family in registry["families"]:
        folders.add(family["material_dir"])
        folders.add(family["prefab_dir"])
    for folder in sorted(folders):
        target = root / folder
        if not target.is_dir():
            continue
        for item in sorted(target.iterdir()):
            relative = folder + item.name
            if item.is_file() and GENERATED.match(item.name) and relative not in files:
                found.append(relative)
    return found


def stale(files, root):
    """Paths whose file on disk differs. Workbench may rewrite a .meta, so only its Name line counts."""
    wrong = []
    for path, text in sorted(files.items()):
        target = root / path
        if not target.exists():
            wrong.append(path)
            continue
        with open(target, encoding="utf-8", newline="") as handle:
            current = handle.read()
        if path.endswith(".meta"):
            if text.splitlines()[1] not in current:
                wrong.append(path)
        elif current != text:
            wrong.append(path)
    return wrong


def parse_value(value):
    """A survey value: a number, or a colour as "r g b a"."""
    if isinstance(value, str):
        return [float(part) for part in value.split()[:3]]
    return value


def linear(value):
    """An sRGB byte as a linear colour value."""
    value = value / 255.0
    if value > 0.04045:
        return ((value + 0.055) / 1.055) ** 2.4
    return value / 12.92


def paint_colour(pixels):
    """The paint colour among a texture's pixels (sRGB bytes), as a linear colour: the commonest.

    A hull texture is paint with markings, wear and unused space around it. Unused space is
    filled with black or a flat grey, so exactly grey pixels are left out while others remain;
    the commonest colour of the rest, averaged with the ones close to it, is the paint.
    """
    counts = {}
    for pixel in pixels:
        counts[pixel] = counts.get(pixel, 0) + 1
    kept = {pixel: count for pixel, count in counts.items() if not pixel[0] == pixel[1] == pixel[2]}
    if sum(kept.values()) * 10 < len(pixels):
        kept = counts
    bins = {}
    for pixel, count in kept.items():
        key = tuple(value // PAINT_BIN for value in pixel)
        bins[key] = bins.get(key, 0) + count

    def around(key):
        return sum(bins.get((key[0] + i, key[1] + j, key[2] + k), 0) for i in (-1, 0, 1) for j in (-1, 0, 1) for k in (-1, 0, 1))

    best = max(sorted(bins), key=around)
    near = [(pixel, count) for pixel, count in kept.items() if all(abs(pixel[i] // PAINT_BIN - best[i]) <= 1 for i in range(3))]
    total = sum(count for pixel, count in near)
    return [round(sum(linear(pixel[i]) * count for pixel, count in near) / total, 4) for i in range(3)]


def texture_paint(path):
    """The paint colour of a colour texture (.edds), from a small mip; see paint_colour.

    Needs Pillow, and lz4 when the mip is stored compressed.
    """
    import io
    import struct
    from PIL import Image

    with open(path, "rb") as handle:
        data = handle.read()
    if data[:4] != b"DDS ":
        raise ValueError("not a DDS texture")
    mips = struct.unpack_from("<I", data, 28)[0]
    head = bytearray(data[:128])
    offset = 128
    extra = b""
    if data[84:88] == b"DX10":
        extra = data[128:148]
        offset = 148
    # An .edds lists its mips smallest first, each as a tag and a stored size, and the data follows.
    table = [(data[offset + i * 8:offset + i * 8 + 4], struct.unpack_from("<I", data, offset + i * 8 + 4)[0]) for i in range(mips)]
    position = offset + mips * 8
    level = min(mips - 1, MEASURE_SIDE.bit_length() - 1)
    for tag, size in table[:level]:
        position += size
    tag, size = table[level]
    block = data[position:position + size]
    if tag == b"LZ4 ":
        import lz4.block
        chunk = struct.unpack_from("<I", block, 4)[0] & 0xFFFFFF
        block = lz4.block.decompress(block[8:8 + chunk], uncompressed_size=struct.unpack_from("<I", block, 0)[0])
    elif tag != b"COPY":
        raise ValueError("unknown mip storage %r" % tag)
    side = 1 << level
    struct.pack_into("<I", head, 12, side)
    struct.pack_into("<I", head, 16, side)
    struct.pack_into("<I", head, 20, len(block))
    struct.pack_into("<I", head, 28, 1)
    image = Image.open(io.BytesIO(bytes(head) + extra + block)).convert("RGB")
    return paint_colour(list(image.get_flattened_data()))


def how_painted(info):
    """How a livery can recolour a material: "layers", "tint", or None when it cannot."""
    if info.get("class") in LAYERED_CLASSES and "Color_1" in info.get("params", {}):
        return "layers"
    if info.get("class") in TINTED_CLASSES:
        return "tint"
    return None


def adopt(registry, survey, options):
    """Add a family from an IA_HeliPaintSurvey file, or refresh one while keeping its paint recipe."""
    key = options["family"]
    old = None
    for family in registry["families"]:
        if family["key"] == key:
            old = family
    if not survey["airframes"]:
        raise RegistryError("the survey holds no airframe")

    first = survey["airframes"][0]
    folder = Path(path_of(first["prefab"])).parent.as_posix() + "/"
    family = {
        "key": key,
        "name": options.get("name") or (old or {}).get("name") or Path(path_of(first["prefab"])).stem.upper(),
        "stock_paint": options.get("stock-paint") or (old or {}).get("stock_paint") or "Factory Paint",
        "stock_swatch": (old or {}).get("stock_swatch") or [78, 88, 60],
        "art": options.get("art") or (old or {}).get("art") or "generic",
    }
    if old and "legacy_guid_prefix" in old:
        family["legacy_guid_prefix"] = old["legacy_guid_prefix"]
    family["material_dir"] = (old or {}).get("material_dir") or folder.replace("Prefabs/", "Assets/", 1) + "Paint/"
    family["prefab_dir"] = (old or {}).get("prefab_dir") or folder + "Paint/"

    # Surfaces: the ones named (hull or part materials), else a guess from the names of the
    # hull's own materials.
    wanted = None
    if options.get("surfaces"):
        wanted = options["surfaces"].split(",")
    elif old:
        wanted = [Path(path_of(surface["material"])).stem for surface in old["surfaces"]]
    found = {}
    for airframe in survey["airframes"]:
        owners = [airframe]
        if wanted is not None:
            owners += airframe["parts"]
        for owner in owners:
            for slot in owner["slots"]:
                material = slot["material"]
                if not material or material in found:
                    continue
                info = survey["materials"].get(material, {})
                stem = Path(path_of(material)).stem
                kind = how_painted(info)
                if wanted is not None:
                    if stem not in wanted:
                        continue
                    if not kind:
                        raise RegistryError("%s is a %s, which a livery cannot recolour" % (stem, info.get("class", "material of an unknown class")))
                elif not kind or NOT_PAINT.search(stem):
                    continue
                found[material] = kind
    if wanted is None:
        named = {material: kind for material, kind in found.items() if HULL.search(Path(path_of(material)).stem)}
        if named:
            found = named
        else:
            found = {material: kind for material, kind in found.items() if not NOT_HULL.search(Path(path_of(material)).stem)}
            if "layers" in found.values():
                found = {material: kind for material, kind in found.items() if kind == "layers"}
        print("guessed the hull paint by name. Check it, and name the right materials with --surfaces (a part's too):")
        for material in found:
            print("  %s" % Path(path_of(material)).stem)
    surfaces = []
    for material in found:
        info = survey["materials"].get(material, {})
        surfaces.append({"material": material, "class": info["class"], "paint": {}, "stock": {name: parse_value(value) for name, value in info.get("params", {}).items()}})
    if wanted is not None:
        surfaces.sort(key=lambda surface: wanted.index(Path(path_of(surface["material"])).stem))
    for index, surface in enumerate(surfaces):
        kept = None
        if old:
            kept = next((entry for entry in old["surfaces"] if entry["material"] == surface["material"]), None)
        if kept:
            surface["paint"] = kept["paint"]
            if "name" in kept:
                surface["name"] = kept["name"]
        elif found[surface["material"]] == "tint":
            # A draft: every tinted surface takes the livery colour over its own texture's paint.
            colour, texture = TINTED_CLASSES[surface["class"]]
            surface["paint"] = {colour: {"tint": measured_paint(survey, surface["material"], texture, options.get("textures"))}}
        elif index == 0:
            # A draft: the first layer of the first surface takes the livery colour.
            surface["paint"] = {"Color_1": {"primary": 1}}
    if not surfaces:
        raise RegistryError("no paint surface found; name the hull materials with --surfaces")
    family["surfaces"] = surfaces

    def paint_slots(entry):
        slots = []
        for slot in entry["slots"]:
            for index, surface in enumerate(surfaces):
                if slot["material"] == surface["material"]:
                    slots.append({"slot": slot["slot"], "surface": index})
                    # An older survey does not say; a vanilla slot name ends with its default material's GUID.
                    origin = slot.get("from")
                    if origin == "prefab" or (origin is None and not slot["slot"].endswith(guid_of(surface["material"]))):
                        print("warning: %s already assigns a material to %s; the twin's entry replaces it" % (entry["prefab"], slot["slot"]))
        return slots

    include = None
    if options.get("parts"):
        include = options["parts"].split(",")
    elif old and not options.get("surfaces"):
        # A refresh keeps the parts it had; naming surfaces anew takes every part that shows them.
        include = sorted({slot["slot"] for entry in old["airframes"] for slot in entry["parts"]})
    parts = []
    airframes = []
    for airframe in survey["airframes"]:
        for name in ("id", "mesh_component"):
            if not airframe[name]:
                raise RegistryError("the survey has no %s for %s" % (name, airframe["prefab"]))
        entry = {name: airframe[name] for name in ("prefab", "class", "id", "mesh_component", "slot_component")}
        entry["slots"] = paint_slots(airframe)
        entry["parts"] = []
        for part in airframe["parts"]:
            if include is not None and part["slot"] not in include:
                continue
            slots = paint_slots(part)
            if not slots:
                continue
            if not part["id"] or not part["mesh_component"] or not airframe["slot_component"]:
                print("warning: ids missing for part %s of %s; it keeps stock paint" % (part["slot"], airframe["prefab"]))
                continue
            index = next((i for i, known in enumerate(parts) if known["prefab"] == part["prefab"]), -1)
            if index < 0:
                index = len(parts)
                parts.append({"prefab": part["prefab"], "class": part["class"], "id": part["id"], "mesh_component": part["mesh_component"], "slots": slots})
            entry["parts"].append({"slot": part["slot"], "slot_class": part["slot_class"], "part": index})
        airframes.append(entry)
    family["parts"] = parts
    family["airframes"] = airframes

    if old:
        registry["families"][registry["families"].index(old)] = family
    else:
        registry["families"].append(family)
    return family


def measured_paint(survey, material, texture, folder):
    """The paint colour of a tinted surface, from its texture when the file can be read."""
    name = survey["materials"].get(material, {}).get("textures", {}).get(texture)
    stem = Path(path_of(material)).stem
    if not folder or not name:
        print("  %s: paint colour not measured; set its tint in the registry" % stem)
        return list(UNMEASURED_PAINT)
    path = Path(folder) / path_of(name)
    try:
        colour = texture_paint(path)
    except (OSError, ImportError, ValueError) as error:
        print("  %s: paint colour not measured (%s); set its tint in the registry" % (stem, error))
        return list(UNMEASURED_PAINT)
    print("  %s: paint colour %s from %s" % (stem, colour, path.name))
    return colour


def option(argv, name):
    flag = "--" + name
    if flag in argv and argv.index(flag) + 1 < len(argv):
        return argv[argv.index(flag) + 1]
    return None


def dump(value, indent=0):
    """JSON with one line for a list of numbers or a small flat object, so the registry stays readable."""
    pad = " " * indent
    if isinstance(value, dict):
        flat = json.dumps(value)
        if len(flat) <= 100 and not any(isinstance(item, dict) for item in value.values()):
            return flat
        rows = ["%s %s: %s" % (pad, json.dumps(name), dump(item, indent + 1)) for name, item in value.items()]
        return "{\n" + ",\n".join(rows) + "\n" + pad + "}"
    if isinstance(value, list):
        if not any(isinstance(item, (dict, list)) for item in value):
            return json.dumps(value)
        rows = ["%s %s" % (pad, dump(item, indent + 1)) for item in value]
        return "[\n" + ",\n".join(rows) + "\n" + pad + "]"
    return json.dumps(value)


def read_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)


def main(argv):
    root = ROOT
    if option(argv, "root"):
        root = Path(option(argv, "root")).resolve()
    registry_path = ROOT / REGISTRY
    if option(argv, "registry"):
        registry_path = Path(option(argv, "registry")).resolve()

    try:
        registry = read_json(registry_path)
        if option(argv, "adopt"):
            if not option(argv, "family"):
                raise RegistryError("--adopt needs --family <key>")
            options = {name: option(argv, name) for name in ("family", "name", "stock-paint", "art", "surfaces", "parts", "textures")}
            family = adopt(registry, read_json(option(argv, "adopt")), options)
            build(registry)
            write_text(registry_path, dump(registry) + "\n")
            print("family %s: %d airframes, %d parts, %d surfaces" % (family["key"], len(family["airframes"]), len(family["parts"]), len(family["surfaces"])))
            print("set its paint recipe in %s, then run this tool again to write the files" % registry_path.name)
            return 0

        files = build(registry)
    except RegistryError as error:
        print("error: %s" % error)
        return 1

    extra = leftovers(registry, files, root)
    if "--check" in argv:
        wrong = stale(files, root)
        for path in wrong:
            print("stale: " + path)
        for path in extra:
            print("leftover: " + path)
        return len(wrong) + len(extra)

    for path, text in files.items():
        write_text(root / path, text)
    for path in extra:
        (root / path).unlink()
    print("wrote %d files, removed %d" % (len(files), len(extra)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
