"""Read-only checks of hook guards, PE slots and real HD camera-call fixtures."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_layouts import read_vfs, DEFAULT_GAME


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-root', type=Path, default=DEFAULT_GAME)
    parser.add_argument('--pathologic-re', type=Path, required=True)
    args = parser.parse_args()
    import pefile
    modules = {}
    for name, stamp, size in [('Game.exe', 0x5698d115, 0x4c4000),
                              ('Engine.dll', 0x561b8394, 0x26e000),
                              ('UI.dll', 0x5654c15f, 0xcf000)]:
        path = args.game_root / 'bin/Final' / name
        pe = pefile.PE(str(path))
        assert pe.FILE_HEADER.TimeDateStamp == stamp
        assert pe.OPTIONAL_HEADER.SizeOfImage == size
        modules[name] = (pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
    checked = 0
    # Presentation contracts used by the UI scripts. StretchBlit's optional
    # sixth argument is a float alpha passed to this image's draw, not a window
    # alpha inherited by children. UI key events forward their argument intact.
    for module, offset, encoded in [
        ('UI.dll', 0x353e0, '83fb0675208b4e148d542420528b018b403c'),
        ('UI.dll', 0x35461, '51f30f1104248bcd'),
        ('UI.dll', 0x2f149, '8b4424188d54240452c7442408010000008944240c'),
        ('UI.dll', 0x2f165, '8b016a016a65ff5010'),
        ('Game.exe', 0x216eb8, 'b9c8000000894c242c8d4168'),
        ('Game.exe', 0x216ee8, '8d8168ffffffc644243c05505157'),
        # Stock UI uses gp_dpadup=267, i.e. the virtual key, not binding ID 411.
        ('Game.exe', 0x217281, '680b010000689b01000057'),
    ]:
        expected = bytes.fromhex(encoded)
        assert modules[module][1][offset:offset+len(expected)] == expected, (module, hex(offset))
        checked += 1
    for source in ['src/dialog_camera.cpp', 'src/dialog_feed.cpp']:
        text = (ROOT / source).read_text(encoding='utf-8')
        for symbol, address, encoded, length in re.findall(
                r'hd::Bytes\((g|e|ui), (0x[0-9a-f]+), "([^"]+)", (\d+)\)', text):
            module = {'g': 'Game.exe', 'e': 'Engine.dll', 'ui': 'UI.dll'}[symbol]
            expected = bytes.fromhex(encoded.replace('\\x', ''))
            assert len(expected) == int(length)
            image = modules[module][1]
            offset = int(address, 16)
            assert image[offset:offset+len(expected)] == expected, (source, address)
            checked += 1
    for module, slot, target in [('Engine.dll', 0x205684, 0x13a1e0),
                                  ('Game.exe', 0x37ba44, 0x23d21),
                                  ('UI.dll', 0xa4e00, 0x4390)]:
        base, image = modules[module]
        assert struct.unpack_from('<I', image, slot)[0] == base + target
    # Verify the HD vector setter/getter slots used by the scoped target edit.
    base, image = modules['Engine.dll']
    assert struct.unpack_from('<I', image, 0x1fe778+5*4)[0] == base + 0x126ac0
    assert struct.unpack_from('<I', image, 0x1fe778+11*4)[0] == base + 0x126350
    # Regression: HD adds a one-argument FOV setter absent from the Beta header.
    # Verify both the real constructor's import getter call and base vtable.
    base, image = modules['Game.exe']
    def target(slot):
        rva = struct.unpack_from('<I', image, 0x371cdc+slot*4)[0] - base
        if image[rva] == 0xe9:
            rva += 5 + struct.unpack_from('<i', image, rva+1)[0]
        return rva
    assert target(9) == 0x153190  # SetViewFOV(float), ends with ret 4.
    assert image[0x153190:0x15319e] == bytes.fromhex('f30f10442404f30f114124c20400')
    assert target(10) == 0x152c80  # GetImportFOV -> virtual GetViewFOV.
    assert image[0x152c80:0x152c88] == bytes.fromhex('8b018b4020ffe0cc')
    header = (ROOT / 'src/camera_abi.h').read_text(encoding='utf-8')
    assert '>(camera, 10)(camera)' in header

    sys.path.insert(0, str(args.pathologic_re / 'parser/lib'))
    import PathologicScript as PS
    PS.IS_ALPHA = False
    directory = ROOT / 'tmp/camera-fixtures'
    directory.mkdir(parents=True, exist_ok=True)
    expected_counts = {'arena_manager.bin': 1, 'citizen_boy.bin': 2,
                       'citizen_worker.bin': 1, 'citizen_girl.bin': 2}
    names = list(expected_counts)
    stock = read_vfs(args.game_root / 'data/Scripts.vfs', names)
    fixtures, sites = [], []
    for name, data in stock.items():
        path = directory / name
        path.write_bytes(data)
        asm = str(PS.PathologicScript(str(path)))
        calls = {int(op, 16): fn for op, fn in re.findall(r'^0x([0-9a-f]+): @ (\w+)\(', asm, re.M)}
        transits = [op for op, fn in calls.items() if fn == 'CameraTransit']
        assert len(transits) == expected_counts[name], (name, transits)
        # These stock scripts share SetDialogCamera first; boy/girl also
        # contain SetTradeCamera second. Labels were checked against their ASM.
        for index, op in enumerate(transits):
            expected = index == 0
            fixtures.append(str(int(expected)) + ' ' + ' '.join(calls.get(i, '-') for i in range(op-9, op+33)))
            sites.append({'script': name, 'op': hex(op), 'dialogue': expected,
                          'sha256': hashlib.sha256(data).hexdigest()})
    fixture_path = directory / 'calls.txt'
    fixture_path.write_text('\n'.join(fixtures)+'\n', encoding='ascii')
    subprocess.run([str(ROOT / 'build-win32/Release/camera_transition_test.exe'), str(fixture_path)], check=True)
    report = {'status': 'PASS', 'guard_byte_sequences': checked, 'vtable_slots': 7,
              'real_camera_call_sites': sites,
              'limitation': 'Static ABI and call-site checks; not a game runtime test'}
    (ROOT / 'release/abi-validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'PASS: {checked} ABI byte guards, 7 vtable slots and {len(sites)} real camera call sites')


if __name__ == '__main__':
    main()
