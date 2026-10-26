-- Small editing helpers. Each one is a command, so they show in the palette (ctrl+shift+p).

slate.command.register {
  id = "toggle-task",
  label = "Toggle task",
  description = "Turn the current line into a task, or check/uncheck it",
  run = function()
    local line = slate.editor.line()
    if not line then return end
    local indent, rest = line:match("^(%s*)(.*)$")
    if rest:match("^[-*+] %[ %]") then
      rest = rest:gsub("^([-*+]) %[ %]", "%1 [x]", 1)
    elseif rest:match("^[-*+] %[[xX]%]") then
      rest = rest:gsub("^([-*+]) %[[xX]%]", "%1 [ ]", 1)
    elseif rest:match("^[-*+] ") then
      rest = rest:gsub("^([-*+]) ", "%1 [ ] ", 1)
    else
      rest = "- [ ] " .. rest
    end
    slate.editor.set_line(indent .. rest)
  end,
}
slate.keymap.set("ctrl+enter", "editing-tools.toggle-task")

slate.command.register {
  id = "sort-lines",
  label = "Sort selected lines",
  description = "Sort the selected lines alphabetically",
  aliases = { "sort" },
  run = function()
    local selection = slate.editor.selection()
    if selection == "" then return "select some lines first" end
    local lines = {}
    for line in (selection .. "\n"):gmatch("(.-)\r?\n") do lines[#lines + 1] = line end
    if lines[#lines] == "" then table.remove(lines) end
    table.sort(lines, function(a, b) return a:lower() < b:lower() end)
    slate.editor.replace_selection(table.concat(lines, "\n"))
  end,
}

-- Optional: strip trailing whitespace on save. Lines ending in two spaces are Markdown
-- hard breaks, so those are kept. Toggle with :trim-on-save
slate.command.register {
  id = "trim-on-save",
  label = "Toggle trim on save",
  aliases = { "trim-on-save" },
  run = function()
    local enabled = not slate.settings.get("trim_on_save", false)
    slate.settings.set("trim_on_save", enabled)
    return "trim on save " .. (enabled and "on" or "off")
  end,
}

slate.on("document_before_save", function(path, text)
  if not slate.settings.get("trim_on_save", false) then return end
  return (text:gsub("([ \t]+)(\r?\n)", function(space, eol)
    if space == "  " then return space .. eol end
    return eol
  end))
end)
