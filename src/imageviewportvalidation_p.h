#pragma once

#include <ImageViewport/imageviewporttypes.h>

#include <QtCore/QPointF>

#include <cmath>

namespace ImageViewportInternal {

inline bool isFinitePositive(double value) { return std::isfinite(value) && value > 0.0; }

inline bool isFinitePoint(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

inline bool isValidFitMode(ImageViewportFitMode mode)
{
    switch (mode) {
    case ImageViewportFitMode::Contain:
    case ImageViewportFitMode::FitWidth:
    case ImageViewportFitMode::FitHeight:
    case ImageViewportFitMode::Manual:
        return true;
    }

    return false;
}

inline bool isValidContentAnchor(ImageViewportContentAnchor direction)
{
    switch (direction) {
    case ImageViewportContentAnchor::Start:
    case ImageViewportContentAnchor::End:
        return true;
    }

    return false;
}

inline bool isValidSpreadDirection(ImageViewportSpreadDirection direction)
{
    switch (direction) {
    case ImageViewportSpreadDirection::LeftToRight:
    case ImageViewportSpreadDirection::RightToLeft:
        return true;
    }

    return false;
}

inline bool isValidPageRole(ImageViewportPageRole role)
{
    switch (role) {
    case ImageViewportPageRole::Primary:
    case ImageViewportPageRole::Secondary:
        return true;
    }

    return false;
}

inline bool isValidBackgroundMode(ImageViewportBackgroundMode mode)
{
    switch (mode) {
    case ImageViewportBackgroundMode::Transparent:
    case ImageViewportBackgroundMode::SolidColor:
    case ImageViewportBackgroundMode::Checkerboard:
        return true;
    }

    return false;
}

inline bool isValidQualityPreference(ImageViewportQualityPreference preference)
{
    switch (preference) {
    case ImageViewportQualityPreference::Default:
    case ImageViewportQualityPreference::FastFirstDisplay:
    case ImageViewportQualityPreference::BalancedDetail:
    case ImageViewportQualityPreference::ExactDetail:
        return true;
    }

    return false;
}

inline bool isValidExactnessPreference(ImageViewportExactnessPreference preference)
{
    switch (preference) {
    case ImageViewportExactnessPreference::Default:
    case ImageViewportExactnessPreference::AllowInexact:
    case ImageViewportExactnessPreference::PreferExact:
    case ImageViewportExactnessPreference::RequireExact:
        return true;
    }

    return false;
}

}
