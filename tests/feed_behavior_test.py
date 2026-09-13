"""Execute the real feed DSL AST against deterministic UI/conversation stubs.

This checks state/event behavior, not engine rendering, ABI or bytecode execution.
The external compiler remains responsible for DSL typing and BIN generation.
"""
import argparse
import json
import math
import operator
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--compiler-root', type=Path, required=True)
args = parser.parse_args()
sys.path.insert(0, str(args.compiler_root.resolve()))
import compiler  # Initialize the compiler package in its supported import order.
import dsl_ast as A
from dsl_parser import parse_program_text

TASK = parse_program_text((ROOT / 'scripts/lua/disco_dialogues_feed.lua').read_text(encoding='utf-8')).task
THEME = parse_program_text((ROOT / 'scripts/lua/disco_dialogues_theme.lua').read_text(encoding='utf-8')).module


class Returned(Exception):
    def __init__(self, value):
        self.value = value


class Feed:
    def __init__(self, width, height, line='Current line', replies=None, task=TASK):
        self.state = {}
        self.width, self.height = width, height
        self.line = line
        self.replies = [('Answer', 42, 7)] if replies is None else replies
        self.npc = 'Very long NPC name'
        self.player = 'Player'
        self.description = 'Character biography ' * 200
        self.messages = []
        self.blits = []
        self.chosen, self.draws, self.native_calls = [], [], []
        self.functions = {f.name: f for f in task.functions}
        self.theme = {var.name: self.expr(var.value, {}) for var in THEME.consts}
        for var in task.vars:
            self.state[var.name] = self.expr(var.init, {}) if var.init else self.default(var.type_name)
        for var in task.consts:
            self.state[var.name] = self.expr(var.value, {})
        self.call('init')

    @staticmethod
    def default(kind):
        return {'int': 0, 'float': 0.0, 'bool': False, 'string': '', 'object': None}[kind]

    def put(self, name, value, local):
        (local if name in local else self.state)[name] = value

    def call(self, name, *values):
        fn = self.functions[name]
        local = {p.name: v for p, v in zip(fn.params, values)}
        try:
            self.block(fn.body, local)
        except Returned as result:
            return result.value

    def block(self, statements, local):
        for s in statements:
            if isinstance(s, A.LocalDecl):
                local[s.name] = self.expr(s.init, local) if s.init else self.default(s.type_name)
            elif isinstance(s, A.Assign):
                self.put(s.target.value, self.expr(s.value, local), local)
            elif isinstance(s, A.ExprStmt):
                self.expr(s.expr, local)
            elif isinstance(s, A.IfStmt):
                self.block(s.then_body if self.expr(s.cond, local) else s.else_body, local)
            elif isinstance(s, A.ForStmt):
                step = self.expr(s.step, local) if s.step else 1
                for i in range(self.expr(s.start, local), self.expr(s.stop, local) + (1 if step > 0 else -1), step):
                    local[s.var_name] = i
                    self.block(s.body, local)
            elif isinstance(s, A.ReturnStmt):
                raise Returned(self.expr(s.value, local) if s.value else None)
            else:
                raise AssertionError(type(s))

    def expr(self, e, local):
        if isinstance(e, A.Attribute):
            assert e.base.value == 'disco_dialogues_theme'
            return self.theme[e.name]
        if isinstance(e, A.Name):
            return local[e.value] if e.value in local else self.state[e.value]
        if isinstance(e, A.Number):
            return float(e.text) if '.' in e.text else int(e.text)
        if isinstance(e, (A.StringLit, A.BoolLit)):
            return e.value
        if isinstance(e, A.NullLit):
            return None
        if isinstance(e, A.UnaryOp):
            value = self.expr(e.operand, local)
            return not value if e.op == '!' else -value
        if isinstance(e, A.BinaryOp):
            left = self.expr(e.left, local)
            if e.op == '&&': return bool(left and self.expr(e.right, local))
            if e.op == '||': return bool(left or self.expr(e.right, local))
            right = self.expr(e.right, local)
            if e.op == '+' and (isinstance(left, str) or isinstance(right, str)):
                return str(left) + str(right)
            if e.op == '/':
                return int(left / right) if isinstance(left, int) and isinstance(right, int) else left / right
            return {'+': operator.add, '-': operator.sub, '*': operator.mul,
                    '<': operator.lt, '>': operator.gt, '<=': operator.le, '>=': operator.ge,
                    '==': operator.eq, '!=': operator.ne}[e.op](left, right)
        if isinstance(e, A.Call):
            if isinstance(e.callee, A.Name):
                return self.call(e.callee.value, *(self.expr(v, local) for v in e.args))
            return self.native(e.callee.base.value, e.callee.name, e.args, local)
        raise AssertionError(type(e))

    def native(self, owner, name, args, local):
        def get(index): return self.expr(args[index], local)
        def out(index, value): self.put(args[index].value, value, local)
        if owner == 'conversation':
            out(0, {'GetNPCName': self.npc, 'GetPlayerName': self.player,
                    'GetNPCDescription': self.description, 'GetPhoto': 'portrait.tex'}[name])
            return
        if owner != 'native':
            vector = self.expr(A.Name(owner), local)
            if name == 'add': vector.append(get(0))
            elif name == 'get': out(0, vector[get(1)])
            elif name == 'size': out(0, len(vector))
            else: raise AssertionError(name)
            return
        self.native_calls.append(name)
        if name == 'GetWindowSize': out(0, self.width); out(1, self.height)
        elif name == 'GetFontHeight': out(0, 20)
        elif name in ('CreateStringVector', 'CreateIntVector'): out(0, [])
        elif name == 'GetConversation': out(0, self)
        elif name == 'GetReplic': out(0, self.line)
        elif name == 'GetAnswerCount': out(0, len(self.replies))
        elif name == '_strupr': out(0, get(0).upper())
        elif name == 'GetAnswer':
            for i, value in enumerate(self.replies[get(0)], 1): out(i, value)
        elif name == 'SelectAnswer':
            assert self.native_calls[-2] == 'PlaySound'
            self.chosen.append((get(0), get(1)))
        elif name == 'PlaySound': assert len(args) == 1 and get(0) == 'disco-dialogs-action'
        elif name == 'GetTextHeightInWidth':
            out(0, max(1, math.ceil(len(get(3)) * 9 / get(2))) * 20)
        elif name in ('DiscoDialoguesPrint', 'PrintInWidth'):
            self.draws.append(tuple(get(i) for i in range(1, len(args))))
            out(0, 0)
        elif name == 'SendMessage': self.messages.append((get(0), get(1)))
        elif name == 'StretchBlit': self.blits.append(tuple(get(i) for i in range(len(args))))
        elif name in ('EnableClipping', 'CaptureKeyboard', 'SetOwnerDraw', 'SetNeedUpdate',
                      'ProcessEvents', 'StretchBlit', 'HideCursor', 'CaptureMouse', 'ReleaseMouse', 'LoadImage'):
            pass
        else: raise AssertionError(name)


