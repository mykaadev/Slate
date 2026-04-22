#include "Modes/Slate/SlateSettingsMode.h"

#include "App/Slate/EditorSettingsService.h"
#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        enum class SettingsRow
        {
            ShellPreset = 0,
            MarkdownPreset,
            FontSize,
            LineSpacing,
            PageWidth,
            WordWrap,
            CaretLine,
            TabWidth,
            IndentMode,
            AutoListContinuation,
            PasteClipboardImages,
            Count
        };

        struct NamedIntOption
        {
            int value = 0;
            const char* label = "";
        };

        const std::array<NamedIntOption, 5>& FontSizeOptions()
        {
            static constexpr std::array<NamedIntOption, 5> options{{
                {13, "13"},
                {14, "14"},
                {15, "15"},
                {17, "17"},
                {19, "19"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 4>& LineSpacingOptions()
        {
            static constexpr std::array<NamedIntOption, 4> options{{
                {1, "tight"},
                {2, "compact"},
                {3, "balanced"},
                {5, "relaxed"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 4>& PageWidthOptions()
        {
            static constexpr std::array<NamedIntOption, 4> options{{
                {720, "narrow"},
                {880, "standard"},
                {980, "wide"},
                {0, "fluid"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 2>& TabWidthOptions()
        {
            static constexpr std::array<NamedIntOption, 2> options{{
                {2, "2 spaces"},
                {4, "4 spaces"},
            }};
            return options;
        }

        template <typename ValueType>
        ValueType Cycle(const std::vector<ValueType>& values, const ValueType& current, int delta)
        {
            if (values.empty())
            {
                return current;
            }

            auto it = std::find(values.begin(), values.end(), current);
            std::size_t index = it == values.end() ? 0 : static_cast<std::size_t>(it - values.begin());
            const int next = static_cast<int>(index) + delta;
            index = static_cast<std::size_t>((next % static_cast<int>(values.size()) + static_cast<int>(values.size())) %
                                             static_cast<int>(values.size()));
            return values[index];
        }

        template <typename OptionArray>
        int CycleNamedOption(const OptionArray& options, int current, int delta)
        {
            std::vector<int> values;
            values.reserve(options.size());
            for (const auto& option : options)
            {
                values.push_back(option.value);
            }
            return Cycle(values, current, delta);
        }

        template <typename OptionArray>
        std::string LabelForNamedOption(const OptionArray& options, int value)
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

        const char* OnOffLabel(bool value)
        {
            return value ? "on" : "off";
        }

        void DrawSettingRow(bool selected, const char* label, std::string_view value)
        {
            const std::string text(value);
            ImGui::TextColored(selected ? Green : Primary, "%s %-18s %s",
                               selected ? ">" : " ", label, text.c_str());
        }
    }

    const char* SlateSettingsMode::ModeName() const
    {
        return Software::Slate::ModeIds::Settings;
    }

    void SlateSettingsMode::DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput)
    {
        auto& workspace = WorkspaceContext(context);
        auto& ui = UiState(context);
        if (!workspace.HasWorkspaceLoaded())
        {
            ShowWorkspaceSetup(context);
            return;
        }

        auto saveAppearance = [&]() {
            std::string error;
            if (!workspace.SaveThemeSettings(&error))
            {
                SetError(error);
            }
            else
            {
                SetStatus("appearance updated");
            }
        };

        auto saveEditor = [&]() {
            std::string error;
            if (!workspace.SaveEditorSettings(&error))
            {
                SetError(error);
            }
            else
            {
                SetStatus("editor updated");
            }
        };

        if (handleInput)
        {
            ui.navigation.SetCount(static_cast<std::size_t>(SettingsRow::Count));
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
            {
                ui.navigation.MoveNext();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
            {
                ui.navigation.MovePrevious();
            }

            const int delta = (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true) ? -1 :
                               (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) ||
                                IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
                                   ? 1
                                   : 0);
            if (delta != 0)
            {
                auto themeSettings = workspace.CurrentThemeSettings();
                auto editorSettings = workspace.CurrentEditorSettings();
                const auto row = static_cast<SettingsRow>(ui.navigation.Selected());

                switch (row)
                {
                case SettingsRow::ShellPreset:
                    themeSettings.shellPreset =
                        Cycle(workspace.Theme().ShellPresetIds(), themeSettings.shellPreset, delta);
                    workspace.SetThemeSettings(themeSettings);
                    saveAppearance();
                    break;
                case SettingsRow::MarkdownPreset:
                    themeSettings.markdownPreset =
                        Cycle(workspace.Theme().MarkdownPresetIds(), themeSettings.markdownPreset, delta);
                    workspace.SetThemeSettings(themeSettings);
                    saveAppearance();
                    break;
                case SettingsRow::FontSize:
                    editorSettings.fontSize = CycleNamedOption(FontSizeOptions(), editorSettings.fontSize, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::LineSpacing:
                    editorSettings.lineSpacing = CycleNamedOption(LineSpacingOptions(), editorSettings.lineSpacing, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::PageWidth:
                    editorSettings.pageWidth = CycleNamedOption(PageWidthOptions(), editorSettings.pageWidth, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::WordWrap:
                    editorSettings.wordWrap = !editorSettings.wordWrap;
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::CaretLine:
                    editorSettings.highlightCurrentLine = !editorSettings.highlightCurrentLine;
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::TabWidth:
                    editorSettings.tabWidth = CycleNamedOption(TabWidthOptions(), editorSettings.tabWidth, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::IndentMode:
                    editorSettings.indentWithTabs = !editorSettings.indentWithTabs;
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::AutoListContinuation:
                    editorSettings.autoListContinuation = !editorSettings.autoListContinuation;
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::PasteClipboardImages:
                    editorSettings.pasteClipboardImages = !editorSettings.pasteClipboardImages;
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::Count:
                    break;
                }
            }

            if (IsKeyPressed(ImGuiKey_R))
            {
                workspace.SetThemeSettings(Software::Slate::ThemeService::DefaultSettings());
                workspace.SetEditorSettings(Software::Slate::EditorSettingsService::DefaultSettings());
                saveAppearance();
                saveEditor();
                SetStatus("settings reset");
            }

            if (IsKeyPressed(ImGuiKey_Escape))
            {
                ShowHome(context);
                return;
            }
        }

        const auto themeSettings = workspace.CurrentThemeSettings();
        const auto editorSettings = workspace.CurrentEditorSettings();
        ui.navigation.SetCount(static_cast<std::size_t>(SettingsRow::Count));

        ImGui::TextColored(Cyan, "settings");
        ImGui::Separator();
        ImGui::TextColored(Muted, "Appearance presets are file-backed. Add your own in .slate/presets/.");
        ImGui::Dummy(ImVec2(1.0f, 10.0f));

        ImGui::TextColored(Muted, "appearance");
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::ShellPreset),
                       "shell palette",
                       workspace.Theme().ShellPresetLabel(themeSettings.shellPreset));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::MarkdownPreset),
                       "markdown accents",
                       workspace.Theme().MarkdownPresetLabel(themeSettings.markdownPreset));

        ImGui::Dummy(ImVec2(1.0f, 10.0f));
        ImGui::TextColored(Muted, "editor");
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::FontSize),
                       "font size", LabelForNamedOption(FontSizeOptions(), editorSettings.fontSize));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::LineSpacing),
                       "line spacing", LabelForNamedOption(LineSpacingOptions(), editorSettings.lineSpacing));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PageWidth),
                       "page width", LabelForNamedOption(PageWidthOptions(), editorSettings.pageWidth));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::WordWrap),
                       "word wrap", OnOffLabel(editorSettings.wordWrap));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::CaretLine),
                       "caret line", OnOffLabel(editorSettings.highlightCurrentLine));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::TabWidth),
                       "tab width", LabelForNamedOption(TabWidthOptions(), editorSettings.tabWidth));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::IndentMode),
                       "indentation", editorSettings.indentWithTabs ? "tabs" : "spaces");
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::AutoListContinuation),
                       "smart lists", OnOffLabel(editorSettings.autoListContinuation));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PasteClipboardImages),
                       "paste images", OnOffLabel(editorSettings.pasteClipboardImages));

        ImGui::Dummy(ImVec2(1.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        ImGui::TextColored(Muted, "preview");
        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        bool previewInCodeFence = false;
        DrawMarkdownLine("# Encounter Notes", previewInCodeFence);
        DrawMarkdownLine("## Combat Loop", previewInCodeFence);
        DrawMarkdownLine("- [ ] tune parry window", previewInCodeFence);
        DrawMarkdownLine("- [x] player dodge feels right", previewInCodeFence);
        DrawMarkdownLine("> pace matters more than particles", previewInCodeFence);
        DrawMarkdownLine("A [link](Docs/Combat.md), **bold**, *italic*, and `code`.", previewInCodeFence);
        DrawMarkdownLine("| stat | value |", previewInCodeFence);
        DrawMarkdownLine("![](assets/encounter/board.png)", previewInCodeFence);

        ImGui::Dummy(ImVec2(1.0f, 10.0f));
        ImGui::TextColored(Muted, "Editor settings save to .slate/editor.tsv. Appearance choices save to .slate/theme.tsv.");
    }

    std::string SlateSettingsMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        (void)context;
        return "(up/down) choose   (left/right) change   (r) reset all   (esc) home";
    }
}
