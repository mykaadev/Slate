-- Theme-only mod: Slate picks up themes/shell/*.tsv and themes/markdown/*.tsv from any enabled plugin.
-- Pick it in config -> shell palette, or run:  :phosphor
slate.command.register {
  id = "apply",
  label = "Use Phosphor theme",
  aliases = { "phosphor" },
  run = function()
    slate.config.set("shell_theme", "phosphor")
    slate.config.set("markdown_theme", "phosphor-notes")
  end,
}
