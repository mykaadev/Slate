-- Shows "123 words · 1 min" on the right side of the status line.

local function count_words(text)
  -- Skip frontmatter and markup characters so the number matches what you read
  text = text:gsub("^%-%-%-\n.-\n%-%-%-\n", "")
  local words = 0
  for _ in text:gmatch("[%w\128-\255][%w\128-\255'%-]*") do
    words = words + 1
  end
  return words
end

local function refresh()
  local text = slate.editor.text()
  if not text then
    slate.status.clear("words")
    return
  end
  local words = count_words(text)
  local parts = { words .. " words" }
  if slate.settings.get("count_characters") then
    parts[#parts + 1] = (utf8.len(text) or #text) .. " chars"
  end
  if slate.settings.get("show_reading_time") then
    local minutes = math.max(1, math.floor(words / slate.settings.get("words_per_minute") + 0.5))
    parts[#parts + 1] = minutes .. " min"
  end
  slate.status.set("words", table.concat(parts, " · "))
end

slate.on("ready", refresh)
slate.on("document_open", refresh)
slate.on("document_change", refresh)
slate.on("settings_changed", refresh)

slate.command.register {
  id = "show",
  label = "Count words",
  description = "Show the word count of the open note",
  aliases = { "wc", "words" },
  run = function()
    local text = slate.editor.text()
    if not text then
      return "no note open"
    end
    return count_words(text) .. " words"
  end,
}
