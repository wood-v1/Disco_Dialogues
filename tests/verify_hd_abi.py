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
from generate_layouts import read_vfs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--pathologic-re', type=Path, required=True)
    parser.add_argument('--oynon-root', type=Path, default=ROOT.parent / 'OynonTools')
    args = parser.parse_args()
    import pefile
    modules = {}
    for name, stamp, size in [('Game.exe', 0x5698d115, 0x4c4000),
                              ('Engine.dll', 0x561b8394, 0x26e000),
                              ('UI.dll', 0x5654c15f, 0xcf000),
                              ('Sound.dll', 0x565374bc, 0xc6000)]:
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
        # HD Resume reaches alSourcePlay even when the pause count was zero.
        ('Sound.dll', 0x2a120, '8b4914e9b83e0100'),
        ('Sound.dll', 0x3e008, 'ff4e6c837e6c00'),
        ('Sound.dll', 0x3e017, '7f0d807e680074078bcee8dac5ffff'),
        ('Sound.dll', 0x3a638, '50ff1594f10810'),
    ]:
        expected = bytes.fromhex(encoded)
        assert modules[module][1][offset:offset+len(expected)] == expected, (module, hex(offset))
        checked += 1
    for source in ['camera_hook.cpp', 'ui_execute_hook.cpp', 'script_audio_hooks.cpp']:
        text = (args.oynon_root / 'src/runtime' / source).read_text(encoding='utf-8')
        for symbol, address, encoded, length in re.findall(
                r'hd::Bytes\((g|e|ui|s), (0x[0-9a-f]+), "([^"]+)", (\d+)\)', text):
            module = {'g': 'Game.exe', 'e': 'Engine.dll', 'ui': 'UI.dll', 's': 'Sound.dll'}[symbol]
            expected = bytes.fromhex(encoded.replace('\\x', ''))
            assert len(expected) == int(length)
            image = modules[module][1]
            offset = int(address, 16)
            assert image[offset:offset+len(expected)] == expected, (source, address)
            checked += 1
    for module, slot, target in [('Engine.dll', 0x205684, 0x13a1e0),
                                  ('Game.exe', 0x37ba44, 0x23d21),
                                  ('UI.dll', 0xa4e00, 0x4390),
                                  ('Engine.dll', 0x200534, 0x8396),
                                  ('Game.exe', 0x36af60, 0x243d4),
                                  ('Game.exe', 0x36af30, 0x2133c),
                                  ('Game.exe', 0x371c44, 0x18b65),
                                  ('Game.exe', 0x37cc40, 0x1ac9e),
                                  ('Engine.dll', 0x20095c, 0x4d59),
                                  ('Sound.dll', 0x8f9c0, 0x29e90),
                                  ('Sound.dll', 0x8f994, 0x2a120),
                                  ('Sound.dll', 0x8f8ec, 0x28970)]:
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
    header = (args.oynon_root / 'src/runtime/camera_abi.h').read_text(encoding='utf-8')
    assert '>(camera, 10)(camera)' in header

    sys.path.insert(0, str(args.pathologic_re / 'parser/lib'))
    import PathologicScript as PS
    PS.IS_ALPHA = False
    directory = ROOT / 'tmp/camera-fixtures'
    directory.mkdir(parents=True, exist_ok=True)
    # Exercise native controller and task getters with the real hook tests.
    # Both bodies contain only local branches and indirect virtual calls.
    image = modules['Game.exe'][1]
    assert image[0x1ac9e:0x1aca3] == bytes.fromhex('e98d012500')
    getter_path = directory / 'speech-getters.bin'
    getter_path.write_bytes(image[0x151150:0x15116f] + image[0x26ae30:0x26ae34] +
                           modules['Engine.dll'][1][0xbfbe0:0xbfc35])
    subprocess.run([str(ROOT / 'build-win32/Release/dialog_speech_test.exe'), str(getter_path)], check=True)
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
    # Validate the actual stock reply path, rather than assuming an audio/UI
    # relationship from names. Both NPCs call StopSpeech before branch actions.
    speech_sites = []
    speech_stock = read_vfs(args.game_root / 'data/Scripts.vfs', ['NPC_Danko_Anna.bin', 'NPC_Danko_Rubin.bin'])
    for name, data in speech_stock.items():
        path = directory / name
        path.write_bytes(data)
        asm = str(PS.PathologicScript(str(path)))
        ops = {int(op, 16): text for op, text in re.findall(r'^0x([0-9a-f]+): (.*)$', asm, re.M)}
        events = [int(op, 16) for op in re.findall(r'EVENT_11 Op = 0x([0-9a-f]+) Vars = \(int, int\)', asm)]
        assert len(events) == {'NPC_Danko_Anna.bin': 9, 'NPC_Danko_Rubin.bin': 7}[name]
        for event in events:
            call = re.fullmatch(r'Call2 0x([0-9a-f]+)', ops[event + 4])
            assert call, (name, event, 'reply entry no longer calls speech helper')
            helper = int(call[1], 16)
            assert ops[helper + 4] == '@ lshStopSpeech()', (name, event, helper)
        speech_sites.append({'script': name, 'reply_handlers': len(events), 'sha256': hashlib.sha256(data).hexdigest()})
    report = {'status': 'PASS', 'guard_byte_sequences': checked, 'vtable_slots': 16,
              'native_speech_getters': 'Executed installed HD getter bodies with controller/task fixtures',
              'native_exit_resume': 'Executed installed Engine.dll ResumeWithFlags; reproduced voice restart and verified guarded exit',
              'real_camera_call_sites': sites,
              'real_speech_reply_sites': speech_sites,
              'limitation': 'Static ABI and call-site checks; not a game runtime test'}
    (ROOT / 'release/abi-validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'PASS: {checked} ABI byte guards, 16 vtable slots and {len(sites)} real camera call sites; native speech getters and exit resume executed')


if __name__ == '__main__':
    main()
