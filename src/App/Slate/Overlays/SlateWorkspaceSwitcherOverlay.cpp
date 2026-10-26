#include "App/Slate/Overlays/SlateWorkspaceSwitcherOverlay.h"

#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/UI/SlateDesign.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>

namespace Software::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        bool IsKeyPressed(ImGuiKey key)
        {
            return ImGui::IsKeyPressed(key, false);
        }
    }

    void SlateWorkspaceSwitcherOverlay::Open(const SlateWorkspaceContext& workspace)
    {
        const auto& registry = workspace.WorkspaceRegistry();
        const auto& vaults = registry.Vaults();
        m_navigation.SetCount(vaults.size());
        m_navigation.Reset();
        if (const auto* activeVault = registry.ActiveVault())
        {
            for (std::size_t i = 0; i < vaults.size(); ++i)
            {
                if (vaults[i].id == activeVault->id)
                {
                    m_navigation.SetSelected(i);
                    break;
                }
            }
        }
        m_open = true;
    }

    void SlateWorkspaceSwitcherOverlay::Close()
    {
        m_open = false;
    }

    void SlateWorkspaceSwitcherOverlay::Reset()
    {
        m_open = false;
        m_navigation.Reset();
        m_navigation.SetCount(0);
    }

    bool SlateWorkspaceSwitcherOverlay::IsOpen() const
    {
        return m_open;
    }

    std::string SlateWorkspaceSwitcherOverlay::HelperText(const ShortcutService& shortcuts) const
    {
        return "(" + shortcuts.Label(ShortcutAction::NavigateUp) + "/" + shortcuts.Label(ShortcutAction::NavigateDown) + ") choose   " + shortcuts.Helper(ShortcutAction::Accept, "switch") + "   " + shortcuts.Helper(ShortcutAction::WorkspaceNew, "new") + "   " + shortcuts.Helper(ShortcutAction::WorkspaceRemove, "remove") + "   " + shortcuts.Helper(ShortcutAction::Cancel, "close");
    }

    void SlateWorkspaceSwitcherOverlay::Draw(Software::Core::Runtime::AppContext& context,
                                             const WorkspaceSwitcherCallbacks& callbacks)
    {
        if (!m_open)
        {
            return;
        }

        auto workspace = context.services.Resolve<SlateWorkspaceContext>();
        if (!workspace)
        {
            Close();
            return;
        }

        auto& registry = workspace->WorkspaceRegistry();
        const auto& shortcuts = workspace->Shortcuts();
        const auto& vaults = registry.Vaults();
        m_navigation.SetCount(vaults.size());

        if (shortcuts.Pressed(ShortcutAction::NavigateDown, true))
        {
            m_navigation.MoveNext();
        }
        if (shortcuts.Pressed(ShortcutAction::NavigateUp, true))
        {
            m_navigation.MovePrevious();
        }
        if ((shortcuts.Pressed(ShortcutAction::Accept)) &&
            m_navigation.HasSelection() && !vaults.empty())
        {
            const auto selectedVault = vaults[m_navigation.Selected()];
            Close();
            if (callbacks.openVault)
            {
                callbacks.openVault(selectedVault);
            }
            return;
        }
        if (shortcuts.Pressed(ShortcutAction::WorkspaceNew))
        {
            Close();
            if (callbacks.showSetup)
            {
                callbacks.showSetup();
            }
            if (callbacks.setStatus)
            {
                callbacks.setStatus("press " + shortcuts.Label(ShortcutAction::WorkspaceNew) + " to create a workspace");
            }
            return;
        }
        if (shortcuts.Pressed(ShortcutAction::WorkspaceRemove) && m_navigation.HasSelection() && !vaults.empty())
        {
            const auto id = vaults[m_navigation.Selected()].id;
            if (registry.RemoveVault(id))
            {
                registry.Save(nullptr);
                m_navigation.SetCount(registry.Vaults().size());
                if (callbacks.setStatus)
                {
                    callbacks.setStatus("workspace removed from list");
                }
                if (registry.Vaults().empty())
                {
                    Close();
                    if (callbacks.showSetup)
                    {
                        callbacks.showSetup();
                    }
                    return;
                }
            }
        }
        if (shortcuts.Pressed(ShortcutAction::Cancel))
        {
            Close();
            return;
        }

        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight * 2.0f + 14.0f;
        const float listHeight = std::max(1.0f, static_cast<float>(std::min<std::size_t>(vaults.size(), 6))) * (rowHeight + 4.0f);
        const ImVec2 size(std::min(640.0f, std::max(380.0f, ImGui::GetWindowWidth() * 0.46f)), listHeight + 92.0f);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   std::max(40.0f, (ImGui::GetWindowHeight() - Tokens::FooterHeight - size.y) * 0.42f)));
        BeginCard("WorkspaceOverlay", size, Green, Tokens::SpaceL);
        const std::string count = std::to_string(vaults.size()) + (vaults.size() == 1 ? " workspace" : " workspaces");
        CardTitle("workspaces", Green, count.c_str());

        if (vaults.empty())
        {
            ImGui::TextColored(TextFaint(), "no workspaces yet");
        }
        else
        {
            const auto* activeVault = registry.ActiveVault();
            auto* drawList = ImGui::GetWindowDrawList();
            for (std::size_t i = 0; i < vaults.size(); ++i)
            {
                const bool selected = i == m_navigation.Selected();
                const bool active = activeVault && vaults[i].id == activeVault->id;
                const auto& vault = vaults[i];
                ImGui::PushID(static_cast<int>(i));
                ImVec2 rowMin;
                ImVec2 rowMax;
                const bool clicked = ListRow("##vault", selected, rowHeight, Green, &rowMin, &rowMax);
                ImGui::PopID();

                // Emoji tile, title, path underneath, and a pill for the open workspace
                const float tile = lineHeight * 1.7f;
                const ImVec2 tileMin(rowMin.x + 12.0f, rowMin.y + (rowHeight - tile) * 0.5f);
                drawList->AddRectFilled(tileMin, ImVec2(tileMin.x + tile, tileMin.y + tile), U32(Mix(Surface(3), Green, selected ? 0.18f : 0.06f)),
                                        Tokens::RadiusM);
                const float emojiWidth = ImGui::CalcTextSize(vault.emoji.c_str()).x;
                drawList->AddText(ImVec2(tileMin.x + (tile - emojiWidth) * 0.5f, tileMin.y + (tile - lineHeight) * 0.5f), U32(Primary),
                                  vault.emoji.c_str());
                const float textX = tileMin.x + tile + 12.0f;
                drawList->AddText(ImVec2(textX, rowMin.y + 6.0f), U32(selected ? Green : Primary), vault.title.c_str());
                drawList->PushClipRect(rowMin, ImVec2(rowMax.x - 90.0f, rowMax.y), true);
                drawList->AddText(ImVec2(textX, rowMin.y + 8.0f + lineHeight), U32(TextFaint()), vault.path.generic_string().c_str());
                drawList->PopClipRect();
                if (active)
                {
                    DrawPill(drawList, ImVec2(rowMax.x - PillWidth("open") - 12.0f, rowMin.y + 8.0f), "open", Green);
                }
                if (clicked)
                {
                    const auto clickedVault = vault;
                    EndCard();
                    Close();
                    if (callbacks.openVault)
                    {
                        callbacks.openVault(clickedVault);
                    }
                    return;
                }
                ImGui::Dummy(ImVec2(1.0f, 2.0f));
            }
        }

        EndCard();
    }
}
