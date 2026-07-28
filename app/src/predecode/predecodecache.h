// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODECACHE_H
#define KIRIVIEW_PREDECODECACHE_H

#include "location/imagelocation.h"
#include "predecodeactiveloads.h"
#include "predecodedimage.h"
#include "predecodedisplayedhistory.h"
#include "rendering/staticimage.h"

#include <QtGlobal>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

namespace kiriview {
struct PredecodeRequest
{
    DisplayedImageLocation location;
};

class PredecodeCache
{
public:
    explicit PredecodeCache(qsizetype byteBudget);

    void clear();
    void clearQueuedLoads();
    void setWindowLocations(const std::vector<DisplayedImageLocation>& locations);
    void setDisplayedLocations(const std::vector<DisplayedImageLocation>& locations);
    void enqueueMissingWindowLoads(
        const DisplayedImageLocation& displayedLocation, const PredecodeActiveLoads& activeLoads);
    std::optional<PredecodeRequest> takeNextRequest(const PredecodeActiveLoads& activeLoads);
    bool windowContains(const DisplayedImageLocation& location) const;
    bool hasImage(const DisplayedImageLocation& location) const;
    bool isInFlight(
        const DisplayedImageLocation& location, const PredecodeActiveLoads& activeLoads) const;
    std::optional<PredecodedImage> findImage(const DisplayedImageLocation& location) const;
    void cacheImage(const DisplayedImageLocation& location, StaticDisplayImagePayload displayImage,
        EmbeddedMetadata metadata = {});
    void cacheDisplayedImage(bool cacheable, const DisplayedImageLocation& location,
        StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata = {});

private:
    struct CachedImage
    {
        DisplayedImageLocation location;
        StaticDisplayImagePayload displayImage;
        qsizetype byteCost = 0;
        mutable quint64 lastUsedSequence = 0;
    };
    using CachedImageIterator = std::vector<CachedImage>::iterator;
    using ConstCachedImageIterator = std::vector<CachedImage>::const_iterator;

    static bool containsLocation(const std::vector<DisplayedImageLocation>& locations,
        const DisplayedImageLocation& location);
    CachedImageIterator findCachedImage(const DisplayedImageLocation& location);
    ConstCachedImageIterator findCachedImage(const DisplayedImageLocation& location) const;
    void removeCachedImage(const DisplayedImageLocation& location);
    std::size_t windowPriority(const DisplayedImageLocation& location) const;
    quint64 nextLastUsedSequence() const;
    void trimImagesToBudget();

    std::vector<DisplayedImageLocation> m_windowLocations;
    PredecodeDisplayedHistory m_displayedHistory;
    std::deque<PredecodeRequest> m_queue;
    std::vector<CachedImage> m_images;
    qsizetype m_byteBudget = 0;
    mutable quint64 m_lastUsedSequence = 0;
};
}

#endif
