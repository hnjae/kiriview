/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <QtCore/QDebug>

class CoordinateResult
{
public:
    CoordinateResult() = default;
    CoordinateResult(bool valid, double x, double y)
        : m_valid(valid)
        , m_x(x)
        , m_y(y)
    {
    }

    bool isValid() const { return m_valid; }
    double x() const { return m_x; }
    double y() const { return m_y; }

    friend bool operator==(CoordinateResult lhs, CoordinateResult rhs)
    {
        return lhs.m_valid == rhs.m_valid && lhs.m_x == rhs.m_x && lhs.m_y == rhs.m_y;
    }
    friend bool operator!=(CoordinateResult lhs, CoordinateResult rhs) { return !(lhs == rhs); }

private:
    bool m_valid = false;
    double m_x = 0.0;
    double m_y = 0.0;
};

inline QDebug operator<<(QDebug debug, CoordinateResult result)
{
    const QDebugStateSaver saver(debug);
    debug.nospace() << "CoordinateResult(valid=" << result.isValid() << ", x=" << result.x()
                    << ", y=" << result.y() << ")";
    return debug;
}
