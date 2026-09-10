"""Convert the bundled inv-action.ogg once; ordinary builds verify cached audio."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / 'resources/audio'


def verify_wav(path):
    with wave.open(str(path), 'rb') as stream:
        assert stream.getcomptype() == 'NONE' and stream.getsampwidth() == 2
        assert stream.getnchannels() in (1, 2)
        assert 8000 <= stream.getframerate() <= 192000
        assert 0 < stream.getnframes() <= stream.getframerate() * 10
        samples = stream.readframes(stream.getnframes())
        assert len(samples) == stream.getnframes() * stream.getnchannels() * 2
        assert any(samples), 'Bundled choice sound is silent'
        return {'channels': stream.getnchannels(), 'rate': stream.getframerate(),
                'frames': stream.getnframes(), 'duration_seconds': stream.getnframes() / stream.getframerate()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    source, target, manifest = (AUDIO / name for name in ('inv-action.ogg', 'inv-action.wav', 'choice-audio.json'))
    if not args.check:
        subprocess.run([args.ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error', '-y',
                        '-i', str(source), '-map_metadata', '-1', '-c:a', 'pcm_s16le',
                        '-fflags', '+bitexact', str(target)], check=True)
    report = {'source': source.name, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'runtime': target.name, 'runtime_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
              **verify_wav(target)}
    if args.check:
        assert json.loads(manifest.read_text(encoding='utf-8')) == report, 'Audio assets changed; regenerate with --ffmpeg'
    else:
        manifest.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: bundled inv-action.ogg -> PCM16 WAV, {report["duration_seconds"]:.3f}s, {report["rate"]} Hz, {report["channels"]} channels')


if __name__ == '__main__':
    main()
