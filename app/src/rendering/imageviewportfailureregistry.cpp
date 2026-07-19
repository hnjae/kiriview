// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportfailureregistry.h"

#include <QMutex>
#include <QMutexLocker>
#include <algorithm>
#include <utility>
#include <vector>

namespace kiriview {
class ImageViewportFailureRegistry::State
{
public:
    struct Entry
    {
        ImageSequenceProviderFailureReference reference;
        ImageLoadFailure failure;
    };

    void insert(ImageSequenceProviderFailureReference reference, ImageLoadFailure failure)
    {
        QMutexLocker locker(&mutex);
        entries.push_back({ reference, std::move(failure) });
    }

    void retire(ImageSequenceProviderFailureReference reference)
    {
        QMutexLocker locker(&mutex);
        const auto match = std::find_if(entries.begin(), entries.end(),
            [reference](const Entry& entry) { return entry.reference == reference; });
        if (match != entries.end()) {
            entries.erase(match);
        }
    }

    std::optional<ImageLoadFailure> resolve(ImageSequenceProviderFailureReference reference) const
    {
        if (!reference.isValid()) {
            return std::nullopt;
        }
        QMutexLocker locker(&mutex);
        const auto match = std::find_if(entries.cbegin(), entries.cend(),
            [reference](const Entry& entry) { return entry.reference == reference; });
        return match == entries.cend() ? std::nullopt
                                       : std::optional<ImageLoadFailure>(match->failure);
    }

    qsizetype size() const
    {
        QMutexLocker locker(&mutex);
        return static_cast<qsizetype>(entries.size());
    }

private:
    mutable QMutex mutex;
    std::vector<Entry> entries;
};

ImageViewportFailureRegistry::ImageViewportFailureRegistry()
    : m_state(std::make_shared<State>())
{
}

ImageViewportFailureRegistry::~ImageViewportFailureRegistry() = default;

ImageSequenceProviderFailureHandle* ImageViewportFailureRegistry::registerFailure(
    ImageLoadFailure failure)
{
    struct ReferenceBox
    {
        ImageSequenceProviderFailureReference reference;
    };

    const auto reference = std::make_shared<ReferenceBox>();
    const std::shared_ptr<State> state = m_state;
    auto* handle = new ImageSequenceProviderFailureHandle(
        [state, reference]() { state->retire(reference->reference); });
    reference->reference = handle->reference();
    m_state->insert(reference->reference, std::move(failure));
    return handle;
}

std::optional<ImageLoadFailure> ImageViewportFailureRegistry::resolve(
    ImageSequenceProviderFailureReference reference) const
{
    return m_state->resolve(reference);
}

qsizetype ImageViewportFailureRegistry::size() const { return m_state->size(); }
}
