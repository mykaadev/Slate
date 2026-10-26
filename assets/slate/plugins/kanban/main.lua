-- Kanban board over Slate's todo documents (Todos/*.md).
-- Every card is a normal Markdown file; dragging a card rewrites its `status:` frontmatter.

local columns = {
  { state = "open", title = "Open", color = "muted" },
  { state = "research", title = "Research", color = "cyan" },
  { state = "doing", title = "Doing", color = "amber" },
  { state = "done", title = "Done", color = "green" },
}
local priority_color = { low = "muted", normal = "cyan", high = "amber", urgent = "red" }
local priority_rank = { urgent = 0, high = 1, normal = 2, low = 3 }
local next_priority = { low = "normal", normal = "high", high = "urgent", urgent = "low" }

local todos = {}
local stale = true
local filter = ""

local function refresh()
  todos = slate.todos.list()
  table.sort(todos, function(a, b)
    if priority_rank[a.priority] ~= priority_rank[b.priority] then
      return priority_rank[a.priority] < priority_rank[b.priority]
    end
    return a.title:lower() < b.title:lower()
  end)
  stale = false
end

local function mark_stale() stale = true end
slate.on("ready", mark_stale)
slate.on("document_save", mark_stale)
slate.on("workspace_open", mark_stale)
slate.timer.every(3, mark_stale)

local function matches(todo)
  if filter == "" then return true end
  local query = filter:lower()
  return todo.title:lower():find(query, 1, true) ~= nil
      or (todo.description or ""):lower():find(query, 1, true) ~= nil
end

local function new_card(state)
  slate.ui.prompt("new card", "", function(title)
    if title ~= "" then
      slate.todos.create { title = title, state = state }
      mark_stale()
    end
  end)
end

local function draw_column(ui, column, width, height)
  local cards = {}
  for _, todo in ipairs(todos) do
    if todo.state == column.state and matches(todo) then cards[#cards + 1] = todo end
  end

  local dragging = ui.dragging("slate-todo")
  ui.begin_panel("column-" .. column.state, width, height, {
    background = "panel",
    highlight = dragging and column.color or nil,
    padding = 10,
  })
  ui.text(column.title, column.color)
  ui.same_line(8)
  ui.badge(tostring(#cards), column.color)
  ui.space(4)

  for _, todo in ipairs(cards) do
    local action = ui.card {
      id = todo.path,
      title = todo.title,
      body = todo.description,
      accent = priority_color[todo.priority] or "muted",
      badge = todo.priority ~= "normal" and todo.priority or nil,
      badge_color = priority_color[todo.priority],
      drag_type = "slate-todo",
      drag_payload = todo.path,
    }
    ui.tooltip("click to open, right-click for priority, drag to move")
    if action == "click" then
      slate.document.open(todo.path)
    elseif action == "right" then
      slate.todos.update(todo.path, { priority = next_priority[todo.priority] })
      mark_stale()
    end
  end

  if ui.link("+ add card") then new_card(column.state) end
  ui.end_panel()

  -- The whole column is a drop zone
  local dropped = ui.drop_target("slate-todo")
  if dropped then
    slate.todos.update(dropped, { state = column.state })
    slate.notify("moved to " .. column.title:lower())
    mark_stale()
  end
end

slate.view.register {
  id = "kanban",
  title = "Kanban",
  draw = function(ui, width, height)
    if stale then refresh() end

    local done = 0
    for _, todo in ipairs(todos) do
      if todo.state == "done" then done = done + 1 end
    end

    filter = ui.input("kanban-filter", filter, "filter cards")
    ui.space(2)
    if #todos > 0 then
      ui.progress(done / #todos, "green")
      ui.muted(string.format("%d of %d done", done, #todos))
    end
    ui.space(6)

    if ui.key("n") then new_card("open") end

    local gap = 12
    local column_width = math.floor((ui.width() - gap * (#columns - 1)) / #columns)
    local column_height = ui.height() - 4
    for index, column in ipairs(columns) do
      draw_column(ui, column, column_width, column_height)
      if index < #columns then ui.same_line(gap) end
    end
  end,
}

slate.command.register {
  id = "open",
  label = "Open Kanban board",
  description = "Show todo documents as a board",
  aliases = { "kanban", "board" },
  run = function() slate.view.open("kanban") end,
}

slate.home.add { id = "board", section = "write", label = "board", key = "k", command = "kanban.open" }
