// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "mediaentrysourcerunner.h"

#include "mediaentrysourcebackend_p.h"

#include <mutex>
#include <utility>

namespace {
namespace Backend = kiriview::MediaEntrySourceBackendDetail;

kiriview::MediaEntrySourceFactory defaultSourceFactory(
    kiriview::MediaEntrySourceFactory sourceFactory)
{
    if (sourceFactory) {
        return std::move(sourceFactory);
    }
    return [](const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
               const kiriview::MediaEntrySourceOpenContext& context) {
        return kiriview::openMediaEntrySource(openedCollectionScope, context);
    };
}
}

namespace kiriview {
MediaEntrySourceRunner::MediaEntrySourceRunner(
    OpenedCollectionScopeLocation openedCollectionScope, MediaEntrySourceFactory sourceFactory)
    : m_openedCollectionScope(std::move(openedCollectionScope))
    , m_sourceFactory(defaultSourceFactory(std::move(sourceFactory)))
{
}

const OpenedCollectionScopeLocation& MediaEntrySourceRunner::openedCollectionScope() const
{
    return m_openedCollectionScope;
}

MediaEntrySourceCandidatesResult MediaEntrySourceRunner::loadImageDocumentPageCandidates(
    const MediaEntrySourceOpenContext& context)
{
    std::scoped_lock lock(m_mutex);
    if (m_cachedCandidates.has_value()) {
        return MediaEntrySourceCandidates { *m_cachedCandidates };
    }

    const std::optional<MediaEntrySourceError> error = ensureSource(context);
    if (error.has_value()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceCandidatesResult>(*error);
    }

    MediaEntrySourceCandidatesResult result = m_source->loadImageDocumentPageCandidates();
    if (const auto* candidates = kiriview::mediaEntrySourceResultValue(result)) {
        m_cachedCandidates = candidates->candidates;
    }
    return result;
}

MediaEntrySourceImageDataResult MediaEntrySourceRunner::loadImageData(
    const QUrl& imageUrl, ImageSourceDataLease lease)
{
    std::scoped_lock lock(m_mutex);
    const std::optional<MediaEntrySourceError> error = ensureSource();
    if (error.has_value()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceImageDataResult>(*error);
    }

    return m_source->loadImageData(imageUrl, std::move(lease));
}

MediaEntrySourceVideoPlaybackDeviceResult MediaEntrySourceRunner::loadVideoPlaybackDevice(
    const QUrl& videoUrl)
{
    std::scoped_lock lock(m_mutex);
    const std::optional<MediaEntrySourceError> error = ensureSource();
    if (error.has_value()) {
        return Backend::mediaEntrySourceErrorResult<MediaEntrySourceVideoPlaybackDeviceResult>(
            *error);
    }

    MediaEntrySourceVideoPlaybackDeviceResult result = m_source->loadVideoPlaybackDevice(videoUrl);
    if (auto* playbackDevice = kiriview::mediaEntrySourceResultValue(result)) {
        playbackDevice->sourceOwner = m_source;
    }
    return result;
}

std::optional<std::vector<ImageDocumentPageCandidate>>
MediaEntrySourceRunner::cachedImageDocumentPageCandidates()
{
    std::scoped_lock lock(m_mutex);
    return m_cachedCandidates;
}

std::optional<MediaEntrySourceError> MediaEntrySourceRunner::ensureSource(
    const MediaEntrySourceOpenContext& context)
{
    if (m_source != nullptr) {
        return std::nullopt;
    }
    if (m_openAttempted) {
        return m_openError;
    }

    m_openAttempted = true;
    MediaEntrySourceOpenResult result = m_sourceFactory(m_openedCollectionScope, context);
    if (const auto* error = kiriview::mediaEntrySourceResultError(result)) {
        m_openError = *error;
        return m_openError;
    }

    const auto* source = kiriview::mediaEntrySourceResultValue(result);
    if (source == nullptr || *source == nullptr) {
        m_openError = Backend::mediaEntrySourceError(
            MediaEntrySourceErrorCause::ProviderUnavailable, MediaEntrySourceBackendKind::Unknown,
            MediaEntrySourceOperation::OpenCollection, m_openedCollectionScope,
            QStringLiteral("media entry source factory returned no source"));
        return m_openError;
    }

    m_source = *source;
    return std::nullopt;
}
}
