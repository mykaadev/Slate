-- Date helpers for journaling.

local date_formats = {
  ["2026-10-26"] = "%Y-%m-%d",
  ["26/10/2026"] = "%d/%m/%Y",
  ["10/26/2026"] = "%m/%d/%Y",
  ["October 26, 2026"] = "%B %d, %Y",
}

local function today()
  return os.date(date_formats[slate.settings.get("date_format")] or "%Y-%m-%d")
end

local function now()
  if slate.settings.get("clock") == "12 hour" then
    return (os.date("%I:%M %p"):gsub("^0", ""))
  end
  return os.date("%H:%M")
end

slate.slash.register { id = "date", label = "date", description = "Insert today's date",
  run = function() return today() end }

slate.slash.register { id = "time", label = "time", description = "Insert the current time",
  run = function() return now() end }

slate.slash.register { id = "stamp", label = "timestamp", description = "Insert a dated log line",
  run = function() return "- " .. now() .. " — " end }

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
