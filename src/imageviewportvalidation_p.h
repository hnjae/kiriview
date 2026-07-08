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

inline bool isValidPageRole(ImageViewport::PageRole role)
{
    switch (role) {
    case ImageViewport::PageRole::Primary:
    case ImageViewport::PageRole::Secondary:
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

inline bool isValidQualityPreference(ImageViewport::QualityPreference preference)
{
    switch (preference) {
    case ImageViewport::QualityPreference::Default:
    case ImageViewport::QualityPreference::FastFirstDisplay:
    case ImageViewport::QualityPreference::BalancedDetail:
    case ImageViewport::QualityPreference::ExactDetail:
        return true;
    }

    return false;
}

inline bool isValidExactnessPreference(ImageViewport::ExactnessPreference preference)
{
    switch (preference) {
    case ImageViewport::ExactnessPreference::Default:
    case ImageViewport::ExactnessPreference::AllowInexact:
    case ImageViewport::ExactnessPreference::PreferExact:
    case ImageViewport::ExactnessPreference::RequireExact:
        return true;
    }

    return false;
}

}
