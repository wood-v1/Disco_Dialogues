-- One view of the live HD conversation. Only SelectAnswer mutates game state.
import "disco_dialogues_theme"

maintask DiscoDialoguesFeed do
  local width: int
  local height: int
  local fontHeight: int
  local rows: object
  local rowHeights: object
  local rowKinds: object
  local answers: object
  local answerHeights: object
  local answerIndices: object
  local conversation: object
  local current: string
  local signature: string
  local npcName: string
  local playerName: string
  local historyHeight: int
  local answerTop: int
  local answerHeight: int
  local historyScroll: int
  local answerScroll: int
  local historyMax: int
  local answerMax: int
  local selected: int = -1
  local pending: bool
  local pendingNpc: string
  local pendingPlayer: string
  local dragging: int
  local dragOffset: int
  local viewMode: int
  local infoText: string
  local infoScroll: int
  local infoMax: int
  local key1Held: bool
  local key2Held: bool
  local key3Held: bool
  local key4Held: bool
  local key5Held: bool
  local const DIALOGUE: int = 0
  local const CHARACTER_INFO: int = 1
  local const NPC: int = 1
  local const PLAYER: int = 2
  local const GAP: int = 12
  local const ANSWER_GAP: int = 5

  function init() -> void
    native.GetWindowSize(width, height)
    native.GetFontHeight(fontHeight, "default")
    native.CreateStringVector(rows)
    native.CreateIntVector(rowHeights)
    native.CreateIntVector(rowKinds)
    native.GetConversation(conversation)
    if conversation == null then return end
    native.EnableClipping(true)
    native.CaptureKeyboard()
    native.SetOwnerDraw(true)
    native.SetNeedUpdate(true)
    Refresh(true)
    native.ProcessEvents()
  end

  function Clamp(value: int, maximum: int) -> int
    if value < 0 then return 0 end
    if value > maximum then return maximum end
    return value
  end

  function AddRow(text: string, kind: int) -> void
    local rowHeight: int
    native.GetTextHeightInWidth(rowHeight, "default", width - 28, text)
    rows.add(text)
    rowHeights.add(rowHeight)
    rowKinds.add(kind)
  end

  function Refresh(forceEnd: bool) -> void
    if conversation == null then return end
    local replic: string
    native.GetReplic(replic)
    conversation.GetNPCName(npcName)
    conversation.GetPlayerName(playerName)
    -- Change display copies only, before measuring or composing paragraphs.
    native._strupr(npcName)
    native._strupr(playerName)
    local count: int
    native.GetAnswerCount(count)
    -- IDs distinguish branches with identical prose. A completed choice also
    -- forces a revision, so even an identical-text self loop remains usable.
    local nextSignature: string = npcName + "\n" + playerName + "\n" + replic
    for i = 0, count - 1 do
      local text: string
      local nextId: int
      local replyId: int
      native.GetAnswer(i, text, nextId, replyId)
      nextSignature = nextSignature + "\n" + nextId + ":" + replyId + ":" + text
    end
    if !forceEnd && nextSignature == signature then return end
    signature = nextSignature
    current = npcName + " — " + replic
    selected = -1
    answerScroll = 0
    native.CreateStringVector(answers)
    native.CreateIntVector(answerHeights)
    native.CreateIntVector(answerIndices)
    local answerTotal: int = 0
    local visibleCount: int = 0
    for i = 0, count - 1 do
      local text: string
      local rowHeight: int
      local nextId: int
      local replyId: int
      native.GetAnswer(i, text, nextId, replyId)
      -- HD exposes available replies, without disabled/type flags. Never make
      -- an empty, invisible reply selectable just by adding its number.
      if text != "" then
        visibleCount = visibleCount + 1
        text = visibleCount + ". " + text
        native.GetTextHeightInWidth(rowHeight, "default", width - 40, text)
        answers.add(text)
        answerHeights.add(rowHeight)
        answerIndices.add(i)
        if visibleCount > 1 then answerTotal = answerTotal + ANSWER_GAP end
        answerTotal = answerTotal + rowHeight
      end
    end
    -- Give answers their measured height before enabling overflow. Reserve a
    -- quarter of the view (at least three lines) for the conversation.
    local minimumHistory: int = height / 4
    if minimumHistory < fontHeight * 3 then minimumHistory = fontHeight * 3 end
    answerHeight = answerTotal
    if answerHeight > height - minimumHistory - GAP then answerHeight = height - minimumHistory - GAP end
    if answerHeight < 1 then answerHeight = 1 end
    answerTop = height - answerHeight
    historyHeight = answerTop - GAP
    local historyTotal: int = 0
    rows.size(count)
    for i = 0, count - 1 do
      local rowHeight: int
      rowHeights.get(rowHeight, i)
      historyTotal = historyTotal + rowHeight + GAP
    end
    local currentHeight: int
    native.GetTextHeightInWidth(currentHeight, "default", width - 28, current)
    historyMax = historyTotal + currentHeight - historyHeight
    if historyMax < 0 then historyMax = 0 end
    answerMax = answerTotal - answerHeight
    if answerMax < 0 then answerMax = 0 end
    historyScroll = historyMax
    if dragging != 0 then dragging = 0 native.ReleaseMouse() end
  end

  function OnUpdate(delta: float) -> void
    -- World processes the dialogue actor before updating the override UI.
    local follow: bool = pending
    if pending then
      AddRow(pendingNpc, NPC)
      AddRow(pendingPlayer, PLAYER)
    end
    pending = false
    if viewMode == DIALOGUE then Refresh(follow) end
  end

  function OnUIMessage(message: int, sender: string, data: object) -> void
    if message != 4101 || sender != "photo" || conversation == null then return end
    if dragging != 0 then dragging = 0 native.ReleaseMouse() end
    if viewMode == CHARACTER_INFO then
      viewMode = DIALOGUE
      -- An unchanged conversation retains rows, answers and both scrolls.
      Refresh(false)
    else
      if pending then return end
      Refresh(false)
      local description: string
      conversation.GetNPCDescription(description)
      infoText = npcName + "\n\n" + description
      local textHeight: int
      native.GetTextHeightInWidth(textHeight, "default", width - 28, infoText)
      infoMax = textHeight - height
      if infoMax < 0 then infoMax = 0 end
      infoScroll = 0
      viewMode = CHARACTER_INFO
    end
    -- Sound belongs to the accepted toggle, not to both photo and feed handlers.
    native.PlaySound("disco-dialogs-action")
  end

  function DrawDialogueRow(text: string, kind: int, y: int) -> void
    local r: float = disco_dialogues_theme.NEUTRAL
    local g: float = disco_dialogues_theme.NEUTRAL
    local b: float = disco_dialogues_theme.NEUTRAL
    if kind == NPC then
      r = disco_dialogues_theme.NPC_R
      g = disco_dialogues_theme.NPC_G
      b = disco_dialogues_theme.NPC_B
    end
    if kind == PLAYER then
      r = disco_dialogues_theme.PLAYER_R
      g = disco_dialogues_theme.PLAYER_G
      b = disco_dialogues_theme.PLAYER_B
    end
    local drawn: int
    native.DiscoDialoguesPrint(drawn, "default", 0, y, width - 28, text, r, g, b, 0, historyHeight)
  end

  function ThumbSize(viewport: int, maximum: int) -> int
    -- A fixed marker; drawing and dragging share the same travel calculation.
    local size: int = disco_dialogues_theme.SCROLL_DIAMETER
    if size > viewport then size = viewport end
    return size
  end
  function DrawScroll(top: int, viewport: int, maximum: int, scroll: int) -> void
    if maximum <= 0 then return end
    local size: int = ThumbSize(viewport, maximum)
    local y: int = top + scroll * (viewport - size) / maximum
    native.StretchBlit("feed_track", width - 8, top, 1, viewport, disco_dialogues_theme.TRACK_ALPHA)
    native.StretchBlit("feed_thumb", width - 14, y, 12, size)
  end

  function OnDraw() -> void
    if conversation == null then return end
    if viewMode == CHARACTER_INFO then
      local drawn: int
      native.DiscoDialoguesPrint(drawn, "default", 0, -infoScroll, width - 28, infoText, disco_dialogues_theme.NEUTRAL, disco_dialogues_theme.NEUTRAL, disco_dialogues_theme.NEUTRAL, 0, height)
      DrawScroll(0, height, infoMax, infoScroll)
      return
    end
    if !pending then Refresh(false) end
    local count: int
    rows.size(count)
    local y: int = -historyScroll
    for i = 0, count - 1 do
      local text: string
      local rowHeight: int
      rows.get(text, i)
      rowHeights.get(rowHeight, i)
      if y + rowHeight > 0 && y < historyHeight then
        local kind: int
        rowKinds.get(kind, i)
        DrawDialogueRow(text, kind, y)
      end
      y = y + rowHeight + GAP
    end
    DrawDialogueRow(current, NPC, y)
    y = -answerScroll
    answers.size(count)
    for i = 0, count - 1 do
      local text: string
      local rowHeight: int
      answers.get(text, i)
      answerHeights.get(rowHeight, i)
      if y + rowHeight > 0 && y < answerHeight then
        local drawn: int
        if i == selected then
          native.DiscoDialoguesPrint(drawn, "default", 12, y, width - 40, text, disco_dialogues_theme.HOVER_R, disco_dialogues_theme.HOVER_G, disco_dialogues_theme.HOVER_B, answerTop, answerHeight)
        else
          native.DiscoDialoguesPrint(drawn, "default", 12, y, width - 40, text, disco_dialogues_theme.ANSWER_R, disco_dialogues_theme.ANSWER_G, disco_dialogues_theme.ANSWER_B, answerTop, answerHeight)
        end
      end
      y = y + rowHeight + ANSWER_GAP
    end
    DrawScroll(0, historyHeight, historyMax, historyScroll)
    DrawScroll(answerTop, answerHeight, answerMax, answerScroll)
  end

  function HitAnswer(x: int, y: int) -> int
    if viewMode != DIALOGUE || pending || x < 0 || x >= width - 24 || y < answerTop || y >= height then return -1 end
    local count: int
    answers.size(count)
    local position: int = answerTop - answerScroll
    for i = 0, count - 1 do
      local rowHeight: int
      answerHeights.get(rowHeight, i)
      if y >= position && y < position + rowHeight then return i end
      position = position + rowHeight + ANSWER_GAP
    end
    return -1
  end
  function Choose() -> void
    if viewMode != DIALOGUE || pending || conversation == null then return end
    Refresh(false)
    local count: int
    answers.size(count)
    if selected < 0 || selected >= count then return end
    local replic: string
    local answer: string
    local nextId: int
    local replyId: int
    native.GetReplic(replic)
    local answerIndex: int
    answerIndices.get(answerIndex, selected)
    native.GetAnswerCount(count)
    if answerIndex < 0 || answerIndex >= count then return end
    native.GetAnswer(answerIndex, answer, nextId, replyId)
    if answer == "" then return end
    native.PlaySound("disco-dialogs-action")
    native.SelectAnswer(nextId, replyId)
    -- Keep the last complete frame until the actor has processed the choice.
    pendingNpc = npcName + " — " + replic
    pendingPlayer = playerName + " — " + answer
    selected = -1
    pending = true
  end

  function Drag(y: int) -> void
    local top: int = 0
    local viewport: int = historyHeight
    local maximum: int = historyMax
    if dragging == 2 then top = answerTop viewport = answerHeight maximum = answerMax end
    if dragging == 3 then viewport = height maximum = infoMax end
    local travel: int = viewport - ThumbSize(viewport, maximum)
    if travel <= 0 then return end
    local position: int = Clamp(y - top - dragOffset, travel)
    if dragging == 3 then
      infoScroll = position * maximum / travel
    else
      if dragging == 1 then historyScroll = position * maximum / travel
      else answerScroll = position * maximum / travel end
    end
  end
  function OnLButtonDown(x: int, y: int) -> void
    if pending then return end
    if viewMode == DIALOGUE then Refresh(false) end
    if x < width - 20 || x >= width || y < 0 || y >= height then return end
    local top: int = 0
    local viewport: int = historyHeight
    local maximum: int = historyMax
    local scroll: int = historyScroll
    dragging = 1
    if viewMode == CHARACTER_INFO then
      dragging = 3 viewport = height maximum = infoMax scroll = infoScroll
    else
      if y >= answerTop then
        dragging = 2 top = answerTop viewport = answerHeight maximum = answerMax scroll = answerScroll
      end
    end
    if maximum <= 0 || y >= top + viewport then dragging = 0 return end
    local size: int = ThumbSize(viewport, maximum)
    local thumb: int = top + scroll * (viewport - size) / maximum
    dragOffset = size / 2
    if y >= thumb && y < thumb + size then dragOffset = y - thumb end
    native.CaptureMouse()
    if viewMode == DIALOGUE then selected = -1 end
    Drag(y)
  end
  function OnMouseMove(x: int, y: int) -> void
    if dragging != 0 then Drag(y) return end
    if viewMode != DIALOGUE then return end
    if !pending then Refresh(false) end
    selected = HitAnswer(x, y)
  end
  function OnMouseLeave() -> void
    if viewMode != DIALOGUE then return end
    if dragging == 0 then selected = -1 end
  end
  function OnLButtonUp(x: int, y: int) -> void
    if dragging != 0 then
      Drag(y) dragging = 0 native.ReleaseMouse() return
    end
    if viewMode != DIALOGUE then return end
    if !pending then Refresh(false) end
    selected = HitAnswer(x, y)
    Choose()
  end
  function OnMouseWheel(x: int, y: int, delta: float) -> void
    local movement: int = delta * fontHeight
    if viewMode == CHARACTER_INFO then
      infoScroll = Clamp(infoScroll - movement, infoMax)
      return
    end
    if y >= answerTop && answerMax > 0 then answerScroll = Clamp(answerScroll - movement, answerMax)
    else historyScroll = Clamp(historyScroll - movement, historyMax) end
    selected = HitAnswer(x, y)
  end

  function MoveSelection(step: int) -> void
    if viewMode != DIALOGUE || pending || conversation == null then return end
    Refresh(false)
    local count: int
    answers.size(count)
    if count <= 0 then return end
    native.HideCursor()
    if selected == -1 then
      if step > 0 then selected = 0 else selected = count - 1 end
    else
      selected = selected + step
      if selected < 0 then selected = count - 1 end
      if selected >= count then selected = 0 end
    end
    local top: int = 0
    for i = 0, selected - 1 do
      local rowHeight: int
      answerHeights.get(rowHeight, i)
      top = top + rowHeight + ANSWER_GAP
    end
    local rowHeight: int
    answerHeights.get(rowHeight, selected)
    if top < answerScroll then answerScroll = top end
    if top + rowHeight > answerScroll + answerHeight then
      answerScroll = top + rowHeight - answerHeight
      if rowHeight > answerHeight then answerScroll = top end
    end
    answerScroll = Clamp(answerScroll, answerMax)
  end
  -- Same HD keyboard/gamepad codes and event phases as the stock answer view.
  -- HD UI callbacks receive virtual keys (49..53 for the number row).
  -- The binding IDs 201..204 registered at Game.exe RVA 0x216eb8 are a
  -- different namespace; stock gamepad UI handlers also use virtual keys.
  function ChooseNumber(index: int) -> void
    if viewMode != DIALOGUE || pending || conversation == null then return end
    Refresh(false)
    local count: int
    answers.size(count)
    if index < 0 || index >= count then return end
    selected = index
    Choose()
  end
  function OnKeyDown(key: int) -> void
    -- Latches survive view switches and conversation revisions until key-up.
    if key == 49 then
      if !key1Held then key1Held = true ChooseNumber(0) end
      return
    end
    if key == 50 then
      if !key2Held then key2Held = true ChooseNumber(1) end
      return
    end
    if key == 51 then
      if !key3Held then key3Held = true ChooseNumber(2) end
      return
    end
    if key == 52 then
      if !key4Held then key4Held = true ChooseNumber(3) end
      return
    end
    if key == 53 then
      if !key5Held then key5Held = true ChooseNumber(4) end
      return
    end
    if viewMode == CHARACTER_INFO then
      if key == 267 then infoScroll = Clamp(infoScroll - fontHeight, infoMax) end
      if key == 268 then infoScroll = Clamp(infoScroll + fontHeight, infoMax) end
      return
    end
    if key == 267 then MoveSelection(-1) end
    if key == 268 then MoveSelection(1) end
  end
  function OnKeyUp(key: int) -> void
    if key == 49 then key1Held = false return end
    if key == 50 then key2Held = false return end
    if key == 51 then key3Held = false return end
    if key == 52 then key4Held = false return end
    if key == 53 then key5Held = false return end
    if viewMode == CHARACTER_INFO then
      if key == 272 then infoScroll = Clamp(infoScroll - fontHeight, infoMax) end
      if key == 274 then infoScroll = Clamp(infoScroll + fontHeight, infoMax) end
      return
    end
    if key == 272 then MoveSelection(-1) end
    if key == 274 then MoveSelection(1) end
    if key == 256 then Choose() end
  end
end
