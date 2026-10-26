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
  local minutes = math.max(1, math.floor(words / 220 + 0.5))
  slate.status.set("words", string.format("%d words · %d min", words, minutes))
end

slate.on("ready", refresh)
slate.on("document_open", refresh)
slate.on("document_change", refresh)

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
