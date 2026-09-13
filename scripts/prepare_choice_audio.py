"""Verify the single bundled OGG; no converted audio is written to disk."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / 'resources/audio'

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    parser.parse_args()
    manifest = json.loads((AUDIO / 'choice-audio.json').read_text(encoding='utf-8'))
    path = AUDIO / 'disco-dialogs-action.ogg'
    data = path.read_bytes()
    assert data.startswith(b'OggS') and b'vorbis' in data[:64]
    assert manifest['file'] == path.name
    assert hashlib.sha256(data).hexdigest() == manifest['sha256']
    assert sorted(p.name for p in AUDIO.iterdir() if p.suffix in ('.ogg', '.wav')) == [path.name]
    print('PASS: one bundled disco-dialogs-action.ogg; unchanged source audio; no WAV duplicate')

if __name__ == '__main__':
    main()
