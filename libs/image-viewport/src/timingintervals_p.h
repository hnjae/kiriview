/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtCore/QVector>

class TimingIntervals
{
public:
    static TimingIntervals fromFrameDurations(const QVector<int>& frameDurations);

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] int frameCount() const;
    [[nodiscard]] int totalDuration() const;
    [[nodiscard]] int frameStartPosition(int frame) const;
    [[nodiscard]] int frameDuration(int frame) const;
    [[nodiscard]] int frameIndexForPosition(int position) const;

private:
    QVector<int> m_frameDurations;
    QVector<int> m_frameStarts;
    int m_totalDuration = -1;
};
