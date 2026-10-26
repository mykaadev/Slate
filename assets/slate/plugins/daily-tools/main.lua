-- Date helpers for journaling.

slate.slash.register { id = "date", label = "date", description = "Insert today's date",
  run = function() return os.date("%Y-%m-%d") end }

slate.slash.register { id = "time", label = "time", description = "Insert the current time",
  run = function() return os.date("%H:%M") end }

slate.slash.register { id = "stamp", label = "timestamp", description = "Insert a dated log line",
  run = function() return "- " .. os.date("%H:%M") .. " — " end }

local function journal_path_for(offset_days)
  local t = os.time() + offset_days * 24 * 60 * 60
  return os.date("Journal/%Y/%m/%Y-%m-%d.md", t)
end

slate.command.register {
  id = "yesterday",
  label = "Open yesterday's journal",
  description = "Open the journal note from the day before",
  aliases = { "yesterday", "yd" },
  run = function()
    local path = journal_path_for(-1)
    if not slate.workspace.exists(path) then
      return "no journal for " .. path:match("(%d+%-%d+%-%d+)")
    end
    slate.document.open(path)
  end,
}

slate.home.add { id = "yesterday", section = "write", label = "yesterday", key = "y",
  command = "daily-tools.yesterday" }
