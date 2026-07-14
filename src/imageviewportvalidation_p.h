#pragma once

#include "imageviewport.h"

#include <cmath>

namespace ImageViewportInternal {

inline bool isFinitePositive(double value) { return std::isfinite(value) && value > 0.0; }

inline bool isFinitePoint(QPointF point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

inline bool isValidFitMode(ImageViewport::FitMode mode)
{
    switch (mode) {
    case ImageViewport::FitMode::Contain:
    case ImageViewport::FitMode::FitWidth:
    case ImageViewport::FitMode::FitHeight:
    case ImageViewport::FitMode::Manual:
        return true;
    }

    return false;
}

inline bool isValidScanDirection(ImageViewport::ScanDirection direction)
{
    switch (direction) {
    case ImageViewport::ScanDirection::Start:
    case ImageViewport::ScanDirection::Previous:
    case ImageViewport::ScanDirection::Next:
    case ImageViewport::ScanDirection::End:
        return true;
    }

    return false;
}

inline bool isValidSpreadDirection(ImageViewport::SpreadDirection direction)
{
    switch (direction) {
    case ImageViewport::SpreadDirection::LeftToRight:
    case ImageViewport::SpreadDirection::RightToLeft:
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

inline bool isValidBackgroundMode(ImageViewport::BackgroundMode mode)
{
    switch (mode) {
    case ImageViewport::BackgroundMode::Transparent:
    case ImageViewport::BackgroundMode::SolidColor:
    case ImageViewport::BackgroundMode::Checkerboard:
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
