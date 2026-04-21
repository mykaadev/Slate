#include "Modes/Slate/SlateSettingsMode.h"

#include "App/Slate/SlateModeIds.h"
#include "App/Slate/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <vector>

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

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

        const auto cycle = [](const std::vector<std::string>& ids, const std::string& current, int delta) {
            auto it = std::find(ids.begin(), ids.end(), current);
            std::size_t index = it == ids.end() ? 0 : static_cast<std::size_t>(it - ids.begin());
            const std::size_t count = ids.size();
            if (count == 0)
            {
                return current;
            }

            const int next = static_cast<int>(index) + delta;
            index = static_cast<std::size_t>((next % static_cast<int>(count) + static_cast<int>(count)) %
                                             static_cast<int>(count));
            return ids[index];
        };

        auto applyAndSave = [&]() {
            std::string error;
            if (!workspace.SaveThemeSettings(&error))
            {
                SetError(error);
            }
            else
            {
                SetStatus("theme updated");
            }
        };

        if (handleInput)
        {
            ui.navigation.SetCount(2);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
            {
                ui.navigation.MoveNext();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
            {
                ui.navigation.MovePrevious();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
            {
                auto settings = workspace.CurrentThemeSettings();
                if (ui.navigation.Selected() == 0)
                {
                    settings.shellPreset = cycle(Software::Slate::ThemeService::ShellPresetIds(), settings.shellPreset, -1);
                }
                else
                {
                    settings.markdownPreset =
                        cycle(Software::Slate::ThemeService::MarkdownPresetIds(), settings.markdownPreset, -1);
                }
                workspace.SetThemeSettings(settings);
                applyAndSave();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true) || IsKeyPressed(ImGuiKey_Enter) ||
                IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                auto settings = workspace.CurrentThemeSettings();
                if (ui.navigation.Selected() == 0)
                {
                    settings.shellPreset = cycle(Software::Slate::ThemeService::ShellPresetIds(), settings.shellPreset, 1);
                }
                else
                {
                    settings.markdownPreset =
                        cycle(Software::Slate::ThemeService::MarkdownPresetIds(), settings.markdownPreset, 1);
                }
                workspace.SetThemeSettings(settings);
                applyAndSave();
            }
            if (IsKeyPressed(ImGuiKey_R))
            {
                workspace.SetThemeSettings(Software::Slate::ThemeService::DefaultSettings());
                applyAndSave();
            }
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                ShowHome(context);
                return;
            }
        }

        const auto settings = workspace.CurrentThemeSettings();
        ui.navigation.SetCount(2);

        ImGui::TextColored(Cyan, "settings");
        ImGui::Separator();
        ImGui::TextColored(Muted, "Theme changes save to this workspace.");
        ImGui::Dummy(ImVec2(1.0f, 10.0f));

        const bool shellSelected = ui.navigation.Selected() == 0;
        const bool markdownSelected = ui.navigation.Selected() == 1;
        ImGui::TextColored(shellSelected ? Green : Primary, "%s overall theme     %s", shellSelected ? ">" : " ",
                           Software::Slate::ThemeService::ShellPresetLabel(settings.shellPreset).c_str());
        ImGui::TextColored(markdownSelected ? Green : Primary, "%s markdown colors  %s",
                           markdownSelected ? ">" : " ",
                           Software::Slate::ThemeService::MarkdownPresetLabel(settings.markdownPreset).c_str());

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
    }

    std::string SlateSettingsMode::ModeHelperText(const Software::Core::Runtime::AppContext& context) const
    {
        (void)context;
        return "(up/down) choose   (left/right) change   (r) reset   (esc) home";
    }
}
