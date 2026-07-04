#pragma once

#include <QtCore/QRectF>

namespace ImageViewportInternal {

inline bool rectsExactlyEqual(const QRectF& left, const QRectF& right)
{
    return left.x() == right.x() && left.y() == right.y() && left.width() == right.width()
        && left.height() == right.height();
}

inline bool rectsDifferExactly(const QRectF& left, const QRectF& right)
{
    return !rectsExactlyEqual(left, right);
}

}
