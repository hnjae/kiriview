// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionmediapredecoderuntime.h"

#include "location/sourcekey.h"
#include "predecode/mediapredecodecoordinator.h"

#include <QObject>
#include <utility>

namespace kiriview {
namespace {
    struct MediaPredecodeScopeIdentity
    {
        bool active = false;
        QString parentIdentity;
    };

    MediaPredecodeScopeIdentity mediaPredecodeScopeIdentity(
        const DocumentSessionMediaPredecodeInput& input)
    {
        if (!input.directMediaNavigationActive || input.currentUrl.isEmpty()) {
            return {};
        }

        if (!input.parentSourceKey.valid) {
            return {};
        }

        return MediaPredecodeScopeIdentity {
            true,
            input.parentSourceKey.identity,
        };
    }
}

DocumentSessionMediaPredecodeRuntime::DocumentSessionMediaPredecodeRuntime(
    MediaPredecodeDependencyOverrides dependencies)
    : m_coordinator(std::make_unique<MediaPredecodeCoordinator>(
          resolveMediaPredecodeDependencies(std::move(dependencies))))
{
}

DocumentSessionMediaPredecodeRuntime::~DocumentSessionMediaPredecodeRuntime() = default;

void DocumentSessionMediaPredecodeRuntime::schedule(const DocumentSessionMediaPredecodeInput& input,
    DirectMediaNavigationCandidateSnapshot candidateSnapshot)
{
    schedule(input, QUrl(), std::move(candidateSnapshot));
}

void DocumentSessionMediaPredecodeRuntime::schedule(const DocumentSessionMediaPredecodeInput& input,
    const QUrl& selectedTargetUrl, DirectMediaNavigationCandidateSnapshot candidateSnapshot)
{
    syncScope(input);
    if (!input.directMediaNavigationActive || input.currentUrl.isEmpty()) {
        return;
    }

    const bool immediate = !selectedTargetUrl.isEmpty();
    m_coordinator->schedule(MediaPredecodeCoordinator::Context {
        immediate ? selectedTargetUrl : input.currentUrl,
        std::move(candidateSnapshot),
        displayedImages(input),
        input.firstDisplayDecodeContext,
        immediate,
    });
}

void DocumentSessionMediaPredecodeRuntime::syncScope(
    const DocumentSessionMediaPredecodeInput& input)
{
    const MediaPredecodeScopeIdentity next = mediaPredecodeScopeIdentity(input);
    if (m_scopeIdentityKnown && m_scopeActive == next.active
        && m_scopeParentIdentity == next.parentIdentity) {
        return;
    }

    const bool clearPreviousScope = m_scopeIdentityKnown;
    m_scopeIdentityKnown = true;
    m_scopeActive = next.active;
    m_scopeParentIdentity = next.parentIdentity;
    if (clearPreviousScope) {
        m_coordinator->clear();
    }
}

void DocumentSessionMediaPredecodeRuntime::cacheDisplayedImages(
    const DocumentSessionMediaPredecodeInput& input)
{
    syncScope(input);
    if (!input.directMediaNavigationActive) {
        return;
    }

    std::vector<DisplayedPredecodeImage> images = displayedImages(input);
    if (images.empty()) {
        return;
    }

    m_coordinator->cacheDisplayedImages(images);
}

void DocumentSessionMediaPredecodeRuntime::cancel() { m_coordinator->cancel(); }

void DocumentSessionMediaPredecodeRuntime::clear()
{
    m_scopeIdentityKnown = false;
    m_scopeActive = false;
    m_scopeParentIdentity.clear();
    m_coordinator->clear();
}

std::optional<PredecodedImage> DocumentSessionMediaPredecodeRuntime::findPredecodedImage(
    const QUrl& url) const
{
    return m_coordinator->findPredecodedImage(url);
}

std::vector<DisplayedPredecodeImage> DocumentSessionMediaPredecodeRuntime::displayedImages(
    const DocumentSessionMediaPredecodeInput& input)
{
    if (input.documentKind != DocumentSessionKind::Image
        || !input.activeImageUsesImageDocumentSourceScope || !input.imageReady
        || !input.primaryDisplayedPredecodeImage.has_value()) {
        return {};
    }

    return { *input.primaryDisplayedPredecodeImage };
}
}
