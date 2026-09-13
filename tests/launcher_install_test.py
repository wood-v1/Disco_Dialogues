"""Exercise the real Launcher CLI in a disposable game-root fixture, never the game."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--launcher', type=Path, required=True)
    parser.add_argument('--inventory-zip', type=Path, required=True)
    args = parser.parse_args()
    (ROOT / 'tmp').mkdir(exist_ok=True)
    fixture = ROOT / 'tmp' / ('launcher-' + uuid.uuid4().hex[:12])
    final = fixture / 'bin/Final'
    final.mkdir(parents=True)
    launcher = final / 'GameModLauncher.exe'
    shutil.copy2(args.launcher, launcher)
    # Install/delete do not execute Game.exe. A marker satisfies path validation.
    (final / 'Game.exe').write_bytes(b'Disco Dialogues package-test fixture; not executable')
    (fixture / 'data').mkdir()
    (fixture / 'data/config.ini').write_text('[Strings]\nmain=txt, 0\n', encoding='ascii')
    ini = final / 'GameModLauncher.ini'
    ini.write_text('[General]\nGamePath=Game.exe\n[Logging]\nEnabled=1\n[UserFixture]\nKeep=untouched\n', encoding='ascii')
    log = []
    def run(*command):
        result = subprocess.run([str(launcher), *map(str, command)], cwd=final,
                                capture_output=True, timeout=60)
        message = result.stdout.decode('cp1251', errors='replace') + result.stderr.decode('cp1251', errors='replace')
        log.append({'command': list(map(str, command)), 'exit': result.returncode, 'output': message})
        if result.returncode:
            (ROOT / 'release/launcher-validation.json').write_text(json.dumps(log, indent=2), encoding='utf-8')
            raise RuntimeError(message)
    run('install', '--zip', args.inventory_zip.resolve(), '--name', 'Inventory Overhaul', '--dll', 'InventoryOverhaul.dll')
    # Apply the DLL-only compatibility update in the disposable fixture, retaining
    # Inventory Overhaul's existing install ownership and every script/config.
    compatibility = ROOT / 'release/compatibility'
    for name in ('InventoryOverhaul.dll', 'OynonTools.dll'):
        shutil.copy2(compatibility / name, final / 'mods' / name)
    def inventory_snapshot():
        return {str(p.relative_to(fixture)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in fixture.rglob('*') if p.is_file() and
                (p.name.startswith('inv_overhaul_') or p.name.startswith('InventoryOverhaul.'))}
    before = inventory_snapshot()
    assert before
    shared = final / 'mods/OynonTools.dll'
    shared_hash = hashlib.sha256(shared.read_bytes()).hexdigest()
    package = ROOT / 'release/Pathologic_Disco_Dialogues_1_0_0.zip'
    run('install', '--zip', package, '--name', 'Disco Dialogues', '--dll', 'DiscoDialogues.dll', '--skip-dll', 'OynonTools.dll')
    assert inventory_snapshot() == before
    assert hashlib.sha256(shared.read_bytes()).hexdigest() == shared_hash
    with zipfile.ZipFile(package) as archive:
        owned = [name for name in archive.namelist() if 'DiscoDialogues.' in name or 'disco_dialogues_' in name or name == 'data/Sounds/disco-dialogs-action.ogg']
        for name in owned:
            assert (fixture / name).read_bytes() == archive.read(name)
    cfg = configparser.ConfigParser()
    cfg.read(ini, encoding='cp1251')
    assert cfg['UserFixture']['Keep'] == 'untouched' and cfg['Logging']['Enabled'] == '1'
    required = cfg['SharedDll:OynonTools.dll']['RequiredBy']
    assert 'disco' in required.lower() and 'inventory' in required.lower()
    run('delete', '--mod', 'DiscoDialogues.dll')
    for name in owned:
        assert not (fixture / name).exists(), name
    assert inventory_snapshot() == before
    assert shared.exists() and hashlib.sha256(shared.read_bytes()).hexdigest() == shared_hash
    cfg.read(ini, encoding='cp1251')
    assert cfg['UserFixture']['Keep'] == 'untouched' and cfg['Logging']['Enabled'] == '1'
    assert 'disco' not in cfg['SharedDll:OynonTools.dll']['RequiredBy'].lower()
    report = {'fixture': str(fixture), 'status': 'PASS', 'inventory_files_preserved': len(before),
              'checks': ['install ZIP using actual Launcher CLI', 'both shared dependency owners registered',
                         'user INI sections preserved', f'all {len(owned)} mod-owned files removed',
                         'Inventory Overhaul files and OynonTools DLL preserved'], 'commands': log}
    (ROOT / 'release/launcher-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: actual Launcher install/delete; {len(before)} Inventory files and shared DLL preserved')


if __name__ == '__main__':
    main()
