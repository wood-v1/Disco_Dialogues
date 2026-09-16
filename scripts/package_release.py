"""Package an explicit allowlist; never deploy into or alter the installed game."""
import argparse
import configparser
import hashlib
import json
import struct
import zipfile
import shutil
from pathlib import Path
import xml.etree.ElementTree as ET
from generate_layouts import ROOT, SIZES, read_vfs, make_layout, validate, forms, rect, accent_texture


def assert_x86_dll(data):
    if data[:2] != b'MZ':
        raise ValueError('Missing DOS header')
    offset, = struct.unpack_from('<I', data, 0x3c)
    if data[offset:offset + 4] != b'PE\0\0':
        raise ValueError('Missing PE header')
    machine, = struct.unpack_from('<H', data, offset + 4)
    flags, magic = struct.unpack_from('<HH', data, offset + 22)
    if machine != 0x14c or magic != 0x10b or not flags & 0x2000:
        raise ValueError('Expected PE32 x86 DLL')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--oynon-root', type=Path, default=ROOT.parent / 'OynonTools')
    parser.add_argument('--game-root', type=Path, required=True)
    args = parser.parse_args()
    files = {
        'bin/Final/mods/DiscoDialogues.dll': ROOT / 'build-win32/Release/DiscoDialogues.dll',
        'bin/Final/mods/OynonTools.dll': ROOT / 'build-win32/Release/OynonTools.dll',
        'bin/Final/mods/DiscoDialogues.ini': ROOT / 'release-assets/DiscoDialogues.ini',
        'bin/Final/mods/DiscoDialogues.manifest.ini': ROOT / 'release-assets/DiscoDialogues.manifest.ini',
        'bin/Final/GameModLauncher.ini': ROOT / 'release-assets/GameModLauncher.ini',
        'data/Scripts/disco_dialogues_feed.bin': ROOT / 'resources/scripts/disco_dialogues_feed.bin',
        'data/Textures/ui/disco_dialogues_accents.tga': ROOT / 'resources/textures/ui/disco_dialogues_accents.tga',
        'data/Sounds/disco-dialogs-action.ogg': ROOT / 'resources/audio/disco-dialogs-action.ogg',
    }
    custom_scripts = {f'disco_dialogues_{part}.bin' for part in ('feed', 'panel', 'photo', 'title')}
    for name in custom_scripts:
        files['data/Scripts/' + name] = ROOT / 'resources/scripts' / name
    originals = read_vfs(args.game_root / 'data/UI.vfs', [f'dialog_{w}x{h}.xml' for w, h, *_ in SIZES])
    scripts = set()
    for size in SIZES:
        w, h, *_ = size
        name = f'disco_dialogues_{w}x{h}.xml'
        source = ROOT / 'resources/ui' / name
        generated = ET.parse(source).getroot()
        original = ET.fromstring(originals[f'dialog_{w}x{h}.xml'])
        validate(original, generated, size)
        expected = forms(make_layout(original, size))
        if any(rect(node) != rect(expected[path]) for path, node in forms(generated).items()):
            raise ValueError('Layout geometry is stale')
        scripts.update(node.get('script') for node in generated.iter() if node.get('script'))
        files[f'data/UI/{name}'] = source
    scripts.difference_update(custom_scripts)
    script_data = read_vfs(args.game_root / 'data/Scripts.vfs', sorted(scripts))
    payload = {name: path.read_bytes() for name, path in files.items()}
    if payload['data/Textures/ui/disco_dialogues_accents.tga'] != accent_texture():
        raise ValueError('UI atlas is stale; regenerate layouts and textures')
    assert sorted(name for name in payload if name.lower().endswith(('.ogg', '.wav'))) == ['data/Sounds/disco-dialogs-action.ogg']
    # A matching shared runtime is required for the Inventory Overhaul hotfix.
    import pefile
    mod_pe = pefile.PE(data=payload['bin/Final/mods/DiscoDialogues.dll'])
    assert all(entry.dll.lower() != b'winmm.dll' for entry in mod_pe.DIRECTORY_ENTRY_IMPORT), 'Separate WinMM player remains linked'
    shared_pe = pefile.PE(data=payload['bin/Final/mods/OynonTools.dll'])
    exports = {symbol.name for symbol in shared_pe.DIRECTORY_ENTRY_EXPORT.symbols}
    required_exports = {b'OynonUIDialogBlocksItemHotkeys', b'OynonUIDialogInputGeneration'}
    runtime_exports = {b'OynonInstallCameraTransitHook', b'OynonInstallUIExecuteHook',
                       b'OynonInstallScriptAudioHooks', b'OynonProceedCameraTransit',
                       b'OynonProceedUIExecute', b'OynonProceedSpeech'}
    assert runtime_exports <= exports, 'OynonTools is missing the runtime API'
    assert required_exports <= exports, 'Rebuild OynonTools with the dialog input gate'
    for name, data in payload.items():
        if name.endswith('.dll'):
            assert_x86_dll(data)
    hints = configparser.ConfigParser()
    hints.read_string(payload['bin/Final/GameModLauncher.ini'].decode('utf-8'))
    assert hints['SharedDlls']['Names'] == 'OynonTools.dll'
    assert hints['SharedDll:OynonTools.dll']['RequiredBy'] == 'DiscoDialogues.dll'
    assert hints['Mods']['LoadOrder'] == 'OynonTools.dll@suspended, DiscoDialogues.dll@ui+3000'
    release = ROOT / 'release'
    release.mkdir(exist_ok=True)
    zip_path = release / 'Pathologic_Disco_Dialogues_1_0_0.zip'
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(payload.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(zip_path) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == set(files) and len(archive.namelist()) == 14
        for name, data in payload.items():
            assert archive.read(name) == data
    report = {
        'package': zip_path.name,
        'sha256': hashlib.sha256(zip_path.read_bytes()).hexdigest(),
        'checks': ['PE32 x86 DLLs', 'XML contracts and bounds', 'vanilla script references exist',
                   'exact 14-file allowlist', 'ZIP CRC and payload byte equality', 'shared DLL install hints',
                   'shared dialog gate exports', 'runtime adapter exports'],
        'gameplay_validation': 'NOT RUN; static and isolated tests only',
        'files': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(payload.items())},
        'vanilla_script_hashes': {name: hashlib.sha256(data).hexdigest() for name, data in script_data.items()},
    }
    compatibility = release / 'compatibility'
    compatibility.mkdir(exist_ok=True)
    inventory_dll = ROOT / 'tmp/build-inventory-stage2/Release/InventoryOverhaul.dll'
    inventory_data = inventory_dll.read_bytes()
    assert_x86_dll(inventory_data)
    inventory_pe = pefile.PE(data=inventory_data)
    imports = {symbol.name for entry in inventory_pe.DIRECTORY_ENTRY_IMPORT for symbol in entry.imports}
    assert required_exports <= imports, 'Rebuild Inventory Overhaul with dialog guards'
    shutil.copy2(inventory_dll, compatibility / 'InventoryOverhaul.dll')
    shutil.copy2(files['bin/Final/mods/OynonTools.dll'], compatibility / 'OynonTools.dll')
    report['compatibility'] = {name: hashlib.sha256((compatibility / name).read_bytes()).hexdigest()
                               for name in ('InventoryOverhaul.dll', 'OynonTools.dll')}
    (release / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: {zip_path} (14 entries; {len(scripts)} unchanged stock script references; four custom UI BINs); matching compatibility DLLs')


if __name__ == '__main__':
    main()
