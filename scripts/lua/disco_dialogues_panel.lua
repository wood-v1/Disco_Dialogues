import "disco_dialogues_theme"

maintask DiscoDialoguesPanel do
  local width: int
  local height: int
  local edgeWidth: int
  local dividerY: int
  function init() -> void
    native.GetWindowSize(width, height)
    edgeWidth = height * disco_dialogues_theme.BORDER_WIDTH / disco_dialogues_theme.PANEL_BASE_HEIGHT
    if edgeWidth < 1 then edgeWidth = 1 end
    dividerY = height * disco_dialogues_theme.HEADER_DIVIDER_Y / disco_dialogues_theme.PANEL_BASE_HEIGHT
    native.SetOwnerDraw(true)
    native.ProcessEvents()
  end
  function OnDraw() -> void
    -- The sixth HD StretchBlit argument affects only this image, not children.
    native.StretchBlit("default", 0, 0, width, height, disco_dialogues_theme.BACKGROUND_ALPHA)
    -- Continuous side strips and closed corners; no inset gaps or short stubs.
    -- Caps meet the sides without overlapping their alpha at the corners.
    native.StretchBlit("panel_edge", 0, 0, edgeWidth, height, disco_dialogues_theme.BORDER_ALPHA)
    native.StretchBlit("panel_edge", width - edgeWidth, 0, edgeWidth, height, disco_dialogues_theme.BORDER_ALPHA)
    native.StretchBlit("panel_edge", edgeWidth, 0, width - edgeWidth * 2, 1, disco_dialogues_theme.BORDER_CAP_ALPHA)
    native.StretchBlit("panel_edge", edgeWidth, height - 1, width - edgeWidth * 2, 1, disco_dialogues_theme.BORDER_CAP_ALPHA)
    -- In the existing gap between portrait/name and the text viewport.
    native.StretchBlit("panel_edge", edgeWidth, dividerY, width - edgeWidth * 2, 1, disco_dialogues_theme.HEADER_DIVIDER_ALPHA)
  end
end
