import "disco_dialogues_theme"

maintask DiscoDialoguesTitle do
  local conversation: object
  local width: int
  local height: int
  function init() -> void
    native.GetConversation(conversation)
    if conversation == null then return end
    native.GetWindowSize(width, height)
    native.EnableClipping(true)
    native.SetOwnerDraw(true)
    native.ProcessEvents()
  end
  function OnDraw() -> void
    local name: string
    local drawn: int
    conversation.GetNPCName(name)
    native._strupr(name)
    native.PrintInWidth(drawn, "default", 0, 0, width, name, disco_dialogues_theme.NPC_R, disco_dialogues_theme.NPC_G, disco_dialogues_theme.NPC_B)
  end
end
