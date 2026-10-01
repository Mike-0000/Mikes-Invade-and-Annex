"""Write the UH-1H paint channel assets.

A material is shared by every entity that names it, so recolouring the vanilla
UH-1H material recolours every Huey. A paint channel is a prefab variant of a
stock airframe whose hull and seats name their own copies of those materials;
the game recolours a channel's copies and only that helicopter changes. The
copies inherit the vanilla materials unchanged, so a channel airframe looks
stock until IA_HeliSkinPaint sets a skin's colours on it.

Names and GUIDs follow one pattern that IA_HeliPaintChannels rebuilds in
script. Run this after changing CHANNEL_COUNT, an airframe or a surface, open
Workbench once so it imports the files, then run IA_HeliSkinAssetCheck.

    python tools/author_heli_paint_channels.py          write the files
    python tools/author_heli_paint_channels.py --check  fail when a file is stale
"""
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CHANNEL_COUNT = 4
GUID_PREFIX = "D3A91F5C7E20"
MATERIAL_DIR = "Assets/Vehicles/Helicopters/UH1H/"
PREFAB_DIR = "Prefabs/Vehicles/Helicopters/UH1H/"
SEAT_DIR = PREFAB_DIR + "VehParts/Seats/"

# Vanilla material GUID and file name, in IA_HeliPaintChannels surface order.
SURFACES = (
    ("6224CA051369DE45", "UH_1H_Body01"),
    ("04455994F64CAB1E", "UH_1H_Interior01"),
    ("5AAD70D749C12511", "UH_1H_Interior02"),
)
SEAT_SURFACES = (1, 2)

# Vanilla seat part GUID and file name.
SEATS = (
    ("67030528FA4FA725", "VehPart_UH1H_seats_gunners"),
    ("205599C7074F1E02", "VehPart_UH1H_seats_cargo"),
    ("0A5C688A2A1E7548", "VehPart_UH1H_seats_gunners_dummy"),
    ("5C4D9B53495B219A", "VehPart_UH1H_seats_cargo_openDoor"),
)

# Stock airframe GUID, file name, and the seat part in its Seat_Gunners and Seat_Cargo slots.
AIRFRAMES = (
    ("70BAEEFC2D3FEE64", "UH1H", 0, 1),
    ("DDDD9B51F1234DF3", "UH1H_armed", 2, 3),
    ("21E9A875C0A3C409", "UH1H_armed_gunship_HE", 2, 3),
    ("CB4D4CF7E887B2D0", "UH1H_armed_gunship_HEDP", 2, 3),
)

RESOURCE_CLASSES = {".emat": "EMATResourceClass", ".et": "EntityTemplateResourceClass"}
PLATFORMS = ("XBOX_ONE", "XBOX_SERIES", "PS4", "PS5", "HEADLESS")


def guid(kind, index, channel):
    return "%s%s%d0%d" % (GUID_PREFIX, kind, index, channel)


def material_path(surface, channel):
    return "%sPaint/IA_%s_Paint%d.emat" % (MATERIAL_DIR, SURFACES[surface][1], channel)


def material_name(surface, channel):
    return "{%s}%s" % (guid("A", surface, channel), material_path(surface, channel))


def seat_path(seat, channel):
    return "%sPaint/IA_%s_Paint%d.et" % (PREFAB_DIR, SEATS[seat][1], channel)


def seat_name(seat, channel):
    return "{%s}%s" % (guid("C", seat, channel), seat_path(seat, channel))


def hull_path(airframe, channel):
    return "%sPaint/IA_%s_Paint%d.et" % (PREFAB_DIR, AIRFRAMES[airframe][1], channel)


def entry_id(path, surface):
    """Stable id for one material entry; Workbench only needs it unique in the file."""
    return hashlib.md5(("%s#%d" % (path, surface)).encode("ascii")).hexdigest()[:16].upper()


def material_text(surface):
    return 'MatPBRMulti : "{%s}%sData/%s.emat" {\n}\n' % (SURFACES[surface][0], MATERIAL_DIR, SURFACES[surface][1])


