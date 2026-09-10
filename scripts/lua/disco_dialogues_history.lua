-- The HD history message protocol is unchanged. Only presentation is replaced:
-- speaker and utterance share one wrapped paragraph instead of overlapping columns.
maintask DiscoDialoguesHistory do
  local width: int
  local height: int
  local scroll: int
  local maximum: int
  local rows: object
  local npcName: string
  local playerName: string
  local const GAP: int = 12

  function init() -> void
    native.GetWindowSize(width, height)
    native.CreateStringVector(rows)
    scroll = 0
    maximum = 0
    local conversation: object
    native.GetConversation(conversation)
    if conversation == null then return end
    conversation.GetNPCName(npcName)
    conversation.GetPlayerName(playerName)
    native.EnableClipping(true)
    native.SetOwnerDraw(true)
    UpdateScroll(true)
    native.ProcessEvents()
  end

  function Measure() -> int
    local count: int
    rows.size(count)
    local total: int = 0
    for i = 0, count - 1 do
      local text: string
      local rowHeight: int
      rows.get(text, i)
      native.GetTextHeightInWidth(rowHeight, "default", width, text)
      total = total + rowHeight
      if i + 1 < count then total = total + GAP end
    end
    return total
  end

  function UpdateScroll(followEnd: bool) -> void
    maximum = Measure() - height
    if maximum < 0 then maximum = 0 end
    if followEnd then scroll = -maximum end
    if scroll < -maximum then scroll = -maximum end
    if scroll > 0 then scroll = 0 end
    if maximum == 0 then
      native.SendMessage(16384, "h_scrollbar")
    else
      local percent: int = -scroll * 100 / maximum
      native.SendMessage(percent, "h_scrollbar")
    end
  end

  function OnDraw() -> void
    local count: int
    rows.size(count)
    local y: int = scroll
    for i = 0, count - 1 do
      local text: string
      local rowHeight: int
      rows.get(text, i)
      native.PrintInWidth(rowHeight, "default", 0, y, width, text, 0.804, 0.804, 0.804)
      y = y + rowHeight + GAP
    end
  end

  function OnUIMessage(message: int, sender: string, data: object) -> void
    if sender == "h_scrollbar" then
      local percent: int = message
      if percent < 0 then percent = 0 end
      if percent > 100 then percent = 100 end
      scroll = -maximum * percent / 100
      return
    end
    if sender == "dialog_text" then
      if data == null then return end
      local count: int
      data.size(count)
      if count < 2 then return end
      local replic: string
      local answer: string
      data.get(replic, 0)
      data.get(answer, 1)
      local npcRow: string = npcName + " — " + replic
      local playerRow: string = playerName + " — " + answer
      rows.add(npcRow)
      rows.add(playerRow)
      UpdateScroll(true)
    end
  end

  function OnMouseWheel(x: int, y: int, delta: float) -> void
    local lineHeight: int
    native.GetFontHeight(lineHeight, "default")
    local movement: int = delta * lineHeight
    scroll = scroll + movement
    UpdateScroll(false)
  end
end
