#pragma once

#include "App/Slate/SlateTypes.h"
#include "App/Slate/WorkspaceService.h"

#include <array>
#include <string>
#include <vector>

namespace Software::Slate
{
    struct JournalDaySummary
    {
        int day = 0;
        bool hasEntry = false;
        bool hasContent = false;
        int wordCount = 0;
    };

    struct JournalMonthSummary
    {
        int year = 0;
        int month = 0;
        int daysInMonth = 0;
        int firstWeekdayMondayBased = 0;
        int writtenDays = 0;
        int currentStreak = 0;
        int currentDay = 0;
        int activeDay = 0;
        int activeWordCount = 0;
        std::array<int, 7> recentDayNumbers{};
        std::array<int, 7> recentWordCounts{};
        std::string prompt;
        std::vector<JournalDaySummary> days;
    };

    class JournalService
    {
    public:
        bool IsJournalPath(const fs::path& relativePath) const;
        bool ParseJournalDate(const fs::path& relativePath, int* year, int* month, int* day) const;
        bool HasMeaningfulContent(const std::string& text) const;
        int CountWords(const std::string& text) const;
        std::string PromptForDate(int year, int month, int day) const;

        JournalMonthSummary BuildCurrentMonthSummary(const WorkspaceService& workspace,
                                                     const fs::path& activeRelativePath = {},
                                                     const std::string* activeText = nullptr) const;
        JournalMonthSummary BuildMonthSummary(const WorkspaceService& workspace, int year, int month,
                                              const fs::path& activeRelativePath = {},
                                              const std::string* activeText = nullptr) const;
    };
}
