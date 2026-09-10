"""Compile via the external DSL compiler with locally verified HD UI arities."""
import argparse
from pathlib import Path
import sys
from generate_layouts import ROOT, DEFAULT_GAME, read_vfs


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler-root', type=Path, required=True)
    parser.add_argument('--pathologic-re', type=Path, required=True)
    parser.add_argument('--game-root', type=Path, default=DEFAULT_GAME)
    args = parser.parse_args()
    sys.path.insert(0, str(args.compiler_root.resolve()))
    import compiler
    from asm_backend_core.native_api import NATIVE_FUNCTION_ARITIES
    sys.path.insert(0, str(args.pathologic_re / 'parser/lib'))
    import PathologicScript as PS
    PS.IS_ALPHA = False
    baseline_dir = ROOT / 'tmp/history-baseline'
    baseline_dir.mkdir(parents=True, exist_ok=True)
    stock = read_vfs(args.game_root / 'data/Scripts.vfs', ['ui_dialog_history.bin', 'ui_dialog_text.bin', 'ui_scrollbar.bin',
                                                       'ui_dialog_btext.bin', 'ui_dialog_photo.bin', 'ui_dialog_title.bin'])
    vanilla = stock['ui_dialog_history.bin']
    baseline = baseline_dir / 'ui_dialog_history.bin'
    baseline.write_bytes(vanilla)
    baseline_asm = str(PS.PathologicScript(str(baseline)))
    for signature in ['EnableClipping (1 args)', 'PrintInWidth (9 args)']:
        if signature not in baseline_asm:
            raise ValueError('HD native signature not verified: ' + signature)
    (baseline_dir / 'ui_dialog_history.asm').write_text(baseline_asm, encoding='utf-8')
    # The external registry omits EnableClipping and lists only PrintInWidth(10).
    # Extend this process only, using arities proven by the installed HD BIN.
    NATIVE_FUNCTION_ARITIES['EnableClipping'] = (1,)
    NATIVE_FUNCTION_ARITIES['PrintInWidth'] = (9, 10)
    for name in ['ui_dialog_text.bin', 'ui_scrollbar.bin', 'ui_dialog_btext.bin', 'ui_dialog_photo.bin', 'ui_dialog_title.bin']:
        path = baseline_dir / name
        path.write_bytes(stock[name])
        baseline_asm += str(PS.PathologicScript(str(path)))
    for name, arities in {'GetAnswer': (2, 3, 4), 'SelectAnswer': (2,),
                         'CaptureKeyboard': (0,), 'CaptureMouse': (0,), 'ReleaseMouse': (0,)}.items():
        for arity in arities:
            if f'{name} ({arity} args)' not in baseline_asm:
                raise ValueError('HD signature not verified: ' + name)
        NATIVE_FUNCTION_ARITIES[name] = arities
    for signature in ['_strupr (1 args)', 'SendMessage (2 args)', 'GetNPCDescription(', 'GetPhoto(']:
        if signature not in baseline_asm:
            raise ValueError('HD presentation contract not verified: ' + signature)
    # Our scoped HD draw bridge, implemented in src/dialog_feed.cpp.
    NATIVE_FUNCTION_ARITIES['DiscoDialoguesPrint'] = (11,)
    NATIVE_FUNCTION_ARITIES['DiscoDialoguesChoiceSound'] = (0,)
    api = compiler.load_assembler_api(args.pathologic_re)
    compiled = {}
    for component, events in {'feed': [0, 1, 2, 3, 8, 10, 15, 101, 102, 200],
                              'panel': [0], 'photo': [0, 2], 'title': [0]}.items():
        source = ROOT / f'scripts/lua/disco_dialogues_{component}.lua'
        _, bin_path = compiler.compile_lua_source(source.read_text(encoding='utf-8'), source, ROOT / 'tmp/history-build', api)
        assembled = str(PS.PathologicScript(str(bin_path)))
        for event in [f'EVENT_{n} ' for n in events]:
            if event not in assembled:
                raise ValueError(f'{component}: missing HD callback: ' + event)
        if api.assemble(api.parse_asm(assembled), is_alpha=False) != bin_path.read_bytes():
            raise ValueError(component + ': BIN disassembly/reassembly differs')
        for forbidden in ['SetMessage', 'AddReply', 'ClearReplies', 'CreateWindow']:
            if forbidden in assembled:
                raise ValueError(component + ': unexpected game/window mutation: ' + forbidden)
        compiled[bin_path.name] = bin_path.read_bytes()
    (ROOT / 'resources/scripts').mkdir(parents=True, exist_ok=True)
    for name, data in compiled.items():
        (ROOT / 'resources/scripts' / name).write_bytes(data)
    print('PASS: four UI scripts compiled; HD contracts/callbacks; exact BIN round trips')


if __name__ == '__main__':
    main()
