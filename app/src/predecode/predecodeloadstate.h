// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODELOADSTATE_H
#define KIRIVIEW_PREDECODELOADSTATE_H

#include "decoding/imagedecoderequest.h"
#include "predecodeactiveloads.h"
#include "predecodecache.h"
#include "predecodedimage.h"
#include "rendering/staticimage.h"

#include <QtGlobal>
#include <cstddef>
#include <optional>
#include <vector>

namespace kiriview {
struct PredecodeLoadWindow
{
    DisplayedImageLocation primaryDisplayedLocation;
    std::vector<DisplayedImageLocation> locations;
    std::vector<DisplayedPredecodeImage> displayedImages;
    ImageFirstDisplayDecodeContext firstDisplayContext;
    quint64 generation = 0;
    std::size_t parallelLimit = 0;
};

struct PredecodeLoadStart
{
    ImageDecodeRequest request;
    std::optional<StaticDisplayImagePayload> authoritativeSeed;
};

class PredecodeLoadState final
{
public:
    explicit PredecodeLoadState(qsizetype cacheByteBudget);

    void cacheDisplayedImages(const std::vector<DisplayedPredecodeImage>& images);
    void clearWindow();
    void startWindow(const PredecodeLoadWindow& window, const PredecodeActiveLoads& activeLoads);
    std::optional<PredecodeLoadStart> takeNextLoad(const PredecodeActiveLoads& activeLoads);
    void cacheDecodedImage(
        const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage);
    void cacheDecodedImage(const ImageDecodeRequest& request,
        StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata);
    void cancelBackgroundWork();
    void clear();
    std::optional<PredecodedImage> findPredecodedImage(
        const DisplayedImageLocation& location) const;

private:
    struct ActiveWindow
    {
        ImageFirstDisplayDecodeContext firstDisplayContext;
        quint64 generation = 0;
        std::size_t parallelLimit = 0;
    };

    bool canStartMoreLoads(const PredecodeActiveLoads& activeLoads) const;

    std::optional<ActiveWindow> m_activeWindow;
    PredecodeCache m_cache;
};
}

#endif
