"""Generate mod-owned layouts and a language-independent UI atlas."""
import argparse
import copy
import hashlib
import json
import math
import struct
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SIZES = [(1920, 1080, 1243, 24, 653, 1032),
         (1600, 900, 1036, 20, 544, 860),
         (1366, 768, 885, 17, 464, 734),
         (1024, 768, 543, 17, 464, 734),
         (800, 600, 424, 13, 363, 574)]


def stock_layout_name(width, height):
    # The base 800x600 layout has no resolution suffix in UI.vfs.
    return "dialog.xml" if (width, height) == (800, 600) else f"dialog_{width}x{height}.xml"


def accent_texture():
    # Cream circle, cream swatch, and opaque black panel swatch (top right).
    # Uncompressed BGRA TGA, same format as Inventory Overhaul's runtime texture.
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, 32, 16, 32, 40)
    pixels = bytearray()
    for y in range(16):
        for x in range(32):
            alpha = round(255 * max(0, min(1, 7.5 - math.hypot(x - 7.5, y - 7.5)))) if x < 16 else 255
            pixels.extend((0, 0, 0, 255) if x >= 16 and y < 4 else
                          (218, 237, 244, alpha))  # #F4EDDA
    return header + pixels


def accent_image(parent, name, circle=False):
    image = ET.SubElement(parent, 'image', name=name, x='0' if circle else '0.75',
                          y='0' if circle else '0.5', w='0.5' if circle else '0.03125',
                          h='1' if circle else '0.0625')
    image.text = 'ui/disco_dialogues_accents.tga'


def read_vfs(path, names):
    result = {}
    with path.open("rb") as stream:
        if stream.read(4) != b"LP1C":
            raise ValueError(f"Unsupported VFS: {path}")
        stream.read(4)
        count, = struct.unpack("<I", stream.read(4))
        entries = {}
        for _ in range(count):
            name = stream.read(stream.read(1)[0]).decode("cp1251")
            length, offset = struct.unpack("<II", stream.read(8))
            stream.read(8)
            entries[name] = (length, offset)
        total = path.stat().st_size
        for name in names:
            length, offset = entries[name]
            if offset + length > total:
                raise ValueError(f"Invalid VFS entry: {name}")
            stream.seek(offset)
            result[name] = stream.read(length)
    return result


def forms(root):
    result = {}
    def visit(node, parent=""):
        path = parent + "/" + node.get("name", node.tag)
        if node.tag == "form":
            result[path] = node
        for child in node:
            visit(child, path)
    visit(root)
    return result


def rect(node):
    return tuple(int(node.get(key)) for key in "xywh")


def set_rect(node, values):
    for key, value in zip("xywh", values):
        node.set(key, str(int(value)))


def contract(node):
    # Compare keyed subtrees, allowing sibling draw order to change.
    return (node.tag, tuple(sorted((k, v) for k, v in node.attrib.items()
                                 if not (node.tag == "form" and k in "xywh"))),
            (node.text or "").strip(), sorted(contract(child) for child in node))


def make_layout(original, size):
    width, height, cx, cy, cw, ch = size
    def s(value): return int(value * height / 1080 + 0.5)
    # Author a new component tree; do not ship a transformed vanilla window.
    root = ET.Element("form", name=f"disco_dialogues_{width}x{height}",
                      x="0", y="0", w=str(width), h=str(height), script="ui_dialog.bin")
    def form(parent, name, rectangle, script=None):
        node = ET.SubElement(parent, "form", name=name)
        set_rect(node, rectangle)
        if script: node.set("script", script)
        return node
    stock = {node.get("name"): node for node in original.iter("form")}
    panel = form(root, "panel", (cx, cy, cw, ch), "disco_dialogues_panel.bin")
    # Stock atlas names/UVs vary by language. Sample inside our black swatch;
    # the panel script applies BACKGROUND_ALPHA when drawing it.
    ET.SubElement(panel, 'image', name='default', x='0.75', y='0.0625',
                  w='0.03125', h='0.0625').text = 'ui/disco_dialogues_accents.tga'
    accent_image(panel, 'panel_edge')
    form(panel, "photo", (cx+s(24), cy+s(24), s(174), s(144)), "disco_dialogues_photo.bin")
    title = form(panel, "name", (cx+s(222), cy+s(24), cw-s(246), s(144)), "disco_dialogues_title.bin")
    title.append(copy.deepcopy(stock["name"].find("font")))
    feed = form(panel, "dialog_text", (cx+s(24), cy+s(188), cw-s(48), ch-s(212)), "disco_dialogues_feed.bin")
    feed.append(copy.deepcopy(stock["dialog_text"].find("font")))
    ET.SubElement(feed, 'sound', name='disco-dialogs-action', stream='0', loop='0').text = 'disco-dialogs-action.ogg'
    accent_image(feed, 'feed_thumb', circle=True)
    accent_image(feed, 'feed_track')
    root.append(copy.deepcopy(original.find("cursor")))
    validate(original, root, size)
    return root