checks = []
for width, height in [(605, 820), (504, 683), (430, 583)]:
    feed = Feed(width, height, 'Long localized line ' * 150)
    s = feed.state
    assert s['answerMax'] == 0 and s['historyMax'] > 0
    assert s['historyScroll'] == s['historyMax']
    assert 'SetNeedUpdate' in feed.native_calls
    feed.call('OnMouseWheel', 20, 10, 3)
    manual = s['historyScroll']
    assert manual < s['historyMax']
    feed.call('OnDraw')
    feed.call('OnUpdate', 0.016)
    assert s['historyScroll'] == manual
    assert all(0 <= draw[-2] < height and draw[-2] + draw[-1] <= height for draw in feed.draws)
    feed.call('OnKeyDown', 268)
    feed.call('OnKeyUp', 256)
    assert feed.chosen == [(42, 7)] and s['pending'] and not s['rows']
    old_current = s['current']
    feed.line = 'Next localized line'
    feed.call('OnDraw')
    feed.call('OnKeyUp', 256)
    assert s['current'] == old_current and feed.chosen == [(42, 7)]
    feed.call('OnUpdate', 0.016)
    assert len(s['rows']) == 2 and s['current'].endswith(feed.line)
    assert s['historyScroll'] == s['historyMax'] and not s['pending']
    # Same prose and IDs can be a valid self loop. It must append exactly once.
    feed.call('OnMouseMove', 20, s['answerTop'] + 1)
    assert s['selected'] == 0
    feed.call('OnLButtonUp', 20, s['answerTop'] + 1)
    feed.call('OnUpdate', 0.016)
    feed.call('OnDraw')
    assert len(s['rows']) == 4 and len(feed.chosen) == 2
    assert s['rows'][1].endswith('Answer')
    feed.replies = [('Overflow answer ' * 8, 100+i, i) for i in range(20)]
    feed.call('OnUpdate', 0.016)
    assert s['answerMax'] > 0 and s['historyHeight'] >= height // 4
    assert feed.call('HitAnswer', width-2, s['answerTop']+1) == -1
    feed.call('OnKeyUp', 272)
    assert s['selected'] == 19 and s['answerScroll'] > 0
    feed.call('OnLButtonDown', width-5, s['answerTop']+3)
    assert s['dragging'] == 2
    feed.call('OnMouseMove', width-5, height-1)
    feed.call('OnLButtonUp', width-5, height-1)
    assert s['dragging'] == 0 and 'ReleaseMouse' in feed.native_calls
    assert 0 <= s['answerScroll'] <= s['answerMax']
    fresh = Feed(width, height)
    assert fresh.state['rows'] == [] and fresh.state['historyScroll'] == 0
    # All three viewports use a fixed round marker and reach both range ends.
    for viewport in (1, 12, 80, height):
        for maximum in (1, 100, 50000):
            size = feed.call('ThumbSize', viewport, maximum)
            assert size == min(12, viewport)
            for scroll in (0, maximum):
                feed.call('DrawScroll', 0, viewport, maximum, scroll)
                marker = feed.blits[-1]
                assert marker == ('feed_thumb', width-14, 0 if scroll == 0 else viewport-size, 12, size)

    # The same IDs must be chosen by a numbered option and a mouse click.
    replies = [('', 999, 999)] + [(f'Choice {i}', 80+i, 90+i) for i in range(5)]
    for index in range(5):
        keyboard = Feed(width, height, replies=replies)
        mouse = Feed(width, height, replies=replies)
        assert keyboard.state['answers'][index].startswith(f'{index+1}. ')
        keyboard.call('OnKeyDown', 49+index)
        y = mouse.state['answerTop'] + sum(mouse.state['answerHeights'][:index]) + index*5 + 1
        mouse.call('OnLButtonUp', 20, y)
        assert keyboard.chosen == mouse.chosen == [(80+index, 90+index)]
        assert keyboard.native_calls.count('PlaySound') == 1
        assert mouse.native_calls.count('PlaySound') == 1
        keyboard.call('OnUpdate', 0.016)
        keyboard.call('OnKeyDown', 49+index)  # repeat after pending has cleared
        assert len(keyboard.chosen) == 1
        keyboard.call('OnKeyUp', 49+index)
        keyboard.call('OnKeyDown', 49+index)
        assert len(keyboard.chosen) == 2

    for replies in [[], [('', 1, 2)], [('Only one', 1, 2)], [('One', 1, 2), ('Two', 3, 4)]]:
        limited = Feed(width, height, replies=replies)
        for key in (51, 52, 53, 201, 202):  # binding IDs are NOT UI virtual keys
            limited.call('OnKeyDown', key)
        assert not limited.chosen
        assert 'PlaySound' not in limited.native_calls
    stale = Feed(width, height)
    stale.call('OnMouseMove', 20, stale.state['answerTop']+1)
    stale.replies = []
    stale.call('OnKeyUp', 256)
    stale.call('OnKeyDown', 49)
    assert not stale.chosen
    assert 'PlaySound' not in stale.native_calls
    stale.state['conversation'] = None
    stale.call('ChooseNumber', 0)
    assert not stale.chosen

    info = Feed(width, height, 'Conversation line ' * 100)
    info.call('OnKeyDown', 49)
    info.call('OnUpdate', 0.016)
    info.call('OnKeyUp', 49)
    info.call('OnMouseWheel', 20, 10, 4)
    before = {k: info.state[k] for k in ('rows', 'rowKinds', 'current', 'signature', 'answers',
                                        'historyScroll', 'answerScroll', 'selected')}
    info.call('OnUIMessage', 4101, 'unrelated', None)
    assert info.state['viewMode'] == 0
    assert info.native_calls.count('PlaySound') == 1
    info.call('OnUIMessage', 4101, 'photo', None)
    assert info.state['viewMode'] == 1 and info.state['infoMax'] > 0
    assert info.native_calls.count('PlaySound') == 2
    info.draws.clear()
    info.call('OnDraw')
    assert len(info.draws) == 1
    assert info.draws[0][4] == info.npc.upper()+'\n\n'+info.description
    assert info.draws[0][-2:] == (0, height)
    info.call('OnMouseWheel', 20, 10, -4)
    assert info.state['infoScroll'] > 0
    info.call('OnLButtonDown', width-5, height-5)
    assert info.state['dragging'] == 3
    info.call('OnMouseMove', width-5, height-1)
    info.call('OnLButtonUp', width-5, height-1)
    assert info.state['dragging'] == 0 and 0 <= info.state['infoScroll'] <= info.state['infoMax']
    for key in range(49, 54): info.call('OnKeyDown', key)
    info.call('OnKeyUp', 256)
    info.call('OnLButtonUp', 20, info.state['answerTop']+1)
    info.call('OnUpdate', 0.016)
    assert len(info.chosen) == 1
    assert info.native_calls.count('PlaySound') == 2
    info.call('OnUIMessage', 4101, 'photo', None)
    assert info.state['viewMode'] == 0
    assert info.native_calls.count('PlaySound') == 3
    assert all(info.state[k] == v for k, v in before.items())
    info.call('OnKeyDown', 49)  # held while returning from information
    assert len(info.chosen) == 1
    info.call('OnKeyUp', 49)
    info.call('OnKeyDown', 49)
    assert len(info.chosen) == 2
    info.call('OnUIMessage', 4101, 'photo', None)  # pending choice cannot open info
    assert info.state['viewMode'] == 0
    assert info.native_calls.count('PlaySound') == 4  # Two choices, two accepted toggles.
    info.state['conversation'] = None
    info.call('OnUIMessage', 4101, 'photo', None)
    assert info.native_calls.count('PlaySound') == 4

    overflow = Feed(width, height, 'Old conversation ' * 100, replies=[('Long answer '*20, i, i+1) for i in range(12)])
    overflow.call('OnMouseWheel', 20, 10, 2)
    overflow.call('OnKeyUp', 272)
    positions = tuple(overflow.state[k] for k in ('historyScroll', 'answerScroll', 'selected'))
    assert positions[0] > 0 and positions[1] > 0 and positions[2] == 11
    overflow.call('OnUIMessage', 4101, 'photo', None)
    overflow.call('OnLButtonDown', width-5, height-5)
    overflow.call('OnUIMessage', 4101, 'photo', None)
    assert overflow.state['dragging'] == 0 and 'ReleaseMouse' in overflow.native_calls
    assert tuple(overflow.state[k] for k in ('historyScroll', 'answerScroll', 'selected')) == positions
    overflow.call('OnUIMessage', 4101, 'photo', None)
    overflow.line = 'Updated externally'
    overflow.replies = [('New branch', 800, 801)]
    overflow.call('OnUpdate', 0.016)
    overflow.call('OnUIMessage', 4101, 'photo', None)
    assert overflow.state['current'].endswith('Updated externally')
    assert overflow.state['answers'] == ['1. New branch'] and overflow.state['selected'] == -1

    # Speaker roles are explicit, even if names happen to be identical.
    colors = Feed(width, height)
    colors.npc = colors.player = '\u0415\u0432\u0430 \u042f\u043d'
    colors.call('OnUpdate', 0.016)
    colors.call('OnKeyDown', 49)
    colors.call('OnUpdate', 0.016)
    colors.call('OnDraw')
    assert colors.state['rowKinds'] == [1, 2]
    assert colors.state['rows'][0].startswith(colors.npc.upper())
    assert colors.npc == '\u0415\u0432\u0430 \u042f\u043d'  # source untouched
    for draw, prefix in zip(colors.draws[:3], ('NPC', 'PLAYER', 'NPC')):
        assert draw[5:8] == tuple(colors.theme[prefix+'_'+c] for c in 'RGB')
    assert colors.draws[-1][5:8] == tuple(colors.theme['ANSWER_'+c] for c in 'RGB')
    colors.call('OnMouseMove', 20, colors.state['answerTop']+1)
    colors.call('OnDraw')
    assert colors.draws[-1][5:8] == tuple(colors.theme['HOVER_'+c] for c in 'RGB')
    colors.call('DrawDialogueRow', 'System text', 99, 0)
    assert colors.draws[-1][5:8] == (colors.theme['NEUTRAL'],)*3
    checks.append({'width': width, 'height': height, 'status': 'PASS'})

