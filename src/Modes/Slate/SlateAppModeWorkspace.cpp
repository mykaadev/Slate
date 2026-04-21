#include "Modes/Slate/SlateAppMode.h"

#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

namespace Software::Modes::Slate
{
    using namespace Software::Slate::UI;

    void SlateAppMode::OpenWorkspace(const Software::Slate::fs::path& root)
    {
        if (m_documents.HasOpenDocument())
        {
            if (!SaveActiveDocument())
            {
                return;
            }
            m_documents.Close();
            m_editor.Load("", "\n");
            m_editorTextFocused = false;
        }

        std::string error;
        if (!m_workspace.Open(root, &error))
        {
            SetError(error);
            return;
        }
        m_documents.SetWorkspaceRoot(m_workspace.Root());
        m_assets.SetWorkspaceRoot(m_workspace.Root());
        CollapseAllWorkspaceFolders();
        m_search.Rebuild(m_workspace);
        m_lastIndexSeconds = 0.0;
        m_workspaceLoaded = true;
        SetStatus("workspace opened: " + m_workspace.Root().string());
    }

    void SlateAppMode::OpenVault(const Software::Slate::WorkspaceVault& vault)
    {
        std::error_code ec;
        Software::Slate::fs::create_directories(vault.path, ec);
        if (ec)
        {
            SetError(ec.message());
            return;
        }

        OpenWorkspace(vault.path);
        if (m_workspaceRegistry.SetActiveVault(vault.id))
        {
            m_workspaceRegistry.Save(nullptr);
        }
        m_workspaceLoaded = true;
        SetStatus(vault.emoji + " " + vault.title);
    }

    void SlateAppMode::BeginWorkspaceCreate()
    {
        BeginPrompt(PromptAction::WorkspaceTitle, "workspace title", "My Workspace");
    }

    void SlateAppMode::DrawWorkspaceSetup()
    {
        ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y * 0.18f));
        TextCentered("Slate", Cyan);
        ImGui::Spacing();
        TextCentered("no workspace yet", Amber);
        ImGui::Dummy(ImVec2(1.0f, 28.0f));
        TextCentered("create a local markdown workspace to begin", Primary);
        ImGui::Dummy(ImVec2(1.0f, 24.0f));

        const float columnWidth = 360.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - columnWidth) * 0.5f);
        ImGui::BeginGroup();
        TextLine("n", "new workspace");
        TextLine(":", "command");
        TextLine("q", "quit");
        ImGui::EndGroup();

        const std::string defaultPath =
            "default: " + Software::Slate::WorkspaceRegistryService::DefaultWorkspaceRoot().string();
        ImGui::Dummy(ImVec2(1.0f, 24.0f));
        TextCentered(defaultPath.c_str(), Muted);
    }

    void SlateAppMode::DrawWorkspaceSwitcher()
    {
        const auto& vaults = m_workspaceRegistry.Vaults();
        m_workspaceNavigation.SetCount(vaults.size());

        ImGui::TextColored(Cyan, "workspaces");
        ImGui::Separator();

        if (vaults.empty())
        {
            ImGui::TextColored(Muted, "no workspaces registered");
            return;
        }

        for (std::size_t i = 0; i < vaults.size(); ++i)
        {
            const bool selected = i == m_workspaceNavigation.Selected();
            const auto& vault = vaults[i];
            ImGui::TextColored(selected ? Green : Primary, "%s %s %s", selected ? ">" : " ", vault.emoji.c_str(),
                               vault.title.c_str());
            ImGui::SameLine();
            ImGui::TextColored(Muted, "%s", vault.path.string().c_str());
        }
    }

    void SlateAppMode::HandleWorkspaceKeys()
    {
        if (m_screen == Screen::WorkspaceSetup)
        {
            if (IsKeyPressed(ImGuiKey_N))
            {
                BeginWorkspaceCreate();
            }
            return;
        }

        const auto& vaults = m_workspaceRegistry.Vaults();
        m_workspaceNavigation.SetCount(vaults.size());

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_workspaceNavigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_workspaceNavigation.MovePrevious();
        }
        if ((IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
            m_workspaceNavigation.HasSelection() && !vaults.empty())
        {
            const auto vault = vaults[m_workspaceNavigation.Selected()];
            OpenVault(vault);
            m_screen = Screen::Home;
        }
        if (IsKeyPressed(ImGuiKey_N))
        {
            BeginWorkspaceCreate();
        }
        if (IsKeyPressed(ImGuiKey_D) && m_workspaceNavigation.HasSelection() && !vaults.empty())
        {
            const auto id = vaults[m_workspaceNavigation.Selected()].id;
            if (m_workspaceRegistry.RemoveVault(id))
            {
                m_workspaceRegistry.Save(nullptr);
                m_workspaceNavigation.SetCount(m_workspaceRegistry.Vaults().size());
                SetStatus("workspace removed from list");
            }
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_screen = m_workspaceLoaded ? Screen::Home : Screen::WorkspaceSetup;
        }
    }
}
