// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEACTIVELOADS_H
#define KIRIVIEW_PREDECODEACTIVELOADS_H

#include "predecodeimagekey.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace kiriview {
class PredecodeActiveLoads final
{
public:
    struct Entry
    {
        PredecodeWorkKey workKey;
        bool sameLocationWildcard = false;
    };

    PredecodeActiveLoads() = default;

    static PredecodeActiveLoads fromKeys(const std::vector<PredecodeImageKey>& imageKeys)
    {
        std::vector<PredecodeWorkKey> keys;
        keys.reserve(imageKeys.size());
        for (const PredecodeImageKey& key : imageKeys) {
            if (!key.isValid()) {
                continue;
            }
            keys.push_back(PredecodeWorkKey { key, {} });
        }
        return fromWorkKeys(keys);
    }

    static PredecodeActiveLoads fromWorkKeys(const std::vector<PredecodeWorkKey>& keys)
    {
        std::vector<Entry> entries;
        entries.reserve(keys.size());
        for (const PredecodeWorkKey& key : keys) {
            entries.push_back(Entry { key, false });
        }
        return fromEntries(entries);
    }

    static PredecodeActiveLoads fromEntries(const std::vector<Entry>& entries)
    {
        PredecodeActiveLoads loads;
        loads.m_entries.reserve(entries.size());
        for (const Entry& entry : entries) {
            const PredecodeWorkKey& key = entry.workKey;
            if (!normalizedValidImageUrl(key.image.location.imageUrl()).has_value()
                || (!key.image.sourceRevision.isValid() && !key.scope.isValid())) {
                continue;
            }

            const bool duplicate
                = std::ranges::any_of(loads.m_entries, [&entry](const Entry& candidate) {
                      return candidate.sameLocationWildcard == entry.sameLocationWildcard
                          && samePredecodeWork(candidate.workKey, entry.workKey);
                  });
            if (!duplicate) {
                loads.m_entries.push_back(entry);
            }
        }
        return loads;
    }

    [[nodiscard]] std::size_t size() const { return m_entries.size(); }

    [[nodiscard]] bool contains(const PredecodeImageKey& key) const
    {
        return key.isValid() && contains(PredecodeWorkKey { key, {} });
    }

    [[nodiscard]] bool contains(const PredecodeWorkKey& key) const
    {
        return normalizedValidImageUrl(key.image.location.imageUrl()).has_value()
            && std::ranges::any_of(m_entries, [&key](const Entry& candidate) {
                   return candidate.workKey.image.location == key.image.location
                       && (candidate.sameLocationWildcard
                           || samePredecodeWork(candidate.workKey, key));
               });
    }

private:
    std::vector<Entry> m_entries;
};
}

#endif