for component in ('photo', 'title', 'panel'):
    task = parse_program_text((ROOT / f'scripts/lua/disco_dialogues_{component}.lua').read_text(encoding='utf-8')).task
    widget = Feed(174, 144, task=task)
    widget.call('OnDraw')
    if component == 'photo':
        widget.call('OnLButtonDown', 10, 10)
        assert widget.messages == [(4101, 'dialog_text')]
        assert len(widget.blits[0]) == 5  # portrait remains fully opaque
    if component == 'title':
        assert widget.draws[0][4] == widget.npc.upper()
    if component == 'panel':
        assert widget.blits[0] == ('default', 0, 0, 174, 144, widget.theme['BACKGROUND_ALPHA'])
        assert len(widget.blits) == 6
        assert all(b[0] == 'panel_edge' and (b[3] == 1 or b[4] == 1) for b in widget.blits[1:])
        assert 0.75 <= widget.theme['BACKGROUND_ALPHA'] <= 0.90
sys.path.insert(0, str(ROOT / 'scripts'))
from generate_layouts import SIZES
panel_task = parse_program_text((ROOT / 'scripts/lua/disco_dialogues_panel.lua').read_text(encoding='utf-8')).task
for screen_w, screen_h, panel_x, panel_y, panel_w, panel_h in SIZES:
    panel = Feed(panel_w, panel_h, task=panel_task)
    panel.call('OnDraw')
    edges = [blit[1:5] for blit in panel.blits[1:5]]
    def covers(rect, x, y):
        rx, ry, rw, rh = rect
        return rx <= x < rx + rw and ry <= y < ry + rh
    # Every perimeter pixel is covered exactly once, including all four corners.
    # This catches disconnected stubs/gaps and alpha doubled by overlapping caps.
    perimeter = {(x, y) for x in range(panel_w) for y in (0, panel_h-1)}
    perimeter |= {(x, y) for x in (0, panel_w-1) for y in range(panel_h)}
    assert all(sum(covers(rect, x, y) for rect in edges) == 1 for x, y in perimeter)
    for blit in panel.blits[1:]:
        x, y, w, h = blit[1:5]
        assert x >= 0 and y >= 0 and w > 0 and h > 0 and x+w <= panel_w and y+h <= panel_h
    divider = panel.blits[-1][1:5]
    layout = ET.parse(ROOT / f'resources/ui/disco_dialogues_{screen_w}x{screen_h}.xml').getroot()
    forms = {node.get('name'): node for node in layout.iter('form')}
    header_bottom = max(int(forms[name].get('y')) + int(forms[name].get('h')) for name in ('photo', 'name'))
    assert header_bottom < panel_y + divider[1]
    assert panel_y + divider[1] + divider[3] < int(forms['dialog_text'].get('y'))
    assert divider[0] == edges[0][2] and divider[0]+divider[2] == edges[1][0]

report = {'status': 'PASS', 'viewports': checks,
          'checks': ['manual scroll retention', 'new-line autoscroll', 'pending frame stability',
                     'duplicate input guard', 'identical branch self loop', 'hover and click IDs',
                     'HD key phases', 'dynamic answers and overflow', 'drag capture/release',
                     'draw clip bounds', 'fresh session state', 'number/mouse ID parity',
                     'empty replies and stale availability', 'key repeat across updates and modes',
                     'exclusive info draw and scroll', 'dialogue state restoration',
                     'role colors independent of names', 'uppercase display copies',
                     'portrait message routing', 'panel-only alpha',
                     'one sound per accepted portrait toggle', 'continuous perimeter without corner overlap',
                     'header divider clears portrait and feed at all resolutions'],
          'limitation': 'Real DSL AST with mocked natives; not engine/BIN execution or visual QA'}
(ROOT / 'release/feed-validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print(f'PASS: UI AST; {len(report["checks"])} behavioral check groups at 3 viewport sizes (mocked engine)')
