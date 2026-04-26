#include "Modes/Slate/SlateSettingsMode.h"

#include "App/Slate/Editor/EditorSettingsService.h"
#include "App/Slate/Core/AppPaths.h"
#include "App/Slate/Core/SlateModeIds.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
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
            ShowWhitespace,
            CaretLine,
            TabWidth,
            PanelLayout,
            PreviewFollow,
            PanelMotion,
            ScrollMotion,
            ScrollbarStyle,
            CaretMotion,
            LinkUnderline,
            IndentMode,
            AutoListContinuation,
            PasteClipboardImages,
            ImageStoragePolicy,
            Count
        };

        struct NamedIntOption
        {
            int value = 0;
            const char* label = "";
        };

        const std::array<NamedIntOption, 15>& FontSizeOptions()
        {
            static constexpr std::array<NamedIntOption, 15> options{{
                {10, "10"},
                {11, "11"},
                {12, "12"},
                {13, "13"},
                {14, "14"},
                {15, "15"},
                {16, "16"},
                {17, "17"},
                {18, "18"},
                {19, "19"},
                {20, "20"},
                {21, "21"},
                {22, "22"},
                {23, "23"},
                {24, "24"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 5>& LineSpacingOptions()
        {
            static constexpr std::array<NamedIntOption, 5> options{{
                {0, "tight"},
                {2, "compact"},
                {3, "balanced"},
                {7, "relaxed"},
                {11, "airy"},
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

        const std::array<NamedIntOption, 2>& PanelLayoutOptions()
        {
            static constexpr std::array<NamedIntOption, 2> options{{
                {0, "margins"},
                {1, "split"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 4>& PreviewFollowOptions()
        {
            static constexpr std::array<NamedIntOption, 4> options{{
                {0, "manual"},
                {1, "caret"},
                {2, "scroll"},
                {3, "mixed"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 4>& PanelMotionOptions()
        {
            static constexpr std::array<NamedIntOption, 4> options{{
                {0, "instant"},
                {1, "quick"},
                {2, "smooth"},
                {3, "floaty"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 3>& ScrollMotionOptions()
        {
            static constexpr std::array<NamedIntOption, 3> options{{
                {0, "instant"},
                {1, "smooth"},
                {2, "glide"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 3>& ScrollbarStyleOptions()
        {
            static constexpr std::array<NamedIntOption, 3> options{{
                {0, "slim"},
                {1, "soft"},
                {2, "bold"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 3>& CaretMotionOptions()
        {
            static constexpr std::array<NamedIntOption, 3> options{{
                {0, "crisp"},
                {1, "calm"},
                {2, "anchored"},
            }};
            return options;
        }

        const std::array<NamedIntOption, 5>& ImageStoragePolicyOptions()
        {
            static constexpr std::array<NamedIntOption, 5> options{{
                {static_cast<int>(Software::Slate::ImageStoragePolicy::WorkspaceMediaFolder), "workspace media"},
                {static_cast<int>(Software::Slate::ImageStoragePolicy::SameFolderAsNote), "same folder"},
                {static_cast<int>(Software::Slate::ImageStoragePolicy::SubfolderUnderNoteFolder), "note attachments"},
                {static_cast<int>(Software::Slate::ImageStoragePolicy::LinkOriginal), "link original"},
                {static_cast<int>(Software::Slate::ImageStoragePolicy::EmbedInMarkdown), "embed in note"},
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
            ImGui::TextColored(selected ? Green : Primary, "%s %-22s %s",
                               selected ? ">" : " ", label, text.c_str());
        }

        std::string PairLabel(const Software::Slate::ShortcutService& shortcuts,
                              Software::Slate::ShortcutAction first,
                              Software::Slate::ShortcutAction second)
        {
            return shortcuts.Label(first) + "/" + shortcuts.Label(second);
        }
    }

    const char* SlateSettingsMode::ModeName() const
    {
        return Software::Slate::ModeIds::Settings;
    }

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

        const std::size_t settingsRowCount = static_cast<std::size_t>(SettingsRow::Count);
        const std::size_t shortcutRowOffset = settingsRowCount;
        const auto& shortcutDefinitions = shortcuts.Definitions();
        const std::size_t totalRows = settingsRowCount + shortcutDefinitions.size();

        if (handleInput && !m_shortcutCaptureOpen)
        {
            ui.navigation.SetCount(totalRows);
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateDown, true))
            {
                ui.navigation.MoveNext();
            }
            if (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateUp, true))
            {
                ui.navigation.MovePrevious();
            }

            const std::size_t selectedRow = ui.navigation.Selected();
            if (selectedRow >= shortcutRowOffset)
            {
                const std::size_t shortcutIndex = selectedRow - shortcutRowOffset;
                if (shortcutIndex < shortcutDefinitions.size() && shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
                {
                    m_shortcutCaptureOpen = true;
                    m_shortcutCaptureArmed = false;
                    m_shortcutCaptureAction = shortcutDefinitions[shortcutIndex].action;
                    m_shortcutCaptureLabel = std::string(shortcutDefinitions[shortcutIndex].group) + ": " +
                                             shortcutDefinitions[shortcutIndex].label;
                }
            }

            const int delta = selectedRow < shortcutRowOffset
                                  ? (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateLeft, true) ? -1 :
                                     (shortcuts.Pressed(Software::Slate::ShortcutAction::NavigateRight, true) ||
                                      shortcuts.Pressed(Software::Slate::ShortcutAction::Accept))
                                         ? 1
                                         : 0)
                                  : 0;
            if (delta != 0)
            {
                auto themeSettings = workspace.CurrentThemeSettings();
                auto editorSettings = workspace.CurrentEditorSettings();
                const auto row = static_cast<SettingsRow>(selectedRow);

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
                case SettingsRow::ShowWhitespace:
                    editorSettings.showWhitespace = !editorSettings.showWhitespace;
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
                case SettingsRow::PanelLayout:
                    editorSettings.panelLayout = CycleNamedOption(PanelLayoutOptions(), editorSettings.panelLayout, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::PreviewFollow:
                    editorSettings.previewFollowMode =
                        CycleNamedOption(PreviewFollowOptions(), editorSettings.previewFollowMode, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::PanelMotion:
                    editorSettings.panelMotion = CycleNamedOption(PanelMotionOptions(), editorSettings.panelMotion, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::ScrollMotion:
                    editorSettings.scrollMotion = CycleNamedOption(ScrollMotionOptions(), editorSettings.scrollMotion, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::ScrollbarStyle:
                    editorSettings.scrollbarStyle =
                        CycleNamedOption(ScrollbarStyleOptions(), editorSettings.scrollbarStyle, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::CaretMotion:
                    editorSettings.caretMotion = CycleNamedOption(CaretMotionOptions(), editorSettings.caretMotion, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::LinkUnderline:
                    editorSettings.linkUnderline = !editorSettings.linkUnderline;
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
                case SettingsRow::ImageStoragePolicy:
                    editorSettings.imageStoragePolicy =
                        CycleNamedOption(ImageStoragePolicyOptions(), editorSettings.imageStoragePolicy, delta);
                    workspace.SetEditorSettings(editorSettings);
                    saveEditor();
                    break;
                case SettingsRow::Count:
                    break;
                    }
            }

            if (shortcuts.Pressed(Software::Slate::ShortcutAction::SettingsReset))
            {
                workspace.SetThemeSettings(Software::Slate::ThemeService::DefaultSettings());
                workspace.SetEditorSettings(Software::Slate::EditorSettingsService::DefaultSettings());
                saveAppearance();
                saveEditor();
                std::string error;
                if (!shortcuts.Reset(&error))
                {
                    SetError(error);
                }
                else
                {
                    SetStatus("settings and shortcuts reset");
                }
            }

            if (shortcuts.Pressed(Software::Slate::ShortcutAction::Cancel))
            {
                ShowHome(context);
                return;
            }
        }

        const auto themeSettings = workspace.CurrentThemeSettings();
        const auto editorSettings = workspace.CurrentEditorSettings();
        ui.navigation.SetCount(totalRows);

        const float contentHeight = std::max(1.0f, ImGui::GetWindowHeight() - ImGui::GetCursorPosY() - 68.0f);
        const float layoutWidth = CenteredColumnWidth(1180.0f);
        const float gap = 24.0f;
        const float previewWidth = std::clamp(layoutWidth * 0.38f, 360.0f, 460.0f);
        const float settingsWidth = std::max(420.0f, layoutWidth - previewWidth - gap);
        SetCursorCenteredForWidth(layoutWidth);

        ImGui::BeginGroup();
        ImGui::BeginChild("SettingsScroll", ImVec2(settingsWidth, contentHeight), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar);

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
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::ShowWhitespace),
                       "whitespace", OnOffLabel(editorSettings.showWhitespace));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::CaretLine),
                       "caret line", OnOffLabel(editorSettings.highlightCurrentLine));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::TabWidth),
                       "tab width", LabelForNamedOption(TabWidthOptions(), editorSettings.tabWidth));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PanelLayout),
                       "side panels", LabelForNamedOption(PanelLayoutOptions(), editorSettings.panelLayout));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PreviewFollow),
                       "preview sync", LabelForNamedOption(PreviewFollowOptions(), editorSettings.previewFollowMode));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PanelMotion),
                       "panel motion", LabelForNamedOption(PanelMotionOptions(), editorSettings.panelMotion));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::ScrollMotion),
                       "scroll motion", LabelForNamedOption(ScrollMotionOptions(), editorSettings.scrollMotion));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::ScrollbarStyle),
                       "scrollbars", LabelForNamedOption(ScrollbarStyleOptions(), editorSettings.scrollbarStyle));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::CaretMotion),
                       "caret feel", LabelForNamedOption(CaretMotionOptions(), editorSettings.caretMotion));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::LinkUnderline),
                       "link underline", OnOffLabel(editorSettings.linkUnderline));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::IndentMode),
                       "indentation", editorSettings.indentWithTabs ? "tabs" : "spaces");
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::AutoListContinuation),
                       "smart lists", OnOffLabel(editorSettings.autoListContinuation));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::PasteClipboardImages),
                       "paste images", OnOffLabel(editorSettings.pasteClipboardImages));
        DrawSettingRow(ui.navigation.Selected() == static_cast<std::size_t>(SettingsRow::ImageStoragePolicy),
                       "image storage", LabelForNamedOption(ImageStoragePolicyOptions(), editorSettings.imageStoragePolicy));

        ImGui::Dummy(ImVec2(1.0f, 12.0f));
        ImGui::TextColored(Muted, "shortcuts");
        ImGui::TextColored(Muted, "select a shortcut and press %s to rebind; overrides save to %s",
                           shortcuts.Label(Software::Slate::ShortcutAction::Accept).c_str(),
                           shortcuts.SettingsPath().generic_string().c_str());
        ImGui::TextColored(Muted, "default bindings live in %s", Software::Slate::ShortcutService::DefaultsSourcePath());
        for (std::size_t i = 0; i < shortcutDefinitions.size(); ++i)
        {
            const auto& definition = shortcutDefinitions[i];
            const bool selected = ui.navigation.Selected() == shortcutRowOffset + i;
            const std::string label = std::string(definition.group) + ": " + definition.label;
            DrawSettingRow(selected, label.c_str(), shortcuts.Label(definition.action));
        }

        ImGui::Dummy(ImVec2(1.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        ImGui::TextColored(Muted, "config root: %s", Software::Slate::AppPaths::ConfigDirectory().generic_string().c_str());
        ImGui::TextColored(Muted, "workspace/settings.tsv, input/shortcuts.tsv, appearance/theme.tsv, editor/editor.tsv");
        ImGui::EndChild();

        ImGui::SameLine(0.0f, gap);
        ImGui::BeginChild("SettingsPreviewPanel", ImVec2(previewWidth, contentHeight), true,
                          ImGuiWindowFlags_NoBackground);
        ImGui::TextColored(Muted, "preview");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(1.0f, 6.0f));

        bool previewInCodeFence = false;
        const float baseFontSize = ImGui::GetFontSize() > 1.0f ? ImGui::GetFontSize() : 18.0f;
        const float previewFontScale =
            std::clamp(static_cast<float>(editorSettings.fontSize) / std::max(1.0f, baseFontSize), 0.65f, 1.50f);
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::SetWindowFontScale(previewFontScale);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(style.ItemSpacing.x, static_cast<float>(std::max(0, editorSettings.lineSpacing))));
        DrawMarkdownLine("# Encounter Notes", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("## Combat Loop", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("- [ ] tune parry window", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("- [x] player dodge feels right", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("> pace matters more than particles", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("A [link](Docs/Combat.md), **bold**, *italic*, and `code`.", previewInCodeFence, false, false,
                         previewFontScale);
        DrawMarkdownLine("| stat | value |", previewInCodeFence, false, false, previewFontScale);
        DrawMarkdownLine("![](assets/encounter/board.png)", previewInCodeFence, false, false, previewFontScale);
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndChild();
        ImGui::EndGroup();

        if (m_shortcutCaptureOpen)
        {
            ImGui::OpenPopup("Bind shortcut");
        }

        if (ImGui::BeginPopupModal("Bind shortcut", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextColored(Cyan, "bind shortcut");
            ImGui::Separator();
            ImGui::TextColored(Primary, "%s", m_shortcutCaptureLabel.c_str());
            ImGui::Dummy(ImVec2(1.0f, 8.0f));
            ImGui::TextColored(Muted, "current: %s", shortcuts.Label(m_shortcutCaptureAction).c_str());
            ImGui::TextColored(Muted, "press the new shortcut now");
            ImGui::TextColored(Muted, "esc cancels");
            ImGui::Dummy(ImVec2(1.0f, 8.0f));

            if (!m_shortcutCaptureArmed)
            {
                m_shortcutCaptureArmed = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                m_shortcutCaptureOpen = false;
                m_shortcutCaptureArmed = false;
                m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
                m_shortcutCaptureLabel.clear();
                ImGui::CloseCurrentPopup();
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
                    m_shortcutCaptureOpen = false;
                    m_shortcutCaptureArmed = false;
                    m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
                    m_shortcutCaptureLabel.clear();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (ImGui::Button("cancel"))
            {
                m_shortcutCaptureOpen = false;
                m_shortcutCaptureArmed = false;
                m_shortcutCaptureAction = Software::Slate::ShortcutAction::Count;
                m_shortcutCaptureLabel.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    std::string SlateSettingsMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        const auto& shortcuts = WorkspaceContext(const_cast<Software::Core::Runtime::AppContext&>(context)).Shortcuts();
        return "(" + PairLabel(shortcuts, Software::Slate::ShortcutAction::NavigateUp,
                                Software::Slate::ShortcutAction::NavigateDown) + ") choose   (" +
               PairLabel(shortcuts, Software::Slate::ShortcutAction::NavigateLeft,
                         Software::Slate::ShortcutAction::NavigateRight) + ") change setting   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Accept, "rebind shortcut") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::SettingsReset, "reset all") + "   " +
               shortcuts.Helper(Software::Slate::ShortcutAction::Cancel, "home");
    }
}
