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

    int takeElapsed(qint64 nowMilliseconds)
    {
        const qint64 elapsedMilliseconds
            = m_valid ? std::max<qint64>(0, nowMilliseconds - m_startedAtMilliseconds) : 0;
        invalidate();
        return static_cast<int>(
            std::min<qint64>(elapsedMilliseconds, std::numeric_limits<int>::max()));
    }

private:
    bool m_valid = false;
    qint64 m_startedAtMilliseconds = 0;
};

} // namespace ImageViewportInternal
