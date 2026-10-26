#include "Modes/Slate/SlateSettingsMode.h"

#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Editor/Live/InlineLayout.h"
#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/Plugins/PluginManager.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"
#include "Core/Runtime/AppConfig.h"
#include "Platform/PlatformWindowService.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        using Category = SlateSettingsMode::Category;

        struct NamedIntOption
        {
            int value = 0;
            const char* label = "";
        };

        using Options = std::vector<NamedIntOption>;

        const Options& FontSizeOptions()
        {
            static const Options options = [] {
                Options values;
                for (int size = 10; size <= 24; ++size)
                {
                    static std::array<std::string, 15> labels;
                    labels[static_cast<std::size_t>(size - 10)] = std::to_string(size) + " px";
                    values.push_back({size, labels[static_cast<std::size_t>(size - 10)].c_str()});
                }
                return values;
            }();
            return options;
        }

        const Options LineSpacingOptions{{0, "tight"}, {2, "compact"}, {3, "balanced"}, {7, "relaxed"}, {11, "airy"}};
        const Options PageWidthOptions{{720, "narrow"}, {880, "standard"}, {980, "wide"}, {0, "fluid"}};
        const Options TabWidthOptions{{2, "2 spaces"}, {4, "4 spaces"}};
        const Options PanelMotionOptions{{0, "instant"}, {1, "quick"}, {2, "smooth"}, {3, "floaty"}};
        const Options ScrollMotionOptions{{0, "instant"}, {1, "smooth"}, {2, "glide"}};
        const Options ScrollbarStyleOptions{{0, "slim"}, {1, "soft"}, {2, "bold"}};
        const Options StartScreenOptions{{0, "home"}, {1, "last note"}, {2, "today's journal"}};
        const Options ScanlineOptions{{0, "off"}, {1, "subtle"}, {2, "strong"}};
        const Options InterfaceScaleOptions{{80, "80%"}, {90, "90%"}, {100, "100%"}, {110, "110%"}, {125, "125%"}, {150, "150%"}};
        const Options ImageStoragePolicyOptions{
            {static_cast<int>(Software::Slate::ImageStoragePolicy::WorkspaceMediaFolder), "workspace media"},
            {static_cast<int>(Software::Slate::ImageStoragePolicy::SameFolderAsNote), "same folder"},
            {static_cast<int>(Software::Slate::ImageStoragePolicy::SubfolderUnderNoteFolder), "note attachments"},
            {static_cast<int>(Software::Slate::ImageStoragePolicy::LinkOriginal), "link original"},
            {static_cast<int>(Software::Slate::ImageStoragePolicy::EmbedInMarkdown), "embed in note"},
        };

        // Steps through a list; enter (0) moves forward like right does
        template <typename Value>
        Value Cycle(const std::vector<Value>& values, const Value& current, int delta)
        {
            if (values.empty())
            {
                return current;
            }
            const int step = delta == 0 ? 1 : delta;
            const auto it = std::find(values.begin(), values.end(), current);
            const int index = it == values.end() ? 0 : static_cast<int>(it - values.begin());
            const int count = static_cast<int>(values.size());
            return values[static_cast<std::size_t>(((index + step) % count + count) % count)];
        }

        int CycleOption(const Options& options, int current, int delta)
        {
            std::vector<int> values;
            for (const auto& option : options)
            {
                values.push_back(option.value);
            }
            return Cycle(values, current, delta);
        }

        std::string OptionLabel(const Options& options, int value)
        {
            for (const auto& option : options)
            {
                if (option.value == value)
                {
                    return option.label;
                }
            }
            return std::to_string(value);
        }

        const char* CategoryLabel(Category category)
        {
            switch (category)
            {
            case Category::General: return "general";
            case Category::Appearance: return "appearance";
            case Category::Editor: return "editor";
            case Category::Keys: return "keys";
            case Category::Plugins: return "plugins";
            default: return "";
            }
        }

        const char* CategoryHint(Category category)
        {
            switch (category)
            {
            case Category::General: return "startup, status bar, workspace";
            case Category::Appearance: return "theme, type, terminal feel";
            case Category::Editor: return "writing, typing, motion, images";
            case Category::Keys: return "every shortcut is rebindable";
            case Category::Plugins: return "turn plugins on and off, tune them";
            default: return "";
            }
        }

        ImVec4 CategoryColor(Category category)
        {
            switch (category)
            {
            case Category::General: return Cyan;
            case Category::Appearance: return MarkdownImage;
            case Category::Editor: return Green;
            case Category::Keys: return Amber;
            case Category::Plugins: return MarkdownLink;
            default: return Muted;
            }
        }

        const char* SourceLabel(Software::Slate::PluginSource source)
        {
            switch (source)
            {
            case Software::Slate::PluginSource::Bundled: return "bundled";
            case Software::Slate::PluginSource::Workspace: return "workspace";
            case Software::Slate::PluginSource::UserInit: return "init.lua";
            default: return "user";
            }
        }

        std::string PluginState(const Software::Slate::PluginInfo& plugin)
        {
            if (!plugin.enabled)
            {
                return "off";
            }
            return plugin.status == Software::Slate::PluginStatus::Failed ? "error" : "on";
        }

        // Formats a number setting without trailing zeros
        std::string FormatNumber(double value, double step)
        {
            if (std::fabs(step - std::round(step)) < 1e-9 && std::fabs(value - std::round(value)) < 1e-9)
            {
                return std::to_string(static_cast<long long>(std::llround(value)));
            }
            char text[32] = {};
            std::snprintf(text, sizeof(text), "%.2f", value);
            return text;
        }

        const std::vector<std::string>& InstalledFonts()
        {
            // Probing the font folders touches the disk, so do it once
            static const std::vector<std::string> fonts = Software::Platform::PlatformWindowService::AvailableFontFamilies();
            return fonts;
        }
    }

    const char* SlateSettingsMode::ModeName() const
    {
        return Software::Slate::ModeIds::Settings;
    }

    std::string SlateSettingsMode::StatusBadge(const Software::Core::Runtime::AppContext&) const
    {
        return "CONFIG";
    }

    void SlateSettingsMode::SetCategory(Category category)
    {
        m_category = category;
        m_pluginPage.clear();
        m_selected = 0;
        m_scrollToSelection = true;
    }

    // ------------------------------------------------------------------ rows

    std::vector<SlateSettingsMode::Row> SlateSettingsMode::BuildRows(Software::Core::Runtime::AppContext& context)
    {
        std::vector<Row> rows;
        if (!m_pluginPage.empty())
        {
            BuildPluginPageRows(context, rows);
            return rows;
        }
        switch (m_category)
        {
        case Category::General: BuildGeneralRows(context, rows); break;
        case Category::Appearance: BuildAppearanceRows(context, rows); break;
        case Category::Editor: BuildEditorRows(context, rows); break;
        case Category::Keys: BuildKeyRows(context, rows); break;
        case Category::Plugins: BuildPluginRows(context, rows); break;
        default: break;
        }
        return rows;
    }

    void SlateSettingsMode::BuildGeneralRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        auto& workspace = WorkspaceContext(context);
        const auto editor = workspace.CurrentEditorSettings();
        auto edit = [this, &context](std::function<void(Software::Slate::EditorSettings&)> mutate) {
            auto& target = WorkspaceContext(context);
            auto settings = target.CurrentEditorSettings();
            mutate(settings);
            target.SetEditorSettings(settings);
            std::string error;
            if (!target.SaveEditorSettings(&error))
            {
                SetError(error);
            }
        };
        const ImVec4 accent = Cyan;

        rows.push_back({Row::Kind::Header, "startup"});
        rows.push_back({Row::Kind::Choice, "start on", OptionLabel(StartScreenOptions, editor.startScreen),
                        "what opens when slate starts", false, accent, [edit](int delta) {
                            edit([delta](auto& s) { s.startScreen = CycleOption(StartScreenOptions, s.startScreen, delta); });
                        }});
        rows.push_back({Row::Kind::Toggle, "boot sequence", "", "the logo and boot log when slate starts", editor.bootSequence, accent,
                        [edit](int) { edit([](auto& s) { s.bootSequence = !s.bootSequence; }); }});

        rows.push_back({Row::Kind::Header, "status bar"});
        rows.push_back({Row::Kind::Toggle, "clock", "", "the time on the right of the status bar", editor.statusClock, accent,
                        [edit](int) { edit([](auto& s) { s.statusClock = !s.statusClock; }); }});

        rows.push_back({Row::Kind::Header, "workspace"});
        std::string name = workspace.Workspace().Root().filename().u8string();
        if (const auto* vault = workspace.WorkspaceRegistry().ActiveVault())
        {
            name = vault->emoji + " " + vault->title;
        }
        rows.push_back({Row::Kind::Action, "switch workspace", name, "open another folder of notes", false, accent,
                        [this, &context](int) { ShowWorkspaceSwitcher(context); }});
        rows.push_back({Row::Kind::Action, "edit init.lua", "", "your own script; it runs like a plugin", false, accent,
                        [this, &context](int) { HandleCommand("init", context); }});

        rows.push_back({Row::Kind::Header, "reset"});
        rows.push_back({Row::Kind::Action, "reset everything", "", "settings and shortcuts back to defaults", false, Red,
                        [this](int) {
                            BeginConfirm("reset every setting and shortcut?", true,
                                         [this](bool accepted, Software::Core::Runtime::AppContext& confirmContext) {
                                             if (!accepted)
                                             {
                                                 return;
                                             }
                                             auto& target = WorkspaceContext(confirmContext);
                                             target.SetThemeSettings(Software::Slate::ThemeService::DefaultSettings());
                                             target.SetEditorSettings(Software::Slate::EditorSettingsService::DefaultSettings());
                                             std::string error;
                                             if (!target.SaveThemeSettings(&error) || !target.SaveEditorSettings(&error) ||
                                                 !target.Shortcuts().Reset(&error))
                                             {
                                                 SetError(error);
                                                 return;
                                             }
                                             SetStatus("settings and shortcuts reset");
                                         },
                                         "reset");
                        }});
    }

    void SlateSettingsMode::BuildAppearanceRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        auto& workspace = WorkspaceContext(context);
        const auto theme = workspace.CurrentThemeSettings();
        const auto editor = workspace.CurrentEditorSettings();
        auto edit = [this, &context](std::function<void(Software::Slate::EditorSettings&)> mutate) {
            auto& target = WorkspaceContext(context);
            auto settings = target.CurrentEditorSettings();
            mutate(settings);
            target.SetEditorSettings(settings);
            std::string error;
            if (!target.SaveEditorSettings(&error))
            {
                SetError(error);
            }
        };
        auto editTheme = [this, &context](std::function<void(Software::Slate::ThemeSettings&, Software::Slate::SlateWorkspaceContext&)> mutate) {
            auto& target = WorkspaceContext(context);
            auto settings = target.CurrentThemeSettings();
            mutate(settings, target);
            target.SetThemeSettings(settings);
            std::string error;
            if (!target.SaveThemeSettings(&error))
            {
                SetError(error);
            }
        };
        const ImVec4 accent = MarkdownImage;

        rows.push_back({Row::Kind::Header, "theme"});
        rows.push_back({Row::Kind::Choice, "shell palette", workspace.Theme().ShellPresetLabel(theme.shellPreset),
                        "colors of the app itself; theme plugins add more", false, accent, [editTheme](int delta) {
                            editTheme([delta](auto& s, auto& target) {
                                s.shellPreset = Cycle(target.Theme().ShellPresetIds(), s.shellPreset, delta);
                            });
                        }});
        rows.push_back({Row::Kind::Choice, "markdown accents", workspace.Theme().MarkdownPresetLabel(theme.markdownPreset),
                        "colors of headings, links, and marks", false, accent, [editTheme](int delta) {
                            editTheme([delta](auto& s, auto& target) {
                                s.markdownPreset = Cycle(target.Theme().MarkdownPresetIds(), s.markdownPreset, delta);
                            });
                        }});

        rows.push_back({Row::Kind::Header, "type"});
        std::vector<std::string> fonts = InstalledFonts();
        if (std::find(fonts.begin(), fonts.end(), editor.fontFamily) == fonts.end())
        {
            fonts.insert(fonts.begin(), editor.fontFamily);
        }
        rows.push_back({Row::Kind::Choice, "font", editor.fontFamily, std::to_string(fonts.size()) + " monospace fonts found on this machine",
                        false, accent, [edit, fonts](int delta) {
                            edit([delta, &fonts](auto& s) { s.fontFamily = Cycle(fonts, s.fontFamily, delta); });
                        }});
        rows.push_back({Row::Kind::Choice, "interface size", OptionLabel(InterfaceScaleOptions, editor.interfaceScale),
                        "scales all text in the app", false, accent, [edit](int delta) {
                            edit([delta](auto& s) { s.interfaceScale = CycleOption(InterfaceScaleOptions, s.interfaceScale, delta); });
                        }});

        rows.push_back({Row::Kind::Header, "terminal"});
        rows.push_back({Row::Kind::Toggle, "cursor blink", "", "the caret and the logo cursor blink", editor.cursorBlink, accent,
                        [edit](int) { edit([](auto& s) { s.cursorBlink = !s.cursorBlink; }); }});
        rows.push_back({Row::Kind::Choice, "scanlines", OptionLabel(ScanlineOptions, editor.scanlines), "an old CRT glow over everything",
                        false, accent, [edit](int delta) {
                            edit([delta](auto& s) { s.scanlines = CycleOption(ScanlineOptions, s.scanlines, delta); });
                        }});
        rows.push_back({Row::Kind::Choice, "scrollbars", OptionLabel(ScrollbarStyleOptions, editor.scrollbarStyle), "", false, accent,
                        [edit](int delta) {
                            edit([delta](auto& s) { s.scrollbarStyle = CycleOption(ScrollbarStyleOptions, s.scrollbarStyle, delta); });
                        }});
        rows.push_back({Row::Kind::Toggle, "link underline", "", "underline links in notes", editor.linkUnderline, accent,
                        [edit](int) { edit([](auto& s) { s.linkUnderline = !s.linkUnderline; }); }});
    }

    void SlateSettingsMode::BuildEditorRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        const auto editor = WorkspaceContext(context).CurrentEditorSettings();
        auto edit = [this, &context](std::function<void(Software::Slate::EditorSettings&)> mutate) {
            auto& target = WorkspaceContext(context);
            auto settings = target.CurrentEditorSettings();
            mutate(settings);
            target.SetEditorSettings(settings);
            std::string error;
            if (!target.SaveEditorSettings(&error))
            {
                SetError(error);
            }
        };
        const ImVec4 accent = Green;
        auto toggle = [&](const char* label, bool value, const char* hint, bool Software::Slate::EditorSettings::*field) {
            rows.push_back({Row::Kind::Toggle, label, "", hint, value, accent, [edit, field](int) {
                                edit([field](auto& s) { s.*field = !(s.*field); });
                            }});
        };
        auto choice = [&](const char* label, const Options& options, int value, const char* hint, int Software::Slate::EditorSettings::*field) {
            rows.push_back({Row::Kind::Choice, label, OptionLabel(options, value), hint, false, accent, [edit, field, &options](int delta) {
                                edit([field, delta, &options](auto& s) { s.*field = CycleOption(options, s.*field, delta); });
                            }});
        };

        rows.push_back({Row::Kind::Header, "writing"});
        toggle("live preview", editor.livePreview, "render markdown in place; syntax shows on the line you edit",
               &Software::Slate::EditorSettings::livePreview);
        toggle("heading scale", editor.headingScale, "draw headings larger than body text", &Software::Slate::EditorSettings::headingScale);
        choice("font size", FontSizeOptions(), editor.fontSize, "", &Software::Slate::EditorSettings::fontSize);
        choice("line spacing", LineSpacingOptions, editor.lineSpacing, "", &Software::Slate::EditorSettings::lineSpacing);
        choice("page width", PageWidthOptions, editor.pageWidth, "width of the writing column", &Software::Slate::EditorSettings::pageWidth);

        rows.push_back({Row::Kind::Header, "typing"});
        toggle("smart lists", editor.autoListContinuation, "enter continues lists and tasks",
               &Software::Slate::EditorSettings::autoListContinuation);
        toggle("indent with tabs", editor.indentWithTabs, "tab inserts a tab character instead of spaces",
               &Software::Slate::EditorSettings::indentWithTabs);
        choice("tab width", TabWidthOptions, editor.tabWidth, "", &Software::Slate::EditorSettings::tabWidth);

        rows.push_back({Row::Kind::Header, "motion"});
        choice("scroll motion", ScrollMotionOptions, editor.scrollMotion, "how the view follows the caret",
               &Software::Slate::EditorSettings::scrollMotion);
        choice("panel motion", PanelMotionOptions, editor.panelMotion, "how side panels slide", &Software::Slate::EditorSettings::panelMotion);
        toggle("caret line", editor.highlightCurrentLine, "highlight the line you are on",
               &Software::Slate::EditorSettings::highlightCurrentLine);

        rows.push_back({Row::Kind::Header, "images"});
        toggle("paste images", editor.pasteClipboardImages, "ctrl+v saves clipboard images next to your notes",
               &Software::Slate::EditorSettings::pasteClipboardImages);
        choice("image storage", ImageStoragePolicyOptions, editor.imageStoragePolicy, "where pasted and dropped images go",
               &Software::Slate::EditorSettings::imageStoragePolicy);
    }

    void SlateSettingsMode::BuildKeyRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        auto& shortcuts = WorkspaceContext(context).Shortcuts();
        std::string group;
        for (const auto& definition : shortcuts.Definitions())
        {
            if (group != definition.group)
            {
                group = definition.group;
                rows.push_back({Row::Kind::Header, group});
            }
            const auto action = definition.action;
            const std::string label = std::string(definition.group) + ": " + definition.label;
            rows.push_back({Row::Kind::Keycap, definition.label, shortcuts.Label(action), "", false, Amber, [this, action, label](int delta) {
                                if (delta != 0)
                                {
                                    return;
                                }
                                m_shortcutCaptureOpen = true;
                                m_shortcutCaptureArmed = false;
                                m_shortcutCaptureAction = action;
                                m_shortcutCaptureLabel = label;
                            }});
        }
        rows.push_back({Row::Kind::Header, "manage"});
        rows.push_back({Row::Kind::Action, "reset shortcuts", "", "every key back to its default", false, Red, [this, &context](int) {
                            std::string error;
                            if (WorkspaceContext(context).Shortcuts().Reset(&error))
                            {
                                SetStatus("shortcuts reset");
                            }
                            else
                            {
                                SetError(error);
                            }
                        }});
    }

    void SlateSettingsMode::BuildPluginRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        const auto manager = context.services.Resolve<Software::Slate::PluginManager>();
        const auto plugins = manager ? manager->Plugins() : std::vector<Software::Slate::PluginInfo>{};
        rows.push_back({Row::Kind::Header, "installed"});
        if (plugins.empty())
        {
            rows.push_back({Row::Kind::Info, "no plugins found"});
        }
        for (const auto& plugin : plugins)
        {
            const std::string id = plugin.manifest.id;
            const bool enabled = plugin.enabled;
            std::string hint = plugin.manifest.description;
            if (!plugin.manifest.settings.empty())
            {
                hint += (hint.empty() ? "" : "  \xc2\xb7  ") + std::to_string(plugin.manifest.settings.size()) + " settings";
            }
            Row row{Row::Kind::Plugin, plugin.manifest.name, PluginState(plugin), hint, enabled, MarkdownLink,
                    [this, manager, id, enabled](int delta) {
                        if (delta == 0)
                        {
                            m_pluginPage = id;
                            m_selected = 0;
                            m_scrollToSelection = true;
                            return;
                        }
                        std::string error;
                        if (manager->SetEnabled(id, !enabled, &error))
                        {
                            SetStatus(std::string(enabled ? "disabled " : "enabled ") + id);
                        }
                        else
                        {
                            SetError(error);
                        }
                    }};
            rows.push_back(std::move(row));
        }

        rows.push_back({Row::Kind::Header, "manage"});
        rows.push_back({Row::Kind::Action, "new plugin", "", "scaffold a plugin in your plugin folder", false, MarkdownLink, [this](int) {
                            BeginPrompt("plugin name", "", [this](const std::string& name, Software::Core::Runtime::AppContext& promptContext) {
                                if (!name.empty())
                                {
                                    HandleCommand("plugin new " + name, promptContext);
                                }
                            });
                        }});
        rows.push_back({Row::Kind::Action, "open plugin folder", "", Software::Slate::PluginManager::UserPluginDirectory().generic_u8string(),
                        false, MarkdownLink, [this, &context](int) { HandleCommand("plugin folder", context); }});
        rows.push_back({Row::Kind::Action, "reload all", "", "rediscover plugin folders and reload every plugin", false, MarkdownLink,
                        [this, &context](int) { HandleCommand("reload", context); }});
    }

    void SlateSettingsMode::BuildPluginPageRows(Software::Core::Runtime::AppContext& context, std::vector<Row>& rows)
    {
        const auto manager = context.services.Resolve<Software::Slate::PluginManager>();
        const Software::Slate::PluginInfo* plugin = nullptr;
        const auto plugins = manager ? manager->Plugins() : std::vector<Software::Slate::PluginInfo>{};
        for (const auto& candidate : plugins)
        {
            if (candidate.manifest.id == m_pluginPage)
            {
                plugin = &candidate;
            }
        }
        if (!plugin)
        {
            m_pluginPage.clear();
            BuildPluginRows(context, rows);
            return;
        }

        const std::string id = plugin->manifest.id;
        const bool enabled = plugin->enabled;
        const ImVec4 accent = MarkdownLink;
        rows.push_back({Row::Kind::Header, plugin->manifest.name});
        rows.push_back({Row::Kind::Toggle, "enabled", "", plugin->manifest.description, enabled, accent, [this, manager, id, enabled](int) {
                            std::string error;
                            if (manager->SetEnabled(id, !enabled, &error))
                            {
                                SetStatus(std::string(enabled ? "disabled " : "enabled ") + id);
                            }
                            else
                            {
                                SetError(error);
                            }
                        }});

        rows.push_back({Row::Kind::Header, "settings"});
        if (plugin->manifest.settings.empty())
        {
            rows.push_back({Row::Kind::Info, "this plugin has no settings"});
        }
        for (const auto& spec : plugin->manifest.settings)
        {
            const std::string stored = manager->SettingValue(id, spec.key);
            const std::string key = spec.key;
            auto store = [this, manager, id, key](const std::string& value) {
                std::string error;
                if (!manager->SetSetting(id, key, value, &error))
                {
                    SetError(error);
                }
            };
            switch (spec.type)
            {
            case Software::Slate::PluginSettingType::Bool:
            {
                const bool value = Software::Slate::PluginManager::StoredAsBool(stored);
                rows.push_back({Row::Kind::Toggle, spec.label, "", spec.description, value, accent,
                                [store, value](int) { store(Software::Slate::PluginManager::StoredBool(!value)); }});
                break;
            }
            case Software::Slate::PluginSettingType::Choice:
            {
                const std::string value = Software::Slate::PluginManager::StoredAsText(stored);
                const auto options = spec.options;
                rows.push_back({Row::Kind::Choice, spec.label, value, spec.description, false, accent, [store, value, options](int delta) {
                                    store(Software::Slate::PluginManager::StoredText(Cycle(options, value, delta)));
                                }});
                break;
            }
            case Software::Slate::PluginSettingType::Number:
            {
                const double value = Software::Slate::PluginManager::StoredAsNumber(stored);
                const double min = spec.min;
                const double max = spec.max;
                const double step = spec.step;
                rows.push_back({Row::Kind::Choice, spec.label, FormatNumber(value, step), spec.description, false, accent,
                                [store, value, min, max, step](int delta) {
                                    double next = value + step * (delta == 0 ? 1 : delta);
                                    // Enter past the top wraps back to the minimum
                                    if (delta == 0 && next > max + 1e-9)
                                    {
                                        next = min;
                                    }
                                    store(Software::Slate::PluginManager::StoredNumber(std::clamp(next, min, max)));
                                }});
                break;
            }
            case Software::Slate::PluginSettingType::Text:
            {
                const std::string value = Software::Slate::PluginManager::StoredAsText(stored);
                const std::string label = spec.label;
                rows.push_back({Row::Kind::Action, spec.label, value.empty() ? std::string("(empty)") : value, spec.description, false, accent,
                                [this, store, value, label](int delta) {
                                    if (delta != 0)
                                    {
                                        return;
                                    }
                                    BeginPrompt(label, value, [store](const std::string& text, Software::Core::Runtime::AppContext&) {
                                        store(Software::Slate::PluginManager::StoredText(text));
                                    });
                                }});
                break;
            }
            }
        }

        rows.push_back({Row::Kind::Header, "actions"});
        rows.push_back({Row::Kind::Action, "reload", "", "read the plugin from disk again", false, accent, [this, manager, id](int) {
                            std::string error;
                            if (manager->Reload(id, &error))
                            {
                                SetStatus("reloaded " + id);
                            }
                            else
                            {
                                SetError(error);
                            }
                        }});
        if (!plugin->manifest.settings.empty())
        {
            rows.push_back({Row::Kind::Action, "reset settings", "", "this plugin's settings back to defaults", false, Red,
                            [this, manager, id](int) {
                                std::string error;
                                if (manager->ResetSettings(id, &error))
                                {
                                    SetStatus(id + " settings reset");
                                }
                                else
                                {
                                    SetError(error);
                                }
                            }});
        }
        rows.push_back({Row::Kind::Action, "back to plugins", "", "", false, Muted, [this](int) {
                            const std::string id = m_pluginPage;
                            m_pluginPage.clear();
                            m_selected = 0;
                            m_hoveredPlugin = id;
                        }});
    }

    // ------------------------------------------------------------------ frame

    void SlateSettingsMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& shortcuts = workspace.Shortcuts();
        auto& ui = UiState(context);
        if (!workspace.HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }
        if (!ui.settingsSection.empty())
        {
            const std::string section = ui.settingsSection;
            ui.settingsSection.clear();
            for (int i = 0; i < static_cast<int>(Category::Count); ++i)
            {
                if (section == CategoryLabel(static_cast<Category>(i)))
                {
                    SetCategory(static_cast<Category>(i));
                }
            }
            if (section.rfind("plugin ", 0) == 0)
            {
                SetCategory(Category::Plugins);
                m_pluginPage = section.substr(7);
            }
        }

        std::vector<Row> rows = BuildRows(context);
        std::vector<std::size_t> selectable;
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].kind != Row::Kind::Header && rows[i].kind != Row::Kind::Info)
            {
                selectable.push_back(i);
            }
        }
        // Coming back from a plugin page selects that plugin again
        if (!m_hoveredPlugin.empty() && m_pluginPage.empty() && m_category == Category::Plugins)
        {
            const auto manager = context.services.Resolve<Software::Slate::PluginManager>();
            const auto plugins = manager ? manager->Plugins() : std::vector<Software::Slate::PluginInfo>{};
            for (std::size_t i = 0; i < plugins.size() && i < selectable.size(); ++i)
            {
                if (plugins[i].manifest.id == m_hoveredPlugin)
                {
                    m_selected = i;
                    m_scrollToSelection = true;
                }
            }
            m_hoveredPlugin.clear();
        }
        m_selectableCount = selectable.size();
        m_selected = selectable.empty() ? 0 : std::min(m_selected, selectable.size() - 1);

        if (handleInput && !m_shortcutCaptureOpen)
        {
            const ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
            {
                const int count = static_cast<int>(Category::Count);
                const int next = (static_cast<int>(m_category) + (io.KeyShift ? count - 1 : 1)) % count;
                SetCategory(static_cast<Category>(next));
                return;
            }
            if (!io.KeyCtrl && !io.KeyAlt)
            {
                for (int i = 0; i < static_cast<int>(Category::Count); ++i)
                {
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i), false))
                    {
                        SetCategory(static_cast<Category>(i));
                        return;
                    }
                }
            }
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateDown, true) && m_selected + 1 < selectable.size())
            {
                ++m_selected;
                m_scrollToSelection = true;
            }
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateUp, true) && m_selected > 0)
            {
                --m_selected;
                m_scrollToSelection = true;
            }

            Row* current = selectable.empty() ? nullptr : &rows[selectable[m_selected]];
            if (current && current->change)
            {
                const bool adjustable = current->kind == Row::Kind::Toggle || current->kind == Row::Kind::Choice ||
                                        current->kind == Row::Kind::Plugin;
                if (adjustable && shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateLeft, true))
                {
                    current->change(-1);
                }
                else if (adjustable && shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateRight, true))
                {
                    current->change(current->kind == Row::Kind::Plugin ? 0 : 1);
                }
                else if (shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
                {
                    current->change(0);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Space, false) &&
                         (current->kind == Row::Kind::Toggle || current->kind == Row::Kind::Plugin))
                {
                    current->change(1);
                }
            }

            if (shortcuts.Pressed(Software::Slate::ShortcutAction::SettingsReset) && !m_pluginPage.empty())
            {
                const auto manager = context.services.Resolve<Software::Slate::PluginManager>();
                std::string error;
                if (manager && manager->ResetSettings(m_pluginPage, &error))
                {
                    SetStatus(m_pluginPage + " settings reset");
                }
            }

            if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
            {
                if (!m_pluginPage.empty())
                {
                    m_hoveredPlugin = m_pluginPage;
                    m_pluginPage.clear();
                    m_selected = 0;
                    return;
                }
                ShowHome(context);
                return;
            }
            // A change may have rebuilt plugin state; draw fresh rows next frame
            rows = BuildRows(context);
        }

        // Header: where you are, and the version on the right
        const std::string version = std::string("slate ") + SOFTWARE_APP_VERSION;
        std::string subtitle = CategoryHint(m_category);
        if (!m_pluginPage.empty())
        {
            subtitle = "plugins / " + m_pluginPage;
        }
        ScreenHeader("settings", subtitle.c_str(), version.c_str(), CategoryColor(m_category));

        const float contentHeight = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - Tokens::FooterHeight - 4.0f);
        const float available = ImGui::GetContentRegionAvail().x;
        const float navWidth = 190.0f * ImGui::GetStyle().FontScaleMain;
        const float gap = Tokens::SpaceXL;
        const bool showCard = available > 1060.0f;
        const float cardWidth = showCard ? std::clamp(available * 0.32f, 340.0f, 440.0f) : 0.0f;
        const float rowsWidth = std::max(300.0f, available - navWidth - gap - (showCard ? cardWidth + gap : 0.0f));

        DrawCategories(navWidth, contentHeight);
        ImGui::SameLine(0.0f, gap);
        DrawRows(rows, rowsWidth, contentHeight);
        if (showCard)
        {
            ImGui::SameLine(0.0f, gap);
            DrawContextCard(context, rows, cardWidth, contentHeight);
        }
        DrawShortcutCapture(context);
    }

    void SlateSettingsMode::DrawCategories(float width, float height)
    {
        ImGui::BeginChild("SettingsCategories", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
        auto* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight + 14.0f;
        for (int i = 0; i < static_cast<int>(Category::Count); ++i)
        {
            const auto category = static_cast<Category>(i);
            const bool selected = category == m_category;
            const ImVec4 color = CategoryColor(category);
            ImVec2 rowMin;
            ImVec2 rowMax;
            ImGui::PushID(i);
            if (ListRow("##category", selected, rowHeight, color, &rowMin, &rowMax))
            {
                SetCategory(category);
            }
            ImGui::PopID();
            const std::string key = std::to_string(i + 1);
            DrawKeycap(drawList, ImVec2(rowMin.x + 12.0f, rowMin.y + (rowHeight - KeycapHeight()) * 0.5f), key, selected ? color : Muted);
            drawList->AddText(ImVec2(rowMin.x + 22.0f + KeycapWidth(key), rowMin.y + (rowHeight - lineHeight) * 0.5f),
                              U32(selected ? Primary : TextSecondary()), CategoryLabel(category));
            if (selected && !m_pluginPage.empty())
            {
                drawList->AddText(ImVec2(rowMax.x - 18.0f, rowMin.y + (rowHeight - lineHeight) * 0.5f), U32(color), ">");
            }
        }
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceL));
        KeyHint("tab", "next section", Muted);
        ImGui::EndChild();
    }

    void SlateSettingsMode::DrawRows(std::vector<Row>& rows, float width, float height)
    {
        ImGui::BeginChild("SettingsRows", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
        auto* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeight();
        std::size_t selectableIndex = 0;
        bool firstHeader = true;
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            Row& row = rows[i];
            if (row.kind == Row::Kind::Header)
            {
                ImGui::Dummy(ImVec2(1.0f, firstHeader ? 0.0f : Tokens::SpaceM));
                firstHeader = false;
                SectionLabel(row.label.c_str(), m_pluginPage.empty() ? CategoryColor(m_category) : MarkdownLink);
                ImGui::Dummy(ImVec2(1.0f, 2.0f));
                continue;
            }
            if (row.kind == Row::Kind::Info)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
                ImGui::TextColored(TextFaint(), "%s", row.label.c_str());
                continue;
            }

            const std::size_t index = selectableIndex++;
            const bool selected = index == m_selected;
            const bool showHint = selected && !row.hint.empty();
            const float rowHeight = lineHeight + 12.0f + (showHint ? lineHeight + 2.0f : 0.0f);
            ImVec2 rowMin;
            ImVec2 rowMax;
            ImGui::PushID(static_cast<int>(i));
            const bool clicked = ListRow("##row", selected, rowHeight, row.accent, &rowMin, &rowMax);
            const bool doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            ImGui::PopID();
            if (selected && m_scrollToSelection)
            {
                const float top = ImGui::GetWindowPos().y;
                const float bottom = top + ImGui::GetWindowHeight();
                if (rowMin.y < top + 8.0f || rowMax.y > bottom - 8.0f)
                {
                    ImGui::SetScrollHereY(rowMin.y < top + 8.0f ? 0.2f : 0.8f);
                }
                m_scrollToSelection = false;
            }

            const float textY = rowMin.y + 6.0f;
            drawList->AddText(ImVec2(rowMin.x + 14.0f, textY), U32(selected ? Primary : TextSecondary()), row.label.c_str());
            if (showHint)
            {
                const std::string hint = Ellipsize(row.hint, rowMax.x - rowMin.x - 28.0f);
                drawList->AddText(ImVec2(rowMin.x + 14.0f, textY + lineHeight + 2.0f), U32(TextFaint()), hint.c_str());
            }

            // Value on the right, drawn by kind
            const float right = rowMax.x - 12.0f;
            const float capY = rowMin.y + (lineHeight + 12.0f - KeycapHeight()) * 0.5f;
            switch (row.kind)
            {
            case Row::Kind::Toggle:
            {
                const char* text = row.on ? "on" : "off";
                DrawPill(drawList, ImVec2(right - PillWidth(text), capY), text, row.on ? Green : Muted, row.on);
                break;
            }
            case Row::Kind::Choice:
            {
                const float valueWidth = ImGui::CalcTextSize(row.value.c_str()).x;
                float x = right - valueWidth;
                if (selected)
                {
                    // Arrows say left and right change this value
                    const float arrowWidth = ImGui::CalcTextSize("> ").x;
                    drawList->AddText(ImVec2(right - arrowWidth + 6.0f, textY), U32(row.accent), ">");
                    x -= arrowWidth;
                    drawList->AddText(ImVec2(x - arrowWidth, textY), U32(row.accent), "<");
                }
                drawList->AddText(ImVec2(x, textY), U32(selected ? Cyan : Alpha(Cyan, 0.8f)), row.value.c_str());
                break;
            }
            case Row::Kind::Keycap:
                DrawKeycap(drawList, ImVec2(right - KeycapWidth(row.value), capY), row.value, selected ? Green : Amber);
                break;
            case Row::Kind::Action:
            {
                float x = right;
                if (selected)
                {
                    x -= KeycapWidth("enter");
                    DrawKeycap(drawList, ImVec2(x, capY), "enter", row.accent);
                    x -= 10.0f;
                }
                if (!row.value.empty())
                {
                    const float valueWidth = ImGui::CalcTextSize(row.value.c_str()).x;
                    drawList->PushClipRect(ImVec2(rowMin.x + 160.0f, rowMin.y), ImVec2(x, rowMax.y), true);
                    drawList->AddText(ImVec2(x - valueWidth, textY), U32(TextSecondary()), row.value.c_str());
                    drawList->PopClipRect();
                }
                break;
            }
            case Row::Kind::Plugin:
            {
                const ImVec4 color = row.value == "error" ? Red : (row.on ? Green : Muted);
                const float pillX = right - PillWidth(row.value);
                DrawPill(drawList, ImVec2(pillX, capY), row.value, color, row.on && row.value != "error");
                if (selected)
                {
                    const float arrowX = pillX - ImGui::CalcTextSize("  >").x;
                    drawList->AddText(ImVec2(arrowX, textY), U32(TextFaint()), ">");
                }
                break;
            }
            default:
                break;
            }

            if (clicked && !m_shortcutCaptureOpen)
            {
                const bool wasSelected = selected;
                m_selected = index;
                if (row.change)
                {
                    if (row.kind == Row::Kind::Toggle || row.kind == Row::Kind::Choice)
                    {
                        row.change(1);
                    }
                    else if (wasSelected && row.kind != Row::Kind::Plugin)
                    {
                        row.change(0);
                    }
                }
            }
            if (doubleClicked && row.kind == Row::Kind::Plugin && row.change)
            {
                row.change(0);
            }
            ImGui::Dummy(ImVec2(1.0f, 1.0f));
        }
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXL));
        ImGui::EndChild();
    }

    void SlateSettingsMode::DrawContextCard(Software::Core::Runtime::AppContext& context, const std::vector<Row>& rows, float width,
                                            float height)
    {
        auto& workspace = WorkspaceContext(context);
        const auto manager = context.services.Resolve<Software::Slate::PluginManager>();

        // Which plugin the card describes: the open page, or the selected row in the plugin list
        std::string pluginId = m_pluginPage;
        if (pluginId.empty() && m_category == Category::Plugins && manager)
        {
            const auto plugins = manager->Plugins();
            std::size_t selectableIndex = 0;
            for (const auto& row : rows)
            {
                if (row.kind == Row::Kind::Header || row.kind == Row::Kind::Info)
                {
                    continue;
                }
                if (selectableIndex == m_selected && row.kind == Row::Kind::Plugin && selectableIndex < plugins.size())
                {
                    pluginId = plugins[selectableIndex].manifest.id;
                }
                ++selectableIndex;
            }
        }

        if (!pluginId.empty() && manager)
        {
            for (const auto& plugin : manager->Plugins())
            {
                if (plugin.manifest.id != pluginId)
                {
                    continue;
                }
                const std::string state = PluginState(plugin);
                BeginCard("SettingsContext", ImVec2(width, height), MarkdownLink);
                CardTitle("plugin", MarkdownLink, plugin.manifest.id.c_str());
                ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.35f);
                ImGui::TextColored(Primary, "%s", plugin.manifest.name.c_str());
                ImGui::PopFont();
                Pill(state, state == "error" ? Red : (plugin.enabled ? Green : Muted), plugin.enabled && state != "error");
                ImGui::SameLine(0.0f, 8.0f);
                Pill(SourceLabel(plugin.source), Cyan);
                ImGui::SameLine(0.0f, 8.0f);
                Pill("v" + plugin.manifest.version, Muted);
                ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                ImGui::PushTextWrapPos(0.0f);
                if (!plugin.manifest.description.empty())
                {
                    ImGui::TextColored(TextSecondary(), "%s", plugin.manifest.description.c_str());
                }
                if (!plugin.manifest.author.empty())
                {
                    ImGui::TextColored(TextFaint(), "by %s", plugin.manifest.author.c_str());
                }
                if (!plugin.error.empty())
                {
                    ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                    SectionLabel("error", Red);
                    ImGui::TextColored(Red, "%s", plugin.error.c_str());
                }
                if (!plugin.commands.empty())
                {
                    ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                    SectionLabel("commands", Amber);
                    std::string list;
                    for (const auto& command : plugin.commands)
                    {
                        list += (list.empty() ? "" : "  ") + command;
                    }
                    ImGui::TextColored(TextSecondary(), "%s", list.c_str());
                }
                if (!plugin.log.empty())
                {
                    ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                    SectionLabel("log", Muted);
                    const std::size_t first = plugin.log.size() > 6 ? plugin.log.size() - 6 : 0;
                    for (std::size_t i = first; i < plugin.log.size(); ++i)
                    {
                        ImGui::TextColored(TextFaint(), "> %s", plugin.log[i].c_str());
                    }
                }
                ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                SectionLabel("folder", Muted);
                ImGui::TextColored(TextFaint(), "%s", FriendlyPath(plugin.directory.generic_u8string()).c_str());
                ImGui::PopTextWrapPos();
                if (m_pluginPage.empty())
                {
                    ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceM));
                    KeyHint("enter", "open settings", MarkdownLink);
                    KeyHint("space", "turn on or off", MarkdownLink);
                }
                EndCard();
                return;
            }
        }

        if (m_category == Category::Appearance || m_category == Category::Editor)
        {
            // The sample renders through the live editor's own layout, so it matches the editor exactly
            const auto editorSettings = workspace.CurrentEditorSettings();
            BeginCard("SettingsContext", ImVec2(width, height), MarkdownImage);
            CardTitle("preview", MarkdownImage, "live");
            static constexpr const char* SampleLines[] = {
                "# Encounter Notes",
                "## Combat Loop",
                "- [ ] tune parry window",
                "- [x] player dodge feels right",
                "> pace matters more than particles",
                "A [link](Docs/Combat.md), **bold**, *italic*, ==marked== and `code`. #design",
            };
            auto* previewDrawList = ImGui::GetWindowDrawList();
            for (const char* sample : SampleLines)
            {
                Software::Slate::Live::InlineOptions options;
                options.fontSize = static_cast<float>(editorSettings.fontSize);
                options.lineGap = static_cast<float>(editorSettings.lineSpacing);
                options.headingScale = editorSettings.headingScale;
                options.linkUnderline = editorSettings.linkUnderline;
                options.reveal = !editorSettings.livePreview;
                options.wrapWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x);
                const auto layout = Software::Slate::Live::LayoutInline(sample, options);
                Software::Slate::Live::DrawInline(previewDrawList, layout, sample, ImGui::GetCursorScreenPos(), options);
                ImGui::Dummy(ImVec2(options.wrapWidth, layout.height));
            }
            EndCard();
            return;
        }

        // About card for general and keys
        BeginCard("SettingsContext", ImVec2(width, height), Cyan);
        CardTitle("about", Cyan, SOFTWARE_APP_VERSION);
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
        BlockLogo(std::clamp(width / 40.0f, 5.0f, 9.0f), ImGui::GetTime());
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceM));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(TextSecondary(), "plain-text notes, todos and a journal. everything is a markdown file you own.");
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
        SectionLabel("config", Muted);
        ImGui::TextColored(TextFaint(), "%s", FriendlyPath(Software::Slate::AppPaths::ConfigDirectory().generic_u8string()).c_str());
        SectionLabel("plugins", Muted);
        ImGui::TextColored(TextFaint(), "%s", FriendlyPath(Software::Slate::PluginManager::UserPluginDirectory().generic_u8string()).c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceM));
        KeyHint("1-5", "jump to a section", Cyan);
        KeyHint("enter", "change or open", Cyan);
        EndCard();
    }

    void SlateSettingsMode::DrawShortcutCapture(Software::Core::Runtime::AppContext& context)
    {
        auto& shortcuts = WorkspaceContext(context).Shortcuts();
        if (m_shortcutCaptureOpen)
        {
            ImGui::OpenPopup("Bind shortcut");
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Tokens::SpaceXL, Tokens::SpaceL));
        if (ImGui::BeginPopupModal("Bind shortcut", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar))
        {
            SectionLabel("bind shortcut", Amber, false);
            ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
            ImGui::TextColored(Primary, "%s", m_shortcutCaptureLabel.c_str());
            ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
            ImGui::TextColored(TextSecondary(), "now");
            ImGui::SameLine();
            Software::Slate::UI::KeyHint(shortcuts.Label(m_shortcutCaptureAction), "", Amber);
            ImGui::TextColored(TextSecondary(), "press the new keys");
            ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
            KeyHint("esc", "cancel", Muted);

            auto close = [this]() {
                m_shortcutCaptureOpen = false;
                m_shortcutCaptureArmed = false;
                m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
                m_shortcutCaptureLabel.clear();
                ImGui::CloseCurrentPopup();
            };
            if (!m_shortcutCaptureArmed)
            {
                m_shortcutCaptureArmed = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                close();
            }
            else
            {
                Software::Slate::KeyBinding captured;
                if (Software::Slate::ShortcutService::CapturePressedBinding(&captured))
                {
                    std::string error;
                    if (shortcuts.SetBinding(m_shortcutCaptureAction, captured, &error))
                    {
                        SetStatus("shortcut updated: " + Software::Slate::ShortcutService::SerializeBinding(captured));
                    }
                    else
                    {
                        SetError(error);
                    }
                    close();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    std::string SlateSettingsMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        const std::string choose = "(" + shortcuts.Label(Software::Slate::ShortcutAction::NavigateUp) + "/" +
                                   shortcuts.Label(Software::Slate::ShortcutAction::NavigateDown) + ") choose   (" +
                                   shortcuts.Label(Software::Slate::ShortcutAction::NavigateLeft) + "/" +
                                   shortcuts.Label(Software::Slate::ShortcutAction::NavigateRight) + ") change   ";
        if (!m_pluginPage.empty())
        {
            return choose + shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "change") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::SettingsReset, "reset plugin") + "   " +
                   shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "back");
        }
        return choose + shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "open") + "   (tab) section   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }
}
