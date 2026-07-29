// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_PREDECODECACHE_H
#define KIRIVIEW_PREDECODECACHE_H

#include "location/imagelocation.h"
#include "predecodeactiveloads.h"
#include "predecodedimage.h"
#include "predecodedisplayedhistory.h"
#include "predecodeimagekey.h"
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
    ImageSourceRevision sourceRevision;
    quint64 lifecycleScope = 0;

    [[nodiscard]] PredecodeImageKey key() const { return { location, sourceRevision }; }
    [[nodiscard]] PredecodeWorkKey workKey() const { return { key(), lifecycleScope }; }
};

class PredecodeCache
{
public:
    explicit PredecodeCache(qsizetype byteBudget);

    void clear();
    void clearQueuedLoads();
    void setWindowLocations(const std::vector<DisplayedImageLocation>& locations);
    void setWindowKeys(const std::vector<PredecodeImageKey>& keys);
    void setDisplayedLocations(const std::vector<DisplayedImageLocation>& locations);
    void enqueueMissingWindowLoads(const DisplayedImageLocation& displayedLocation,
        const PredecodeActiveLoads& activeLoads, quint64 lifecycleScope = 0);
    std::optional<PredecodeRequest> takeNextRequest(const PredecodeActiveLoads& activeLoads);
    bool isInFlight(const PredecodeWorkKey& key, const PredecodeActiveLoads& activeLoads) const;
    std::optional<PredecodedImage> findImage(const PredecodeImageKey& key) const;
    std::optional<PredecodedImage> findCandidate(const DisplayedImageLocation& location) const;
    void cacheImage(const DisplayedImageLocation& location, StaticDisplayImagePayload displayImage,
        EmbeddedMetadata metadata = {});
    void cacheDisplayedImage(bool cacheable, const DisplayedImageLocation& location,
        StaticDisplayImagePayload displayImage, EmbeddedMetadata metadata = {});

private:
    struct CachedImage
    {
        PredecodeImageKey key;
        StaticDisplayImagePayload displayImage;
        qsizetype byteCost = 0;
        mutable quint64 lastUsedSequence = 0;
    };
    using CachedImageIterator = std::vector<CachedImage>::iterator;
    using ConstCachedImageIterator = std::vector<CachedImage>::const_iterator;

    static bool containsKey(
        const std::vector<PredecodeImageKey>& keys, const PredecodeImageKey& key);
    bool windowContains(const PredecodeImageKey& key) const;
    bool hasImage(const PredecodeImageKey& key) const;
    ConstCachedImageIterator findCachedImage(const DisplayedImageLocation& location) const;
    CachedImageIterator findCachedImage(const PredecodeImageKey& key);
    ConstCachedImageIterator findCachedImage(const PredecodeImageKey& key) const;
    void removeCachedImage(const PredecodeImageKey& key);
    std::size_t windowPriority(const DisplayedImageLocation& location) const;
    quint64 nextLastUsedSequence() const;
    void trimImagesToBudget();

    std::vector<PredecodeImageKey> m_windowKeys;
    PredecodeDisplayedHistory m_displayedHistory;
    std::deque<PredecodeRequest> m_queue;
    std::vector<CachedImage> m_images;
    qsizetype m_byteBudget = 0;
    mutable quint64 m_lastUsedSequence = 0;
};
}

#endif
