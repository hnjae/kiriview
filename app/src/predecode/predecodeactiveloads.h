// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEACTIVELOADS_H
#define KIRIVIEW_PREDECODEACTIVELOADS_H

#include "location/imageurl.h"

#include <QUrl>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

namespace kiriview {
class PredecodeActiveLoads final
{
public:
    PredecodeActiveLoads() = default;

    static PredecodeActiveLoads fromUrls(const std::vector<QUrl>& urls)
    {
        PredecodeActiveLoads loads;
        loads.m_urls.reserve(urls.size());
        for (const QUrl& url : urls) {
            const std::optional<QUrl> normalizedUrl = normalizedValidImageUrl(url);
            if (!normalizedUrl.has_value() || loads.containsNormalized(*normalizedUrl)) {
                continue;
            }

            loads.m_urls.push_back(*normalizedUrl);
        }
        return loads;
    }

    [[nodiscard]] std::size_t size() const { return m_urls.size(); }

    [[nodiscard]] bool contains(const QUrl& url) const
    {
        const std::optional<QUrl> normalizedUrl = normalizedValidImageUrl(url);
        return normalizedUrl.has_value() && containsNormalized(*normalizedUrl);
    }

private:
    [[nodiscard]] bool containsNormalized(const QUrl& normalizedUrl) const
    {
        return std::ranges::contains(m_urls, normalizedUrl);
    }

    std::vector<QUrl> m_urls;
};
}

#endif
