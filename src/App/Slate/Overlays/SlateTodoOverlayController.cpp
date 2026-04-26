#include "App/Slate/Overlays/SlateTodoOverlayController.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Editor/SlashCommandService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/UI/SlateUi.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace Software::Slate
{
    using namespace Software::Slate::UI;

    namespace
    {
        constexpr std::array<const char*, 5> TodoFilterLabels{{"all", "open", "research", "doing", "done"}};

        bool IsKeyPressed(ImGuiKey key)
        {
            return ImGui::IsKeyPressed(key, false);
        }

        SlateWorkspaceContext* WorkspaceContext(Software::Core::Runtime::AppContext& context)
        {
            return context.services.Resolve<SlateWorkspaceContext>().get();
        }

        SlateEditorContext* EditorContext(Software::Core::Runtime::AppContext& context)
        {
            return context.services.Resolve<SlateEditorContext>().get();
        }

        const char* TodoFilterLabel(int filter)
        {
            const int index = std::clamp(filter, 0, static_cast<int>(TodoFilterLabels.size() - 1));
            return TodoFilterLabels[static_cast<std::size_t>(index)];
        }

        const ImVec4& TodoFilterColor(int filter)
        {
            switch (std::clamp(filter, 0, 4))
            {
            case 1:
                return TodoStateColor(TodoState::Open);
            case 2:
                return TodoStateColor(TodoState::Research);
            case 3:
                return TodoStateColor(TodoState::Doing);
            case 4:
                return TodoStateColor(TodoState::Done);
            default:
                return Primary;
            }
        }


        const ImVec4& TodoPriorityColor(TodoPriority priority)
        {
            switch (priority)
            {
            case TodoPriority::Low:
                return Muted;
            case TodoPriority::Normal:
                return Cyan;
            case TodoPriority::High:
                return Amber;
            case TodoPriority::Urgent:
                return Red;
            }
            return Cyan;
        }

        bool MatchesTodoFilter(const TodoTicket& todo, int filter)
        {
            switch (std::clamp(filter, 0, 4))
            {
            case 1:
                return todo.state == TodoState::Open;
            case 2:
                return todo.state == TodoState::Research;
            case 3:
                return todo.state == TodoState::Doing;
            case 4:
                return todo.state == TodoState::Done;
            default:
                return true;
            }
        }

        void SetStatus(const TodoOverlayCallbacks& callbacks, std::string message)
        {
            if (callbacks.setStatus)
            {
                callbacks.setStatus(std::move(message));
            }
        }

        void SetError(const TodoOverlayCallbacks& callbacks, std::string message)
        {
            if (callbacks.setError)
            {
                callbacks.setError(std::move(message));
            }
        }
    }

    void SlateTodoOverlayController::OpenList(Software::Core::Runtime::AppContext& context)
    {
        if (auto* workspace = WorkspaceContext(context); workspace && workspace->HasWorkspaceLoaded())
        {
            if (auto* editor = EditorContext(context))
            {
                editor->CommitToActiveDocument(workspace->Documents(), context.frame.elapsedSeconds);
                editor->ReleaseNativeEditorFocus();
            }
            m_listOpen = true;
            m_navigation.Reset();
        }
    }

    void SlateTodoOverlayController::BeginCreate(Software::Core::Runtime::AppContext& context, bool fromSlashCommand)
    {
        auto* workspace = WorkspaceContext(context);
        auto* editor = EditorContext(context);
        if (!workspace || !workspace->HasWorkspaceLoaded() || !editor)
        {
            return;
        }

        auto& documents = workspace->Documents();
        editor->CommitToActiveDocument(documents, context.frame.elapsedSeconds);

        std::string activeLine;
        const bool hasActiveLine = editor->ActiveLineText(&activeLine);
        const auto* active = documents.Active();
        const std::size_t activeLineNumber =
            editor->NativeEditorAvailable()
                ? static_cast<std::size_t>(std::max(1, editor->NativeEditorScrollState().caretLine + 1))
                : (editor->Editor().ActiveLine() + 1);
        const bool launchedFromTodoCommand = fromSlashCommand ||
                                             (hasActiveLine && IsSlashCommand(activeLine));

        m_form = {};
        m_form.mode = TodoFormMode::Create;
        m_form.requestTextFocus = true;
        m_form.focusField = TodoFormFocusField::Title;
        m_form.state = TodoState::Open;
        m_form.priority = TodoPriority::Normal;
        m_form.replaceActiveLine = launchedFromTodoCommand;
        if (m_form.replaceActiveLine && active)
        {
            m_form.pendingCommandPath = PathUtils::NormalizeRelative(active->relativePath);
            m_form.pendingCommandLine = activeLineNumber;
        }

        if (launchedFromTodoCommand)
        {
            RemovePendingCommand(context);
        }

        editor->ReleaseNativeEditorFocus();
        m_form.title = "Untitled todo";
        m_form.description = "Description";
        m_form.open = true;
    }

    void SlateTodoOverlayController::Reset()
    {
        m_listOpen = false;
        m_stateFilter = 0;
        m_visibleTodos.clear();
        m_navigation.Reset();
        m_form = {};
    }

    bool SlateTodoOverlayController::IsListOpen() const
    {
        return m_listOpen;
    }

    bool SlateTodoOverlayController::IsFormOpen() const
    {
        return m_form.open;
    }

    bool SlateTodoOverlayController::IsAnyOpen() const
    {
        return m_listOpen || m_form.open;
    }

    std::string SlateTodoOverlayController::HelperText(const ShortcutService& shortcuts) const
    {
        if (m_form.open)
        {
            return "(" + shortcuts.Label(ShortcutAction::NavigateUp) + "/" +
                   shortcuts.Label(ShortcutAction::NavigateDown) + ") field   " +
                   shortcuts.Helper(ShortcutAction::TodoState, "state") + "   " +
                   shortcuts.Helper(ShortcutAction::TodoPriority, "priority") + "   " +
                   shortcuts.Helper(ShortcutAction::EditorSave, "save") + "   " +
                   shortcuts.Helper(ShortcutAction::Cancel, "cancel");
        }
        if (m_listOpen)
        {
            return "(" + shortcuts.Label(ShortcutAction::NavigateUp) + "/" +
                   shortcuts.Label(ShortcutAction::NavigateDown) + ") move   " +
                   shortcuts.Helper(ShortcutAction::TodoOpen, "open") + "   " +
                   shortcuts.Helper(ShortcutAction::TodoEdit, "edit") + "   " +
                   shortcuts.Helper(ShortcutAction::TodoDelete, "delete") + "   " +
                   shortcuts.Helper(ShortcutAction::Cancel, "close");
        }
        return {};
    }

    bool SlateTodoOverlayController::IsSlashCommand(std::string_view text)
    {
        return PathUtils::ToLower(PathUtils::Trim(text)) == "/todo";
    }

    void SlateTodoOverlayController::Draw(Software::Core::Runtime::AppContext& context,
                                          const TodoOverlayCallbacks& callbacks)
    {
        if (m_listOpen)
        {
            DrawList(context, callbacks);
        }
        if (m_form.open)
        {
            if (auto* editor = EditorContext(context))
            {
                editor->ReleaseNativeEditorFocus();
            }
            DrawForm(context, callbacks);
        }
    }

    void SlateTodoOverlayController::BeginEdit(const TodoTicket& ticket)
    {
        m_form = {};
        m_form.open = true;
        m_form.requestTextFocus = true;
        m_form.focusField = TodoFormFocusField::Title;
        m_form.mode = TodoFormMode::Edit;
        m_form.ticket = ticket;
        m_form.state = ticket.state;
        m_form.priority = ticket.priority;
        m_form.title = ticket.title;
        m_form.description = ticket.description;
    }

    void SlateTodoOverlayController::DrawList(Software::Core::Runtime::AppContext& context,
                                              const TodoOverlayCallbacks& callbacks)
    {
        auto* workspace = WorkspaceContext(context);
        const auto* shortcuts = workspace ? &workspace->Shortcuts() : nullptr;
        const bool allowInput = !m_form.open && shortcuts;
        if (!workspace)
        {
            m_visibleTodos.clear();
        }
        else if (const auto todoService = context.services.Resolve<TodoService>())
        {
            m_visibleTodos = todoService->CollectTodos(*workspace);
        }
        else
        {
            m_visibleTodos.clear();
        }

        m_visibleTodos.erase(std::remove_if(m_visibleTodos.begin(),
                                            m_visibleTodos.end(),
                                            [this](const auto& todo) {
                                                return !MatchesTodoFilter(todo, m_stateFilter);
                                            }),
                             m_visibleTodos.end());
        std::sort(m_visibleTodos.begin(), m_visibleTodos.end(), [](const auto& left, const auto& right) {
            const int leftState = TodoService::StateSortKey(left.state);
            const int rightState = TodoService::StateSortKey(right.state);
            if (leftState != rightState)
            {
                return leftState < rightState;
            }
            const int leftPriority = TodoService::PrioritySortKey(left.priority);
            const int rightPriority = TodoService::PrioritySortKey(right.priority);
            if (leftPriority != rightPriority)
            {
                return leftPriority > rightPriority;
            }
            return left.title < right.title;
        });

        const std::size_t pinnedRows = 1;
        m_navigation.SetCount(m_visibleTodos.size() + pinnedRows);

        if (allowInput && shortcuts->Pressed(ShortcutAction::NavigateDown, true))
        {
            m_navigation.MoveNext();
        }
        if (allowInput && shortcuts->Pressed(ShortcutAction::NavigateUp, true))
        {
            m_navigation.MovePrevious();
        }
        if (allowInput && shortcuts->Pressed(ShortcutAction::Cancel))
        {
            m_listOpen = false;
            if (callbacks.restoreEditorFocus)
            {
                callbacks.restoreEditorFocus();
            }
            return;
        }
        if (allowInput && shortcuts->Pressed(ShortcutAction::TodoOpen) &&
            m_navigation.HasSelection())
        {
            OpenSelected(context, callbacks);
            return;
        }
        if (allowInput && shortcuts->Pressed(ShortcutAction::TodoEdit) &&
            m_navigation.HasSelection())
        {
            if (m_navigation.Selected() == 0)
            {
                BeginCreate(context, false);
            }
            else if (!m_visibleTodos.empty())
            {
                BeginEdit(m_visibleTodos[m_navigation.Selected() - 1]);
            }
            return;
        }
        if (allowInput && shortcuts->Pressed(ShortcutAction::TodoDelete) &&
            m_navigation.HasSelection() && m_navigation.Selected() > 0 && !m_visibleTodos.empty())
        {
            DeleteSelected(context, callbacks);
            return;
        }

        const ImVec2 size(std::min(760.0f, std::max(420.0f, ImGui::GetWindowWidth() * 0.60f)),
                          std::min(500.0f, std::max(280.0f, ImGui::GetWindowHeight() * 0.54f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Amber.x, Amber.y, Amber.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("TodoOverlay", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        const bool overlayHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::TextColored(Amber, "todos");
        ImGui::SameLine();
        ImGui::TextColored(TodoFilterColor(m_stateFilter), "%s", TodoFilterLabel(m_stateFilter));
        ImGui::Dummy(ImVec2(1.0f, 8.0f));
        ImGui::Separator();

        auto* drawList = ImGui::GetWindowDrawList();
        const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + 6.0f;
        const float rowRadius = 6.0f;
        const float rowInsetX = 4.0f;
        const ImU32 selectedRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Green.x, Green.y, Green.z, 0.14f));
        const ImU32 hoveredRowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(Cyan.x, Cyan.y, Cyan.z, 0.08f));

        auto drawSelectionBackground = [&](const ImVec2& rowStart, float rowWidth, bool selected, bool hovered)
        {
            if (selected || hovered)
            {
                drawList->AddRectFilled(ImVec2(rowStart.x + rowInsetX, rowStart.y + 1.0f),
                                        ImVec2(rowStart.x + rowWidth - rowInsetX, rowStart.y + rowHeight - 1.0f),
                                        selected ? selectedRowColor : hoveredRowColor,
                                        rowRadius);
            }
        };

        {
            const bool selected = m_navigation.Selected() == 0;
            ImVec2 rowStart = ImGui::GetCursorScreenPos();
            const float rowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            ImGui::PushID("TodoNewPinnedRow");
            ImGui::InvisibleButton("##TodoNewRow", ImVec2(rowWidth, rowHeight));
            const bool hovered = ImGui::IsItemHovered();
            if (allowInput && ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                m_navigation.SetSelected(0);
            }
            if (allowInput && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_navigation.SetSelected(0);
                BeginCreate(context, false);
                ImGui::PopID();
                return;
            }
            ImGui::PopID();

            drawSelectionBackground(rowStart, rowWidth, selected, hovered);
            const float y = rowStart.y + 4.0f;
            drawList->AddText(ImVec2(rowStart.x + 18.0f, y),
                              ImGui::ColorConvertFloat4ToU32(selected ? Green : Amber),
                              "+ new todo");
        }

        if (m_visibleTodos.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, 6.0f));
            ImGui::TextColored(Muted, "no todo files yet");
        }
        else
        {
            bool groupStarted = false;
            TodoState currentGroup = TodoState::Open;
            for (std::size_t i = 0; i < m_visibleTodos.size(); ++i)
            {
                const auto& todo = m_visibleTodos[i];
                const ImVec4& stateColor = TodoStateColor(todo.state);
                const ImVec4& priorityColor = TodoPriorityColor(todo.priority);
                if (m_stateFilter == 0 && (!groupStarted || todo.state != currentGroup))
                {
                    if (groupStarted)
                    {
                        ImGui::Dummy(ImVec2(1.0f, 6.0f));
                    }
                    currentGroup = todo.state;
                    groupStarted = true;
                    ImGui::TextColored(stateColor, "%s", MarkdownService::TodoStateLabel(todo.state));
                    ImGui::Separator();
                }

                const std::size_t rowIndex = i + 1;
                const bool selected = rowIndex == m_navigation.Selected();
                ImVec2 rowStart = ImGui::GetCursorScreenPos();
                const float rowWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                ImGui::PushID(static_cast<int>(rowIndex));
                ImGui::InvisibleButton("##TodoRow", ImVec2(rowWidth, rowHeight));
                const bool hovered = ImGui::IsItemHovered();
                if (allowInput && ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    m_navigation.SetSelected(rowIndex);
                }
                if (allowInput && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_navigation.SetSelected(rowIndex);
                    OpenSelected(context, callbacks);
                    ImGui::PopID();
                    return;
                }
                ImGui::PopID();

                drawSelectionBackground(rowStart, rowWidth, selected, hovered);

                const float titleX = rowStart.x + 18.0f;
                const float metaX = rowStart.x + rowWidth - 170.0f;
                const float y = rowStart.y + 4.0f;
                drawList->AddText(ImVec2(titleX, y), ImGui::ColorConvertFloat4ToU32(selected ? Green : Primary), todo.title.c_str());
                drawList->AddText(ImVec2(metaX, y), ImGui::ColorConvertFloat4ToU32(stateColor), MarkdownService::TodoStateLabel(todo.state));
                drawList->AddText(ImVec2(metaX + 88.0f, y), ImGui::ColorConvertFloat4ToU32(priorityColor), TodoService::PriorityLabel(todo.priority));
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (allowInput && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !overlayHovered)
        {
            m_listOpen = false;
            if (callbacks.restoreEditorFocus)
            {
                callbacks.restoreEditorFocus();
            }
            return;
        }
    }

    void SlateTodoOverlayController::DrawForm(Software::Core::Runtime::AppContext& context,
                                              const TodoOverlayCallbacks& callbacks)
    {
        auto* workspace = WorkspaceContext(context);
        const auto* shortcuts = workspace ? &workspace->Shortcuts() : nullptr;
        if (!shortcuts)
        {
            return;
        }

        const std::string helperText = HelperText(*shortcuts);
        const float helperWidth = ImGui::CalcTextSize(helperText.c_str()).x;
        const float maxWidth = std::max(360.0f, ImGui::GetWindowWidth() - 96.0f);
        const float desiredWidth = std::max({620.0f, helperWidth + 52.0f, ImGui::GetWindowWidth() * 0.52f});
        const float formWidth = std::min(maxWidth, desiredWidth);
        const float desiredHeight = 292.0f;
        const float formHeight = std::min(std::max(236.0f, desiredHeight), std::max(220.0f, ImGui::GetWindowHeight() - 96.0f));
        const ImVec2 size(formWidth, formHeight);
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   (ImGui::GetWindowHeight() - size.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, Panel);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(Amber.x, Amber.y, Amber.z, 0.30f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::BeginChild("TodoFormOverlay", size, true);
        if (m_form.requestTextFocus)
        {
            ImGui::SetWindowFocus();
        }
        const bool formHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::TextColored(Amber, "%s", m_form.mode == TodoFormMode::Create ? "new todo" : "edit todo");
        ImGui::SameLine();
        ImGui::TextColored(TodoStateColor(m_form.state),
                           "[%s]",
                           MarkdownService::TodoStateLabel(m_form.state));
        ImGui::SameLine();
        ImGui::TextColored(TodoPriorityColor(m_form.priority),
                           "[%s]",
                           TodoService::PriorityLabel(m_form.priority));
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);

        const bool upPressed = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);
        const bool downPressed = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);

        // Move focus before drawing the title so description -> title happens immediately.
        if (m_form.focusField == TodoFormFocusField::Description && upPressed)
        {
            m_form.focusField = TodoFormFocusField::Title;
            m_form.requestTextFocus = true;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (m_form.requestTextFocus && m_form.focusField == TodoFormFocusField::Title)
        {
            ImGui::SetKeyboardFocusHere();
        }
        const auto titleResult =
            InputTextString("##TodoTitle", m_form.title, ImGuiInputTextFlags_EnterReturnsTrue);
        const bool titleActive = ImGui::IsItemActive();
        const bool titleFocused = ImGui::IsItemFocused();
        if (titleActive || titleFocused || ImGui::IsItemClicked())
        {
            m_form.focusField = TodoFormFocusField::Title;
        }
        if (m_form.requestTextFocus && m_form.focusField == TodoFormFocusField::Title &&
            (titleActive || titleFocused))
        {
            m_form.requestTextFocus = false;
        }

        // Move focus before drawing the description so title -> description happens in the same frame.
        if ((titleActive || titleFocused || m_form.focusField == TodoFormFocusField::Title) && downPressed)
        {
            m_form.focusField = TodoFormFocusField::Description;
            m_form.requestTextFocus = true;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (m_form.requestTextFocus && m_form.focusField == TodoFormFocusField::Description)
        {
            ImGui::SetKeyboardFocusHere();
        }
        const float descriptionHeight = std::max(86.0f, size.y - 154.0f);
        auto descriptionResult =
            InputTextMultilineString("##TodoDescription",
                                     m_form.description,
                                     ImVec2(-1.0f, descriptionHeight),
                                     0,
                                     &m_form.descriptionCursor,
                                     m_form.requestedDescriptionCursor);
        m_form.requestedDescriptionCursor = -1;
        const bool descriptionActive = ImGui::IsItemActive();
        const bool descriptionFocused = ImGui::IsItemFocused();
        if (descriptionActive || descriptionFocused || ImGui::IsItemClicked())
        {
            m_form.focusField = TodoFormFocusField::Description;
        }
        if (m_form.requestTextFocus && m_form.focusField == TodoFormFocusField::Description &&
            (descriptionActive || descriptionFocused))
        {
            m_form.requestTextFocus = false;
        }
        const bool textInputActive = titleActive || descriptionActive || titleFocused || descriptionFocused;
        ImGui::PopItemFlag();
        ImGui::Separator();
        DrawShortcutText(helperText, Amber, Primary);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        if (!textInputActive && !ImGui::IsAnyItemActive())
        {
            m_form.requestTextFocus = true;
        }

        if (shortcuts->Pressed(ShortcutAction::TodoState))
        {
            m_form.state = MarkdownService::NextTodoState(m_form.state);
        }
        if (shortcuts->Pressed(ShortcutAction::TodoPriority))
        {
            m_form.priority = TodoService::NextPriority(m_form.priority);
        }
        if (shortcuts->Pressed(ShortcutAction::Cancel))
        {
            CancelForm(context, callbacks);
            return;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !formHovered)
        {
            CancelForm(context, callbacks);
            return;
        }

        if (titleResult.submitted)
        {
            m_form.focusField = TodoFormFocusField::Description;
            m_form.requestTextFocus = true;
            return;
        }

        if (shortcuts->Pressed(ShortcutAction::EditorSave))
        {
            AcceptForm(context, callbacks);
            return;
        }
    }

    void SlateTodoOverlayController::CancelForm(Software::Core::Runtime::AppContext& context,
                                                const TodoOverlayCallbacks& callbacks)
    {
        if (m_form.mode == TodoFormMode::Create && m_form.replaceActiveLine)
        {
            RemovePendingCommand(context);
        }

        m_form = {};
        if (!m_listOpen && callbacks.restoreEditorFocus)
        {
            callbacks.restoreEditorFocus();
        }
    }

    bool SlateTodoOverlayController::RemovePendingCommand(Software::Core::Runtime::AppContext& context)
    {
        auto* workspace = WorkspaceContext(context);
        auto* editor = EditorContext(context);
        if (!workspace || !editor)
        {
            return false;
        }

        auto& documents = workspace->Documents();
        auto* active = documents.Active();
        if (!active || m_form.pendingCommandLine == 0)
        {
            return false;
        }

        if (!m_form.pendingCommandPath.empty() &&
            PathUtils::NormalizeRelative(active->relativePath) != m_form.pendingCommandPath)
        {
            return false;
        }

        const std::string lineEnding = active->lineEnding.empty()
                                           ? PathUtils::DetectLineEnding(active->text)
                                           : active->lineEnding;
        auto lines = MarkdownService::SplitLines(active->text);
        if (lines.empty())
        {
            return false;
        }
        const std::size_t removeIndex = std::min(m_form.pendingCommandLine - 1, lines.size() - 1);
        std::string query;
        std::size_t tokenStart = 0;
        std::size_t tokenEnd = 0;
        const bool hasTodoToken =
            SlashCommandService::QueryFromLine(lines[removeIndex], &query, &tokenStart, &tokenEnd) && query == "todo";
        const bool wholeLineTodoCommand = IsSlashCommand(lines[removeIndex]);
        if (!hasTodoToken && !wholeLineTodoCommand)
        {
            return false;
        }

        std::string remainingLine;
        if (hasTodoToken)
        {
            remainingLine = lines[removeIndex].substr(0, tokenStart) + lines[removeIndex].substr(tokenEnd);
        }

        if (PathUtils::Trim(remainingLine).empty())
        {
            if (lines.size() == 1)
            {
                lines.front().clear();
            }
            else
            {
                lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(removeIndex));
            }
        }
        else
        {
            lines[removeIndex] = std::move(remainingLine);
        }

        std::string updatedText;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                updatedText += lineEnding;
            }
            updatedText += lines[i];
        }

        active->text = std::move(updatedText);
        documents.MarkEdited(context.frame.elapsedSeconds);
        editor->LoadFromActiveDocument(documents);
        editor->ReleaseNativeEditorFocus();
        editor->SetNativeEditorVisible(false);
        editor->JumpToLine(std::min(removeIndex + 1, lines.size()));
        return true;
    }

    bool SlateTodoOverlayController::AcceptForm(Software::Core::Runtime::AppContext& context,
                                                const TodoOverlayCallbacks& callbacks)
    {
        auto* workspace = WorkspaceContext(context);
        auto* editor = EditorContext(context);
        if (!workspace || !editor)
        {
            return false;
        }

        const auto state = m_form.state;
        const std::string title = PathUtils::Trim(m_form.title).empty()
                                      ? "Untitled todo"
                                      : PathUtils::Trim(m_form.title);
        const std::string description = PathUtils::Trim(m_form.description);

        if (m_form.mode == TodoFormMode::Create)
        {
            const auto todoService = context.services.Resolve<TodoService>();
            if (!todoService)
            {
                SetError(callbacks, "todo service unavailable");
                return false;
            }

            TodoMutationResult result;
            if (!todoService->CreateTodo(*workspace,
                                         state,
                                         title,
                                         description,
                                         context.frame.elapsedSeconds,
                                         &result,
                                         m_form.priority))
            {
                SetError(callbacks, result.error.empty() ? "could not create todo file" : result.error);
                return false;
            }

            if (callbacks.openDocument)
            {
                callbacks.openDocument(result.relativePath);
            }
            if (auto* activeEditor = EditorContext(context))
            {
                activeEditor->JumpToLine(result.revealLine);
            }
            SetStatus(callbacks, "todo file created");
        }
        else
        {
            auto* workspace = WorkspaceContext(context);
            const auto todoService = context.services.Resolve<TodoService>();
            if (!workspace || !todoService)
            {
                SetError(callbacks, "todo service unavailable");
                return false;
            }

            TodoMutationResult result;
            if (!todoService->UpdateTodo(*workspace,
                                          m_form.ticket,
                                          state,
                                          title,
                                          description,
                                          context.frame.elapsedSeconds,
                                          &result,
                                          m_form.priority))
            {
                SetError(callbacks, result.error.empty() ? "could not update todo" : result.error);
                return false;
            }
            RefreshEditorAfterMutation(context, result);
            SetStatus(callbacks, "todo updated");
        }

        m_form = {};
        if (!m_listOpen && callbacks.restoreEditorFocus)
        {
            callbacks.restoreEditorFocus();
        }
        return true;
    }

    bool SlateTodoOverlayController::OpenSelected(Software::Core::Runtime::AppContext& context,
                                                    const TodoOverlayCallbacks& callbacks)
    {
        if (!m_navigation.HasSelection())
        {
            return false;
        }

        if (m_navigation.Selected() == 0)
        {
            BeginCreate(context, false);
            return true;
        }

        const std::size_t todoIndex = m_navigation.Selected() - 1;
        if (todoIndex >= m_visibleTodos.size())
        {
            return false;
        }

        const auto todo = m_visibleTodos[todoIndex];
        m_listOpen = false;
        if (callbacks.openDocument && callbacks.openDocument(todo.relativePath))
        {
            if (auto* editor = EditorContext(context))
            {
                editor->JumpToLine(todo.line);
            }
            return true;
        }
        return false;
    }

    bool SlateTodoOverlayController::DeleteSelected(Software::Core::Runtime::AppContext& context,
                                                      const TodoOverlayCallbacks& callbacks)
    {
        if (!m_navigation.HasSelection() || m_navigation.Selected() == 0 || m_visibleTodos.empty())
        {
            return false;
        }

        auto* workspace = WorkspaceContext(context);
        const auto todoService = context.services.Resolve<TodoService>();
        if (!workspace || !todoService)
        {
            SetError(callbacks, "todo service unavailable");
            return false;
        }

        TodoMutationResult result;
        const std::size_t todoIndex = m_navigation.Selected() - 1;
        if (todoIndex >= m_visibleTodos.size())
        {
            return false;
        }
        const auto todo = m_visibleTodos[todoIndex];
        if (!todoService->DeleteTodo(*workspace, todo, context.frame.elapsedSeconds, &result))
        {
            SetError(callbacks, result.error.empty() ? "could not delete todo" : result.error);
            return false;
        }

        RefreshEditorAfterMutation(context, result);
        SetStatus(callbacks, "todo deleted");
        if (!m_visibleTodos.empty())
        {
            m_navigation.SetSelected(std::min(m_navigation.Selected(), m_visibleTodos.size()));
        }
        return true;
    }

    void SlateTodoOverlayController::RefreshEditorAfterMutation(Software::Core::Runtime::AppContext& context,
                                                                const TodoMutationResult& result)
    {
        if (!result.touchedActiveDocument)
        {
            return;
        }

        auto* workspace = WorkspaceContext(context);
        auto* editor = EditorContext(context);
        if (!workspace || !editor)
        {
            return;
        }

        editor->LoadFromActiveDocument(workspace->Documents());
        editor->JumpToLine(result.revealLine);
    }
}
