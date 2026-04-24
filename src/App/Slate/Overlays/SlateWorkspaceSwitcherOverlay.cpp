#include "App/Slate/Overlays/SlateWorkspaceSwitcherOverlay.h"

#include "App/Slate/State/SlateWorkspaceContext.h"
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

    std::string SlateWorkspaceSwitcherOverlay::HelperText() const
    {
        return "(up/down) choose   (enter) switch   (n) new   (d) remove   (esc) close";
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
        const auto& vaults = registry.Vaults();
        m_navigation.SetCount(vaults.size());

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_navigation.MovePrevious();
        }
        if ((IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
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
        if (IsKeyPressed(ImGuiKey_N))
        {
            Close();
            if (callbacks.showSetup)
            {
                callbacks.showSetup();
            }
            if (callbacks.setStatus)
            {
                callbacks.setStatus("press n to create a workspace");
            }
            return;
        }
        if (IsKeyPressed(ImGuiKey_D) && m_navigation.HasSelection() && !vaults.empty())
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
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            Close();
            return;
        }

        const ImVec2 size(std::min(760.0f, std::max(360.0f, ImGui::GetWindowWidth() * 0.58f)),
                          std::min(460.0f, std::max(240.0f, ImGui::GetWindowHeight() * 0.46f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Green.x, Green.y, Green.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("WorkspaceOverlay", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImGui::TextColored(Green, "workspaces");
        ImGui::Separator();

        if (vaults.empty())
        {
            ImGui::TextColored(Muted, "no workspaces registered");
        }
        else
        {
            const auto* activeVault = registry.ActiveVault();
            for (std::size_t i = 0; i < vaults.size(); ++i)
            {
                const bool selected = i == m_navigation.Selected();
                const bool active = activeVault && vaults[i].id == activeVault->id;
                const auto& vault = vaults[i];
                ImGui::TextColored(selected ? Green : Primary, "%s %s %s%s",
                                   selected ? ">" : " ", vault.emoji.c_str(), vault.title.c_str(),
                                   active ? " *" : "");
                ImGui::SameLine();
                ImGui::TextColored(Muted, "%s", vault.path.string().c_str());
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
}