def validate(original, generated, size):
    width, height, cx, cy, cw, ch = size
    nodes = {node.get("name"): node for node in generated.iter("form")}
    background = nodes['panel'].find("image[@name='default']")
    if background is None or contract(background) != (
            'image', (('h', '0.0625'), ('name', 'default'), ('w', '0.03125'),
                      ('x', '0.75'), ('y', '0.0625')), 'ui/disco_dialogues_accents.tga', []):
        raise ValueError('Panel must use the language-independent background swatch')
    sounds = list(generated.iter('sound'))
    if len(sounds) != 1 or sounds[0] not in list(nodes['dialog_text']) or contract(sounds[0]) != (
            'sound', (('loop', '0'), ('name', 'disco-dialogs-action'), ('stream', '0')), 'disco-dialogs-action.ogg', []):
        raise ValueError('Native choice sound resource missing or changed')
    if set(nodes) != {f"disco_dialogues_{width}x{height}", "panel", "photo", "name", "dialog_text"}:
        raise ValueError("Unexpected legacy or missing feed widget")
    scripts = {n.get("name"): n.get("script") for n in generated.iter("form")}
    for name, script in {"panel": "disco_dialogues_panel.bin", "photo": "disco_dialogues_photo.bin", "name": "disco_dialogues_title.bin",
                         "dialog_text": "disco_dialogues_feed.bin"}.items():
        if scripts[name] != script: raise ValueError("Widget behaviour changed: " + name)
    for path, node in forms(generated).items():
        x, y, w, h = rect(node)
        if w <= 0 or h <= 0 or x < 0 or y < 0 or x+w > width or y+h > height:
            raise ValueError(f"Invalid screen bounds: {path}")
        if node is not generated and not (cx <= x and cy <= y and x+w <= cx+cw and y+h <= cy+ch):
            raise ValueError(f"Outside column: {path}")
    photo, title, feed = (rect(nodes[n]) for n in ("photo", "name", "dialog_text"))
    if photo[0]+photo[2] >= title[0] or max(photo[1]+photo[3], title[1]+title[3]) >= feed[1]:
        raise ValueError("Portrait/title overlap the feed")
    if feed[3] < height * 0.7: raise ValueError("Feed must use most of the column")
    for name in ("name", "dialog_text"):
        if contract(nodes[name].find("font")) != contract(original.find(f".//form[@name='{name}']/font")):
            raise ValueError("Native font contract changed")
    if contract(generated.find("cursor")) != contract(original.find("cursor")):
        raise ValueError("Native cursor contract changed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    texture = ROOT / 'resources/textures/ui/disco_dialogues_accents.tga'
    if args.check:
        if texture.read_bytes() != accent_texture(): raise ValueError('Accent texture is stale')
    else:
        texture.parent.mkdir(parents=True, exist_ok=True)
        texture.write_bytes(accent_texture())
    names = [stock_layout_name(s[0], s[1]) for s in SIZES]
    originals = read_vfs(args.game_root / "data/UI.vfs", names)
    output = ROOT / "resources/ui"
    output.mkdir(parents=True, exist_ok=True)
    report = ["# Geometry changes", "", "Mod-owned component tree and language-independent background texture. Stock fonts and cursor are retained. Panel opacity, portrait toggle and uppercase title use mod-owned scripts. The same feed switches between dialogue and scrollable character information.", ""]
    hashes = {}
    for size, name in zip(SIZES, names):
        raw = originals[name]
        hashes[name] = hashlib.sha256(raw).hexdigest()
        original = ET.fromstring(raw)
        generated = make_layout(original, size)
        ET.indent(generated, space="  ")
        data = ET.tostring(generated, encoding="utf-8") + b"\n"
        path = output / f"disco_dialogues_{size[0]}x{size[1]}.xml"
        if args.check:
            if path.read_bytes() != data:
                raise ValueError(f"Generated file is stale: {path}")
        else:
            path.write_bytes(data)
        report += [f"## {size[0]} x {size[1]}", "", "| Form path | Original | Disco Dialogues | Delta |", "|---|---|---|---|"]
        old = forms(original)
        for key, node in forms(generated).items():
            old_name = node.get("name")
            old_node = next((n for n in old.values() if n.get("name") == old_name), original)
            before, after = rect(old_node), rect(node)
            if before != after:
                delta = tuple(b - a for a, b in zip(before, after))
                render = lambda r: ",".join(map(str, r))
                report.append(f"| `{key}` | `{render(before)}` | `{render(after)}` | `{render(delta)}` |")
        report.append("")
    if not args.check:
        (ROOT / "docs").mkdir(exist_ok=True)
        (ROOT / "docs/GEOMETRY.md").write_text("\n".join(report), encoding="utf-8")
        (ROOT / "docs/original-layout-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")
    print("PASS: five layouts; native widget contracts; screen/column bounds; unified feed; no legacy frames; deterministic output")


if __name__ == "__main__":
    main()
