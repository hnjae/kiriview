#include "timingintervals_p.h"

TimingIntervals TimingIntervals::fromFrameDurations(const QVector<int>& frameDurations)
{
    TimingIntervals intervals;
    if (frameDurations.isEmpty()) {
        return intervals;
    }

    intervals.m_frameDurations = frameDurations;
    intervals.m_frameStarts.reserve(frameDurations.size());
    int position = 0;
    for (int duration : frameDurations) {
        if (duration <= 0) {
            intervals = {};
            return intervals;
        }
        intervals.m_frameStarts.append(position);
        position += duration;
    }
    intervals.m_totalDuration = position;
    return intervals;
}

bool TimingIntervals::isValid() const { return m_totalDuration > 0; }

int TimingIntervals::frameCount() const { return isValid() ? m_frameDurations.size() : -1; }

int TimingIntervals::totalDuration() const { return m_totalDuration; }

int TimingIntervals::frameStartPosition(int frame) const
{
    if (!isValid() || frame < 0 || frame >= m_frameStarts.size()) {
        return -1;
    }
    return m_frameStarts.at(frame);
}

int TimingIntervals::frameDuration(int frame) const
{
    if (!isValid() || frame < 0 || frame >= m_frameDurations.size()) {
        return -1;
    }
    return m_frameDurations.at(frame);
}

int TimingIntervals::frameIndexForPosition(int position) const
{
    if (!isValid() || position < 0 || position > m_totalDuration) {
        return -1;
    }
    if (position == m_totalDuration) {
        return m_frameDurations.size() - 1;
    }

    for (int index = 0; index < m_frameStarts.size(); ++index) {
        const int frameStart = m_frameStarts.at(index);
        const int frameEnd = frameStart + m_frameDurations.at(index);
        if (position >= frameStart && position < frameEnd) {
            return index;
        }
    }

    return -1;
}
