#include "Modes/Slate/SlateAppMode.h"

#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <utility>

namespace Software::Modes::Slate
{
    using Software::Slate::UI::IsShiftQuestion;

    void SlateAppMode::HandleGlobalKeys(Software::Core::Runtime::AppContext& context)
    {
        if (IsCtrlPressed(ImGuiKey_S))
        {
            SaveActiveDocument();
        }

        if (m_screen == Screen::Prompt || m_screen == Screen::Confirm || m_searchOverlayOpen)
        {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (m_editorTextFocused || io.WantTextInput)
        {
            return;
        }

        if (IsShiftQuestion())
        {
            if (m_screen == Screen::Help)
            {
                m_screen = m_returnScreen;
            }
            else
            {
                m_returnScreen = m_screen;
                m_screen = Screen::Help;
            }
            return;
        }

        if (io.KeyShift && IsKeyPressed(ImGuiKey_Semicolon))
        {
            BeginPrompt(PromptAction::Command, "command", "");
            return;
        }

        if (m_screen != Screen::FileTree && m_screen != Screen::Library && !io.KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            BeginSearchOverlay(true);
            return;
        }

        if ((m_screen == Screen::Home || m_screen == Screen::WorkspaceSetup) && IsKeyPressed(ImGuiKey_Q))
        {
            BeginConfirm(ConfirmAction::Quit, "quit Slate? y quit / esc cancel");
        }

        if (IsCtrlPressed(ImGuiKey_Q))
        {
            BeginConfirm(ConfirmAction::Quit, "quit Slate? y quit / esc cancel");
        }
    }

    void SlateAppMode::HandleListKeys()
    {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_navigation.MovePrevious();
        }

        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            if (m_screen == Screen::Outline)
            {
                if (m_navigation.HasSelection() && !m_visibleHeadings.empty())
                {
                    SetStatus("heading line " + std::to_string(m_visibleHeadings[m_navigation.Selected()].line));
                }
                m_screen = Screen::Editor;
            }
            else
            {
                OpenSelectedPath();
            }
        }

        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_screen = (m_screen == Screen::Outline) ? Screen::Editor : Screen::Home;
        }

        if (IsKeyPressed(ImGuiKey_N))
        {
            BeginFolderPicker();
        }
    }

    void SlateAppMode::HandleTreeKeys(bool folderPicker)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            m_filterActive = true;
            m_focusFilter = true;
        }

        if (m_filterActive)
        {
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                m_filterActive = false;
                return;
            }
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            const bool wasMovePicker = m_folderPickerAction == FolderPickerAction::MoveDestination;
            m_folderPickerActive = false;
            m_folderPickerAction = FolderPickerAction::None;
            m_screen = wasMovePicker ? Screen::FileTree : Screen::Home;
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_navigation.MovePrevious();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            ToggleSelectedFolder(true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            ToggleSelectedFolder(false);
        }
        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            ActivateSelectedTreeRow();
        }

        if (m_filterActive)
        {
            return;
        }

        if (IsKeyPressed(ImGuiKey_A))
        {
            BeginFolderCreate();
        }

        if (!folderPicker && IsKeyPressed(ImGuiKey_M) && m_navigation.HasSelection() && !m_treeRows.empty())
        {
            BeginRenameMove(m_treeRows[m_navigation.Selected()].relativePath);
        }

        if (!folderPicker && (IsKeyPressed(ImGuiKey_D) || IsKeyPressed(ImGuiKey_Delete)) && m_navigation.HasSelection() &&
            !m_treeRows.empty())
        {
            BeginDelete(m_treeRows[m_navigation.Selected()].relativePath);
        }

        if (!folderPicker && IsKeyPressed(ImGuiKey_N))
        {
            BeginFolderPicker();
        }
    }

    void SlateAppMode::HandleSearchOverlayKeys()
    {
        if (m_searchOverlayScope == SearchOverlayScope::Workspace && IsKeyPressed(ImGuiKey_Tab))
        {
            m_searchMode = m_searchMode == Software::Slate::SearchMode::FileNames
                               ? Software::Slate::SearchMode::FullText
                               : Software::Slate::SearchMode::FileNames;
            m_searchNavigation.Reset();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_searchNavigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_searchNavigation.MovePrevious();
        }
        if ((IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter)) &&
            m_searchNavigation.HasSelection() && !m_visibleSearchResults.empty())
        {
            if (m_searchOverlayScope == SearchOverlayScope::Document && ImGui::GetIO().KeyShift)
            {
                m_searchNavigation.MovePrevious();
            }
            OpenSelectedSearchResult();
            return;
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            CloseSearchOverlay();
        }
    }

    void SlateAppMode::HandleHomeKeys(Software::Core::Runtime::AppContext& context)
    {
        (void)context;
        if (IsKeyPressed(ImGuiKey_J))
        {
            OpenTodayJournal();
        }
        else if (IsKeyPressed(ImGuiKey_N))
        {
            BeginFolderPicker();
        }
        else if (IsKeyPressed(ImGuiKey_O))
        {
            std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
            m_navigation.Reset();
            m_screen = Screen::QuickOpen;
        }
        else if (IsKeyPressed(ImGuiKey_S))
        {
            BeginSearchOverlay(true);
        }
        else if (IsKeyPressed(ImGuiKey_R))
        {
            std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
            m_navigation.Reset();
            m_screen = Screen::Recent;
        }
        else if (IsKeyPressed(ImGuiKey_F))
        {
            std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
            m_folderPickerActive = false;
            m_folderPickerAction = FolderPickerAction::None;
            m_navigation.Reset();
            m_screen = Screen::FileTree;
        }
        else if (IsKeyPressed(ImGuiKey_L))
        {
            std::fill(m_filterBuffer.begin(), m_filterBuffer.end(), '\0');
            m_folderPickerActive = false;
            m_folderPickerAction = FolderPickerAction::None;
            m_navigation.Reset();
            m_screen = Screen::Library;
        }
        else if (IsKeyPressed(ImGuiKey_W))
        {
            m_workspaceNavigation.SetCount(m_workspaceRegistry.Vaults().size());
            m_workspaceNavigation.Reset();
            m_screen = Screen::WorkspaceSwitcher;
        }
    }

    void SlateAppMode::HandleLibraryKeys()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.KeyShift && IsKeyPressed(ImGuiKey_Slash))
        {
            m_filterActive = true;
            m_focusFilter = true;
        }

        if (m_filterActive)
        {
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                m_filterActive = false;
                return;
            }
        }
        else if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_screen = Screen::Home;
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
        {
            m_navigation.MoveNext();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
        {
            m_navigation.MovePrevious();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        {
            ToggleSelectedFolder(true);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        {
            ToggleSelectedFolder(false);
        }
        if (IsKeyPressed(ImGuiKey_Enter) || IsKeyPressed(ImGuiKey_KeypadEnter))
        {
            ActivateSelectedTreeRow();
        }
    }

    void SlateAppMode::HandleEditorKeys(Software::Core::Runtime::AppContext& context)
    {
        if (IsCtrlPressed(ImGuiKey_F) && m_documents.HasOpenDocument())
        {
            BeginDocumentFindOverlay(true);
            return;
        }

        if (IsCtrlPressed(ImGuiKey_V) && m_assets.HasClipboardImage() && m_documents.HasOpenDocument())
        {
            auto* document = m_documents.Active();
            Software::Slate::fs::path assetRelative;
            std::string error;
            if (m_assets.SaveClipboardImageAsset(document->relativePath, &assetRelative, &error))
            {
                InsertTextAtEditorCursor(Software::Slate::AssetService::MarkdownImageLink(document->relativePath,
                                                                                         assetRelative) +
                                             document->lineEnding,
                                         context.frame.elapsedSeconds);
                SetStatus("pasted image");
            }
            else
            {
                SetError(error);
            }
        }

        if (m_editorTextFocused)
        {
            if (IsKeyPressed(ImGuiKey_Escape))
            {
                m_editorTextFocused = false;
            }
            return;
        }

        if (IsKeyPressed(ImGuiKey_O))
        {
            m_navigation.Reset();
            m_screen = Screen::Outline;
        }
        if (IsKeyPressed(ImGuiKey_Escape))
        {
            m_screen = Screen::Home;
        }
    }

    bool SlateAppMode::IsKeyPressed(ImGuiKey key) const
    {
        return ImGui::IsKeyPressed(key, false);
    }

    bool SlateAppMode::IsCtrlPressed(ImGuiKey key) const
    {
        const ImGuiIO& io = ImGui::GetIO();
        return io.KeyCtrl && ImGui::IsKeyPressed(key, false);
    }

    void SlateAppMode::SetStatus(std::string message)
    {
        m_status = std::move(message);
        m_statusSeconds = m_nowSeconds;
        m_statusIsError = false;
    }

    void SlateAppMode::SetError(std::string message)
    {
        m_status = std::move(message);
        m_statusSeconds = m_nowSeconds;
        m_statusIsError = true;
    }
}
