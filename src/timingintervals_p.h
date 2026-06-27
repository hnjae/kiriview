#pragma once

#include <QtCore/QVector>

class TimingIntervals
{
public:
    static TimingIntervals fromFrameDurations(const QVector<int>& frameDurations);

    bool isValid() const;
    int frameCount() const;
    int totalDuration() const;
    int frameStartPosition(int frame) const;
    int frameDuration(int frame) const;
    int frameIndexForPosition(int position) const;

private:
    QVector<int> m_frameDurations;
    QVector<int> m_frameStarts;
    int m_totalDuration = -1;
};
