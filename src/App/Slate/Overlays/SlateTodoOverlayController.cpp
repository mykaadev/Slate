#include "App/Slate/Overlays/SlateTodoOverlayController.h"

#include "App/Slate/Core/PathUtils.h"
#include "App/Slate/Documents/DocumentService.h"
#include "App/Slate/Editor/SlateEditorContext.h"
#include "App/Slate/Editor/SlashCommandService.h"
#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/State/SlateWorkspaceContext.h"
#include "App/Slate/Todos/TodoService.h"
#include "App/Slate/UI/SlateDesign.h"
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
                editor->ReleaseFocus();
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
        std::size_t activeLineNumber = 1;
        editor->CaretPosition(&activeLineNumber, nullptr);
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

        editor->ReleaseFocus();
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
                   shortcuts.Label(ShortcutAction::NavigateDown) + ") move   " +
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
                editor->ReleaseFocus();
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
                          std::min(540.0f, std::max(300.0f, ImGui::GetWindowHeight() * 0.58f)));
        ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - size.x) * 0.5f,
                                   std::max(40.0f, (ImGui::GetWindowHeight() - Tokens::FooterHeight - size.y) * 0.42f)));
        BeginCard("TodoOverlay", size, Amber, Tokens::SpaceL);
        const bool overlayHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        // Title on the border, the filters as pills; the active filter is solid
        const std::string todoCount = std::to_string(m_visibleTodos.size()) + (m_visibleTodos.size() == 1 ? " todo" : " todos");
        CardTitle("todos", Amber, todoCount.c_str());
        {
            float pillX = ImGui::GetWindowContentRegionMax().x;
            for (const char* label : TodoFilterLabels)
            {
                pillX -= PillWidth(label) + 6.0f;
            }
            ImGui::TextColored(TextFaint(), "filter");
            ImGui::SameLine(pillX);
            for (int filter = 0; filter < static_cast<int>(TodoFilterLabels.size()); ++filter)
            {
                Pill(TodoFilterLabels[static_cast<std::size_t>(filter)], TodoFilterColor(filter), filter == m_stateFilter);
                ImGui::SameLine(0.0f, 6.0f);
            }
            ImGui::NewLine();
        }
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXS));

        ImGui::BeginChild("TodoRows", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
        auto* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeight();
        const float rowHeight = lineHeight + 14.0f;
        bool createRequested = false;

        {
            const bool selected = m_navigation.Selected() == 0;
            ImVec2 rowMin;
            ImVec2 rowMax;
            ImGui::PushID("TodoNewPinnedRow");
            const bool clicked = ListRow("##TodoNewRow", selected, rowHeight, Amber, &rowMin, &rowMax);
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            if (allowInput && clicked)
            {
                m_navigation.SetSelected(0);
            }
            if (allowInput && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_navigation.SetSelected(0);
                createRequested = true;
            }
            const float y = rowMin.y + (rowHeight - lineHeight) * 0.5f;
            drawList->AddText(ImVec2(rowMin.x + 14.0f, y), U32(Amber), "+");
            drawList->AddText(ImVec2(rowMin.x + 32.0f, y), U32(selected ? Primary : TextSecondary()), "new todo");
        }

        if (m_visibleTodos.empty())
        {
            ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceM));
            ImGui::TextColored(TextFaint(), "%s", m_stateFilter == 0 ? "no todos yet" : "nothing here");
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
                    currentGroup = todo.state;
                    groupStarted = true;
                    ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceS));
                    SectionLabel(MarkdownService::TodoStateLabel(todo.state), stateColor);
                }

                const std::size_t rowIndex = i + 1;
                const bool selected = rowIndex == m_navigation.Selected();
                ImVec2 rowMin;
                ImVec2 rowMax;
                ImGui::PushID(static_cast<int>(rowIndex));
                const bool clicked = ListRow("##TodoRow", selected, rowHeight, stateColor, &rowMin, &rowMax);
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();
                if (allowInput && clicked)
                {
                    m_navigation.SetSelected(rowIndex);
                }
                if (allowInput && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    m_navigation.SetSelected(rowIndex);
                    m_pendingOpenAfterListDraw = true;
                }

                // State dot, title, then state and priority pills on the right
                const float y = rowMin.y + (rowHeight - lineHeight) * 0.5f;
                const bool done = todo.state == TodoState::Done;
                drawList->AddCircleFilled(ImVec2(rowMin.x + 18.0f, rowMin.y + rowHeight * 0.5f), 3.5f, U32(stateColor));
                const char* priorityLabel = TodoService::PriorityLabel(todo.priority);
                const char* stateLabel = MarkdownService::TodoStateLabel(todo.state);
                const float priorityX = rowMax.x - PillWidth(priorityLabel) - 10.0f;
                const float stateX = priorityX - PillWidth(stateLabel) - 6.0f;
                const float pillY = rowMin.y + (rowHeight - PillHeight()) * 0.5f;
                drawList->PushClipRect(rowMin, ImVec2(stateX - 8.0f, rowMax.y), true);
                const ImVec4 titleColor = done ? TextFaint() : (selected ? Primary : TextSecondary());
                drawList->AddText(ImVec2(rowMin.x + 32.0f, y), U32(titleColor), todo.title.c_str());
                if (done)
                {
                    const float width = ImGui::CalcTextSize(todo.title.c_str()).x;
                    drawList->AddLine(ImVec2(rowMin.x + 32.0f, y + lineHeight * 0.55f),
                                      ImVec2(rowMin.x + 32.0f + width, y + lineHeight * 0.55f), U32(TextFaint()));
                }
                drawList->PopClipRect();
                if (m_stateFilter != 0)
                {
                    DrawPill(drawList, ImVec2(stateX, pillY), stateLabel, stateColor);
                }
                DrawPill(drawList, ImVec2(priorityX, pillY), priorityLabel, priorityColor);
            }
        }

        ImGui::EndChild();
        EndCard();

        if (createRequested)
        {
            BeginCreate(context, false);
            return;
        }

        if (m_pendingOpenAfterListDraw)
        {
            m_pendingOpenAfterListDraw = false;
            OpenSelected(context, callbacks);
            return;
        }

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
        BeginCard("TodoFormOverlay", size, Amber, Tokens::SpaceL);
        if (m_form.requestTextFocus)
        {
            ImGui::SetWindowFocus();
        }
        const bool formHovered =
            ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        SectionLabel(m_form.mode == TodoFormMode::Create ? "new todo" : "edit todo", Amber);
        {
            const char* stateLabel = MarkdownService::TodoStateLabel(m_form.state);
            const char* priorityLabel = TodoService::PriorityLabel(m_form.priority);
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - PillWidth(stateLabel) - PillWidth(priorityLabel) - 6.0f);
            Pill(stateLabel, TodoStateColor(m_form.state), true);
            ImGui::SameLine(0.0f, 6.0f);
            Pill(priorityLabel, TodoPriorityColor(m_form.priority));
        }
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXS));
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
        ImGui::Dummy(ImVec2(1.0f, Tokens::SpaceXS));
        DrawShortcutText(helperText, Amber, TextSecondary());
        EndCard();

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
        editor->ReleaseFocus();
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
