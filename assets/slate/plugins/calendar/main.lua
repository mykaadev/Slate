-- Month calendar in the note sidebar. Days with a journal entry are green; click any day to open it.

local months = { "January", "February", "March", "April", "May", "June", "July",
                 "August", "September", "October", "November", "December" }
local now = os.date("*t")
local shown = { year = now.year, month = now.month }
local entries = {}

local function scan()
  entries = {}
  for _, file in ipairs(slate.workspace.files()) do
    local date = file:match("^Journal/%d+/%d+/(%d%d%d%d%-%d%d%-%d%d)%.md$")
    if date then entries[date] = true end
  end
end

slate.on("ready", scan)
slate.on("document_save", scan)
slate.on("workspace_open", scan)

local function days_in_month(year, month)
  return os.date("*t", os.time { year = year, month = month + 1, day = 0 }).day
end

-- Monday-based weekday of the first day of the month, 0..6
local function first_weekday(year, month)
  local wday = os.date("*t", os.time { year = year, month = month, day = 1 }).wday
  return (wday + 5) % 7
end

local function open_day(date)
  local year, month = date:match("^(%d+)%-(%d+)")
  local path = string.format("Journal/%s/%s/%s.md", year, month, date)
  if not slate.workspace.exists(path) then
    slate.workspace.write(path, "# " .. date .. "\n\n---\n\n")
    slate.workspace.rescan()
    scan()
  end
  slate.document.open(path)
end

local function shift_month(delta)
  shown.month = shown.month + delta
  if shown.month < 1 then shown.month, shown.year = 12, shown.year - 1 end
  if shown.month > 12 then shown.month, shown.year = 1, shown.year + 1 end
end

slate.view.register {
  id = "calendar",
  title = "calendar",
  location = "sidebar",
  draw = function(ui, width)
    if ui.link("<") then shift_month(-1) end
    ui.same_line(10)
    ui.text(months[shown.month] .. " " .. shown.year, "cyan")
    ui.same_line(10)
    if ui.link(">") then shift_month(1) end

    local cell = math.floor((ui.width() - 6 * 2) / 7)
    local weekdays = { "M", "T", "W", "T", "F", "S", "S" }
    for index, day in ipairs(weekdays) do
      ui.button(day .. "##weekday" .. index, { width = cell, color = "muted" })
      if index < 7 then ui.same_line(2) end
    end

    local offset = first_weekday(shown.year, shown.month)
    local total = days_in_month(shown.year, shown.month)
    local today = os.date("%Y-%m-%d")
    local count = 0
    local column = 0
    for _ = 1, offset do
      ui.dummy(cell, 1)
      ui.same_line(2)
      column = column + 1
    end
    for day = 1, total do
      local date = string.format("%04d-%02d-%02d", shown.year, shown.month, day)
      local color = "muted"
      if entries[date] then
        color = "green"
        count = count + 1
      end
      if date == today then color = "amber" end
      if ui.button(tostring(day) .. "##" .. date, { width = cell, color = color }) then open_day(date) end
      column = column + 1
      if column % 7 ~= 0 and day < total then ui.same_line(2) end
    end
    ui.space(4)
    ui.muted(count .. " journal " .. (count == 1 and "entry" or "entries") .. " this month")
  end,
}
