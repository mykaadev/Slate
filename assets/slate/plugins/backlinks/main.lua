-- Lists notes that link to the open note, in the note sidebar (ctrl+shift+b toggles it).

local cache = { path = nil, items = {} }

local function stem(path)
  return path:match("([^/]+)%.md$") or path
end

local function trim(text)
  return (text:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function scan()
  local current = slate.document.path()
  cache.path = current
  cache.items = {}
  if not current then return end

  local name = stem(current):lower()
  local without_extension = current:gsub("%.md$", ""):lower()
  for _, file in ipairs(slate.workspace.files()) do
    if file ~= current then
      local text = slate.workspace.read(file) or ""
      local hits = {}
      local line_number = 0
      for line in (text .. "\n"):gmatch("(.-)\r?\n") do
        line_number = line_number + 1
        local lower = line:lower()
        local found = false
        for target in lower:gmatch("%[%[([^%]|#]+)") do
          target = trim(target)
          if target == name or target == without_extension then found = true end
        end
        for target in lower:gmatch("%]%(([^%)]+)%)") do
          if target:find(name .. ".md", 1, true) then found = true end
        end
        if found then hits[#hits + 1] = { line = line_number, text = trim(line) } end
      end
      if #hits > 0 then cache.items[#cache.items + 1] = { path = file, hits = hits } end
    end
  end
end

slate.on("ready", scan)
slate.on("document_open", scan)
slate.on("document_save", scan)

slate.view.register {
  id = "backlinks",
  title = "backlinks",
  location = "sidebar",
  draw = function(ui)
    if cache.path ~= slate.document.path() then scan() end
    if #cache.items == 0 then
      ui.muted("no notes link here yet")
      return
    end
    for _, item in ipairs(cache.items) do
      if ui.link(stem(item.path)) then slate.document.open(item.path) end
      ui.tooltip(item.path)
      for index, hit in ipairs(item.hits) do
        if index > 2 then break end
        ui.markdown(hit.text)
      end
      ui.space(6)
    end
  end,
}
