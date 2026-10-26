#include "App/Slate/Extensions/SlateExtensionRegistry.h"

#include <algorithm>
#include <utility>

namespace Software::Slate
{
    namespace
    {
        template <typename T>
        void Upsert(std::vector<T>* items, T item)
        {
            auto existing = std::find_if(items->begin(), items->end(), [&](const T& candidate) {
                return candidate.owner == item.owner && candidate.id == item.id;
            });
            if (existing == items->end())
            {
                items->push_back(std::move(item));
            }
            else
            {
                *existing = std::move(item);
            }
        }

        template <typename T>
        bool RemoveByOwner(std::vector<T>* items, const std::string& owner)
        {
            const auto before = items->size();
            items->erase(std::remove_if(items->begin(), items->end(), [&](const T& item) {
                             return item.owner == owner;
                         }),
                         items->end());
            return items->size() != before;
        }

        bool SameChord(const KeyBinding& lhs, const KeyBinding& rhs)
        {
            return lhs.key == rhs.key && lhs.ctrl == rhs.ctrl && lhs.shift == rhs.shift && lhs.alt == rhs.alt;
        }
    }

    void SlateExtensionRegistry::SetHomeEntry(HomeEntry entry)
    {
        Upsert(&m_homeEntries, std::move(entry));
    }

    void SlateExtensionRegistry::SetStatusItem(StatusItem item)
    {
        if (item.text.empty())
        {
            m_statusItems.erase(std::remove_if(m_statusItems.begin(), m_statusItems.end(), [&](const StatusItem& candidate) {
                                    return candidate.owner == item.owner && candidate.id == item.id;
                                }),
                                m_statusItems.end());
            return;
        }
        Upsert(&m_statusItems, std::move(item));
    }

    void SlateExtensionRegistry::SetSlashCommand(SlashExtension command)
    {
        // Slash tokens are global, so the newest registration wins
        m_slashCommands.erase(std::remove_if(m_slashCommands.begin(), m_slashCommands.end(), [&](const SlashExtension& candidate) {
                                  return candidate.id == command.id;
                              }),
                              m_slashCommands.end());
        m_slashCommands.push_back(std::move(command));
    }

    void SlateExtensionRegistry::SetKeyBinding(CommandKeyBinding binding)
    {
        m_keyBindings.erase(std::remove_if(m_keyBindings.begin(), m_keyBindings.end(), [&](const CommandKeyBinding& candidate) {
                                return SameChord(candidate.binding, binding.binding);
                            }),
                            m_keyBindings.end());
        if (!binding.command.empty())
        {
            m_keyBindings.push_back(std::move(binding));
        }
        ++m_keyBindingRevision;
    }

    void SlateExtensionRegistry::RemoveOwner(const std::string& owner)
    {
        RemoveByOwner(&m_homeEntries, owner);
        RemoveByOwner(&m_statusItems, owner);
        RemoveByOwner(&m_slashCommands, owner);
        if (RemoveByOwner(&m_keyBindings, owner))
        {
            ++m_keyBindingRevision;
        }
    }

    const std::vector<HomeEntry>& SlateExtensionRegistry::HomeEntries() const
    {
        return m_homeEntries;
    }

    const std::vector<StatusItem>& SlateExtensionRegistry::StatusItems() const
    {
        return m_statusItems;
    }

    const std::vector<SlashExtension>& SlateExtensionRegistry::SlashCommands() const
    {
        return m_slashCommands;
    }

    const std::vector<CommandKeyBinding>& SlateExtensionRegistry::KeyBindings() const
    {
        return m_keyBindings;
    }

    const SlashExtension* SlateExtensionRegistry::FindSlashCommand(const std::string& id) const
    {
        const auto it = std::find_if(m_slashCommands.begin(), m_slashCommands.end(), [&](const SlashExtension& command) {
            return command.id == id;
        });
        return it == m_slashCommands.end() ? nullptr : &(*it);
    }

    std::uint64_t SlateExtensionRegistry::KeyBindingRevision() const
    {
        return m_keyBindingRevision;
    }
}
