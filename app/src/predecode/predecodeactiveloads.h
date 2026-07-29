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
    PredecodeActiveLoads() = default;

    static PredecodeActiveLoads fromKeys(const std::vector<PredecodeImageKey>& imageKeys)
    {
        std::vector<PredecodeWorkKey> keys;
        keys.reserve(imageKeys.size());
        for (const PredecodeImageKey& key : imageKeys) {
            if (!key.isValid()) {
                continue;
            }
            keys.push_back(PredecodeWorkKey { key, 0 });
        }
        return fromWorkKeys(keys);
    }

    static PredecodeActiveLoads fromWorkKeys(const std::vector<PredecodeWorkKey>& keys)
    {
        PredecodeActiveLoads loads;
        loads.m_keys.reserve(keys.size());
        for (const PredecodeWorkKey& key : keys) {
            if (!normalizedValidImageUrl(key.image.location.imageUrl()).has_value()
                || (!key.image.sourceRevision.isValid() && key.lifecycleScope == 0)
                || loads.contains(key)) {
                continue;
            }

            loads.m_keys.push_back(key);
        }
        return loads;
    }

    [[nodiscard]] std::size_t size() const { return m_keys.size(); }

    [[nodiscard]] bool contains(const PredecodeImageKey& key) const
    {
        return key.isValid() && contains(PredecodeWorkKey { key, 0 });
    }

    [[nodiscard]] bool contains(const PredecodeWorkKey& key) const
    {
        return normalizedValidImageUrl(key.image.location.imageUrl()).has_value()
            && std::ranges::any_of(m_keys, [&key](const PredecodeWorkKey& candidate) {
                   return samePredecodeWork(candidate, key);
               });
    }

private:
    std::vector<PredecodeWorkKey> m_keys;
};
}

#endif
