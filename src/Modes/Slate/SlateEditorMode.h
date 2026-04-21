#pragma once

#include "App/Slate/DocumentService.h"
#include "App/Slate/JournalService.h"
#include "Modes/Slate/SlateModeBase.h"

namespace Software::Modes::Slate
{
    class SlateEditorMode final : public SlateModeBase
    {
    private:
        const char* ModeName() const override;
        void DrawMode(Software::Core::Runtime::AppContext& context, bool handleInput) override;
        std::string ModeHelperText(const Software::Core::Runtime::AppContext& context) const override;

        void DrawEditor(Software::Core::Runtime::AppContext& context,
                        Software::Slate::DocumentService::Document& document);
        void DrawOutline(Software::Core::Runtime::AppContext& context);
        void DrawJournalCompanion(Software::Core::Runtime::AppContext& context,
                                  const Software::Slate::DocumentService::Document& document);
        void DrawJournalCalendar(const Software::Slate::JournalMonthSummary& summary);
        void DrawJournalActivityGraph(const Software::Slate::JournalMonthSummary& summary);
        void DrawLiveMarkdownEditor(Software::Core::Runtime::AppContext& context,
                                    Software::Slate::DocumentService::Document& document);
    };
}
