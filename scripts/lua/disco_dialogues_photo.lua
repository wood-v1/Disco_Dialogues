maintask DiscoDialoguesPhoto do
  local photo: string
  local width: int
  local height: int
  function init() -> void
    local conversation: object
    native.GetConversation(conversation)
    if conversation == null then return end
    conversation.GetPhoto(photo)
    native.LoadImage(photo)
    native.GetWindowSize(width, height)
    native.SetOwnerDraw(true)
    native.ProcessEvents()
  end
  function OnDraw() -> void
    native.StretchBlit(photo, 0, 0, width, height)
  end
  function OnLButtonDown(x: int, y: int) -> void
    -- Keep the stock portrait's click phase and toggle, within the existing feed.
    native.SendMessage(4101, "dialog_text")
  end
end
