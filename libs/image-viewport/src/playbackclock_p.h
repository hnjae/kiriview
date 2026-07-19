/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtGlobal>

#include <algorithm>
#include <limits>

namespace ImageViewportInternal {

class PlaybackClock
{
public:
    void restart(qint64 nowMilliseconds)
    {
        m_valid = true;
        m_startedAtMilliseconds = nowMilliseconds;
    }

    void invalidate()
    {
        m_valid = false;
        m_startedAtMilliseconds = 0;
    }

    bool isValid() const { return m_valid; }

    int elapsed(qint64 nowMilliseconds) const
    {
        const qint64 elapsedMilliseconds
            = m_valid ? std::max<qint64>(0, nowMilliseconds - m_startedAtMilliseconds) : 0;
        return static_cast<int>(
            std::min<qint64>(elapsedMilliseconds, std::numeric_limits<int>::max()));
    }

    int takeElapsed(qint64 nowMilliseconds)
    {
        const int elapsedMilliseconds = elapsed(nowMilliseconds);
        invalidate();
        return elapsedMilliseconds;
    }

private:
    bool m_valid = false;
    qint64 m_startedAtMilliseconds = 0;
};

} // namespace ImageViewportInternal
