#include "App/Slate/Markdown/JournalService.h"

#include "App/Slate/Markdown/MarkdownService.h"
#include "App/Slate/Core/PathUtils.h"

#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Software::Slate
{
    namespace
    {
        struct LocalDate
        {
            int year = 0;
            int month = 0;
            int day = 0;
        };

        LocalDate CurrentLocalDate()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm localTime{};
#if defined(_WIN32)
            localtime_s(&localTime, &time);
#else
            localtime_r(&time, &localTime);
#endif

            return {localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday};
        }

        bool IsLeapYear(int year)
        {
            return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        }

        int DaysInMonth(int year, int month)
        {
            static constexpr std::array<int, 12> DaysPerMonth{{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
            if (month < 1 || month > 12)
            {
                return 30;
            }
            if (month == 2 && IsLeapYear(year))
            {
                return 29;
            }
            return DaysPerMonth[static_cast<std::size_t>(month - 1)];
        }

        int FirstWeekdayMondayBased(int year, int month)
        {
            std::tm timeInfo{};
            timeInfo.tm_year = year - 1900;
            timeInfo.tm_mon = month - 1;
            timeInfo.tm_mday = 1;
            std::mktime(&timeInfo);
            return (timeInfo.tm_wday + 6) % 7;
        }

        bool IsPlaceholderLine(const std::string& trimmed)
        {
            return trimmed.empty() || trimmed == "-" || trimmed == "*" || trimmed == "+" || trimmed == "- [ ]" ||
                   trimmed == "* [ ]" || trimmed == "+ [ ]";
        }

        std::string ReadTextFile(const fs::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return {};
            }

            std::ostringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }
    }

    bool JournalService::IsJournalPath(const fs::path& relativePath) const
    {
        const std::string normalized = PathUtils::ToLower(PathUtils::NormalizeRelative(relativePath).generic_string());
        return normalized == "journal" || normalized.rfind("journal/", 0) == 0;
    }

    bool JournalService::ParseJournalDate(const fs::path& relativePath, int* year, int* month, int* day) const
    {
        const fs::path normalized = PathUtils::NormalizeRelative(relativePath);
        std::vector<std::string> parts;
        for (const auto& part : normalized)
        {
            parts.push_back(part.string());
        }

        if (parts.size() != 4 || PathUtils::ToLower(parts[0]) != "journal" || !PathUtils::IsMarkdownFile(normalized))
        {
            return false;
        }

        int parsedYear = 0;
        int parsedMonth = 0;
        int parsedDay = 0;
        try
        {
            parsedYear = std::stoi(parts[1]);
            parsedMonth = std::stoi(parts[2]);

            const std::string stem = normalized.stem().string();
            if (stem.size() != 10 || stem[4] != '-' || stem[7] != '-')
            {
                return false;
            }
            if (std::stoi(stem.substr(0, 4)) != parsedYear || std::stoi(stem.substr(5, 2)) != parsedMonth)
            {
                return false;
            }
            parsedDay = std::stoi(stem.substr(8, 2));
        }
        catch (...)
        {
            return false;
        }

        if (parsedMonth < 1 || parsedMonth > 12 || parsedDay < 1 || parsedDay > DaysInMonth(parsedYear, parsedMonth))
        {
            return false;
        }

        if (year)
        {
            *year = parsedYear;
        }
        if (month)
        {
            *month = parsedMonth;
        }
        if (day)
        {
            *day = parsedDay;
        }
        return true;
    }

    bool JournalService::HasMeaningfulContent(const std::string& text) const
    {
        const auto lines = MarkdownService::SplitLines(text);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string trimmed = PathUtils::Trim(lines[i]);
            if (i == 0 && trimmed.rfind("# ", 0) == 0)
            {
                continue;
            }
            if (IsPlaceholderLine(trimmed))
            {
                continue;
            }
            return true;
        }
        return false;
    }

    int JournalService::CountWords(const std::string& text) const
    {
        int words = 0;
        const auto lines = MarkdownService::SplitLines(text);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const std::string trimmed = PathUtils::Trim(lines[i]);
            if (i == 0 && trimmed.rfind("# ", 0) == 0)
            {
                continue;
            }
            if (IsPlaceholderLine(trimmed))
            {
                continue;
            }

            bool inWord = false;
            for (const unsigned char ch : trimmed)
            {
                if (std::isalnum(ch))
                {
                    if (!inWord)
                    {
                        ++words;
                        inWord = true;
                    }
                }
                else
                {
                    inWord = false;
                }
            }
        }
        return words;
    }

    std::string JournalService::PromptForDate(int year, int month, int day) const
    {
        static const std::array<const char*, 12> Prompts{{
            "what felt lighter than expected today?",
            "what am I quietly proud of right now?",
            "what deserves a small thank-you?",
            "what am I carrying that can wait until tomorrow?",
            "what felt clear today?",
            "what drained me more than it should have?",
            "what do I want a little more of tomorrow?",
            "what detail do I want to remember from today?",
            "what am I avoiding naming directly?",
            "what was actually enough today?",
            "what moved forward, even a little?",
            "what would make tomorrow gentler?",
        }};

        const std::size_t seed =
            static_cast<std::size_t>(year) * 31u + static_cast<std::size_t>(month) * 17u + static_cast<std::size_t>(day) * 13u;
        const std::size_t index = seed % Prompts.size();
        return Prompts[index];
    }

    JournalMonthSummary JournalService::BuildCurrentMonthSummary(const WorkspaceService& workspace,
                                                                 const fs::path& activeRelativePath,
                                                                 const std::string* activeText) const
    {
        const LocalDate today = CurrentLocalDate();
        return BuildMonthSummary(workspace, today.year, today.month, activeRelativePath, activeText);
    }

    JournalMonthSummary JournalService::BuildMonthSummary(const WorkspaceService& workspace, int year, int month,
                                                          const fs::path& activeRelativePath,
                                                          const std::string* activeText) const
    {
        JournalMonthSummary summary;
        summary.year = year;
        summary.month = month;
        summary.daysInMonth = DaysInMonth(year, month);
        summary.firstWeekdayMondayBased = FirstWeekdayMondayBased(year, month);
        summary.days.resize(static_cast<std::size_t>(summary.daysInMonth));
        for (int dayIndex = 0; dayIndex < summary.daysInMonth; ++dayIndex)
        {
            summary.days[static_cast<std::size_t>(dayIndex)].day = dayIndex + 1;
        }

        const LocalDate today = CurrentLocalDate();
        if (today.year == year && today.month == month)
        {
            summary.currentDay = today.day;
        }

        int activeYear = 0;
        int activeMonth = 0;
        int activeDay = 0;
        if (ParseJournalDate(activeRelativePath, &activeYear, &activeMonth, &activeDay) && activeYear == year &&
            activeMonth == month)
        {
            summary.activeDay = activeDay;
        }

        const fs::path normalizedActivePath = PathUtils::NormalizeRelative(activeRelativePath);
        for (const auto& relativePath : workspace.MarkdownFiles())
        {
            int fileYear = 0;
            int fileMonth = 0;
            int fileDay = 0;
            if (!ParseJournalDate(relativePath, &fileYear, &fileMonth, &fileDay) || fileYear != year || fileMonth != month)
            {
                continue;
            }

            auto& daySummary = summary.days[static_cast<std::size_t>(fileDay - 1)];
            daySummary.hasEntry = true;
            const std::string text =
                relativePath == normalizedActivePath && activeText ? *activeText : ReadTextFile(workspace.Resolve(relativePath));
            daySummary.hasContent = HasMeaningfulContent(text);
            daySummary.wordCount = CountWords(text);
        }

        for (const auto& daySummary : summary.days)
        {
            if (daySummary.hasContent)
            {
                ++summary.writtenDays;
            }
        }

        const int streakAnchor = summary.currentDay > 0 ? summary.currentDay : summary.daysInMonth;
        for (int dayIndex = streakAnchor; dayIndex >= 1; --dayIndex)
        {
            if (!summary.days[static_cast<std::size_t>(dayIndex - 1)].hasContent)
            {
                break;
            }
            ++summary.currentStreak;
        }

        if (summary.activeDay > 0)
        {
            summary.activeWordCount = summary.days[static_cast<std::size_t>(summary.activeDay - 1)].wordCount;
        }

        const int graphEndDay = summary.currentDay > 0 ? summary.currentDay : summary.daysInMonth;
        for (int i = 0; i < 7; ++i)
        {
            const int dayNumber = graphEndDay - 6 + i;
            if (dayNumber < 1 || dayNumber > summary.daysInMonth)
            {
                continue;
            }
            summary.recentDayNumbers[static_cast<std::size_t>(i)] = dayNumber;
            summary.recentWordCounts[static_cast<std::size_t>(i)] =
                summary.days[static_cast<std::size_t>(dayNumber - 1)].wordCount;
        }

        const int promptDay = summary.currentDay > 0 ? summary.currentDay : (summary.activeDay > 0 ? summary.activeDay : 1);
        summary.prompt = PromptForDate(year, month, promptDay);
        return summary;
    }
}