def assignments(path, surfaces, channel, indent):
    lines = []
    for surface in surfaces:
        lines.append('%sMaterialAssignClass "{%s}" {' % (indent, entry_id(path, surface)))
        lines.append('%s SourceMaterial "%s_%s"' % (indent, SURFACES[surface][1], SURFACES[surface][0]))
        lines.append('%s AssignedMaterial "%s"' % (indent, material_name(surface, channel)))
        lines.append("%s}" % indent)
    return lines


def seat_text(seat, channel):
    lines = ['GenericEntity : "{%s}%s%s.et" {' % (SEATS[seat][0], SEAT_DIR, SEATS[seat][1])]
    lines += [' ID "4B42E71698F5739C"', " components {", '  MeshObject "{4B42E716914465B9}" {', "   Materials {"]
    lines += assignments(seat_path(seat, channel), SEAT_SURFACES, channel, "    ")
    lines += ["   }", "  }", " }", "}"]
    return "\n".join(lines)


def hull_text(airframe, channel):
    stock_guid, name, gunners, cargo = AIRFRAMES[airframe]
    lines = ['Vehicle : "{%s}%s%s.et" {' % (stock_guid, PREFAB_DIR, name)]
    lines += [' ID "5DB688595BAB2FD7"', " components {", '  MeshObject "{51DAA09FEFBFC0E7}" {', "   Materials {"]
    lines += assignments(hull_path(airframe, channel), range(len(SURFACES)), channel, "    ")
    lines += ["   }", "  }", '  SlotManagerComponent "{55BCE45E438E4CFF}" {', "   Slots {"]
    for slot, seat in (("Seat_Gunners", gunners), ("Seat_Cargo", cargo)):
        lines += ["    RegisteringComponentSlotInfo %s {" % slot, '     Prefab "%s"' % seat_name(seat, channel), "    }"]
    lines += ["   }", "  }", " }", "}"]
    return "\n".join(lines)


def meta_text(file_guid, path):
    resource_class = RESOURCE_CLASSES[Path(path).suffix]
    lines = ["MetaFileClass {", ' Name "{%s}%s"' % (file_guid, path), " Configurations {", "  %s PC {" % resource_class, "  }"]
    for platform in PLATFORMS:
        lines += ["  %s %s : PC {" % (resource_class, platform), "  }"]
    lines += [" }", "}"]
    return "\n".join(lines)


def build():
    """Every generated file: relative path -> text."""
    files = {}

    def add(file_guid, path, text):
        files[path] = text
        files[path + ".meta"] = meta_text(file_guid, path)

    for channel in range(1, CHANNEL_COUNT + 1):
        for surface in range(len(SURFACES)):
            add(guid("A", surface, channel), material_path(surface, channel), material_text(surface))
        for seat in range(len(SEATS)):
            add(guid("C", seat, channel), seat_path(seat, channel), seat_text(seat, channel))
        for airframe in range(len(AIRFRAMES)):
            add(guid("B", airframe, channel), hull_path(airframe, channel), hull_text(airframe, channel))
    return files


def stale(files):
    """Paths whose file on disk differs. Workbench may rewrite a .meta, so only its Name line counts."""
    wrong = []
    for path, text in sorted(files.items()):
        target = ROOT / path
        if not target.exists():
            wrong.append(path)
            continue
        current = target.read_text(encoding="utf-8")
        if path.endswith(".meta"):
            if text.splitlines()[1] not in current:
                wrong.append(path)
        elif current != text:
            wrong.append(path)
    return wrong


def main(argv):
    files = build()
    if "--check" in argv:
        wrong = stale(files)
        for path in wrong:
            print("stale: " + path)
        return len(wrong)

    for path, text in files.items():
        target = ROOT / path
        target.parent.mkdir(parents=True, exist_ok=True)
        with open(target, "w", encoding="utf-8", newline="") as handle:
            handle.write(text)
    print("wrote %d files" % len(files))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
