// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODEDISPLAYEDHISTORY_H
#define KIRIVIEW_PREDECODEDISPLAYEDHISTORY_H

#include <QUrl>
#include <cstddef>
#include <vector>

namespace kiriview {
class PredecodeDisplayedHistory
{
public:
    void clear();
    void setDisplayedUrls(const std::vector<QUrl>& urls);

    [[nodiscard]] bool currentContains(const QUrl& url) const;
    [[nodiscard]] bool recentContains(const QUrl& url) const;
    [[nodiscard]] std::size_t currentPriority(const QUrl& url) const;
    [[nodiscard]] std::size_t recentPriority(const QUrl& url) const;

private:
    static bool containsUrl(const std::vector<QUrl>& urls, const QUrl& url);
    static void removeUrl(std::vector<QUrl>& urls, const QUrl& url);
    static std::size_t priority(const std::vector<QUrl>& urls, const QUrl& url);

    std::vector<QUrl> m_currentUrls;
    std::vector<QUrl> m_recentUrls;
};
}

#endif
