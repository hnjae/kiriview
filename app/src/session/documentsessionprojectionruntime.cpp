// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "documentsessionprojectionruntime.h"

#include "session/thumbnaillogging.h"

#include <QDebug>

#include <cstddef>
#include <utility>

namespace kiriview {
namespace {
    const DirectMediaNavigationCandidateSnapshot& emptyDirectMediaNavigationCandidateSnapshot()
    {
        static const DirectMediaNavigationCandidateSnapshot snapshot;
        return snapshot;
    }

    const char* activeNavigationSourceKindLogName(ActiveNavigationSourceKind sourceKind)
    {
        switch (sourceKind) {
        case ActiveNavigationSourceKind::OrdinaryDirectMedia:
            return "ordinary-direct-media";
        case ActiveNavigationSourceKind::ImageDocumentPages:
            return "image-document-pages";
        case ActiveNavigationSourceKind::None:
            return "none";
        }

        return "unknown";
    }

    const char* missingActiveNavigationThumbnailRowSetReason(ActiveNavigationSourceKind sourceKind,
        ActiveNavigationSnapshot navigation,
        const DirectMediaNavigationCandidateSnapshot& directMediaNavigationCandidateSnapshot,
        const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
    {
        if (!navigation.available) {
            return "navigation-unavailable";
        }
        if (!navigation.known) {
            return "navigation-unknown";
        }
        if (navigation.count <= 0) {
            return "navigation-empty";
        }

        switch (sourceKind) {
        case ActiveNavigationSourceKind::OrdinaryDirectMedia:
            if (!directMediaNavigationCandidateSnapshot.known) {
                return "direct-media-candidates-unknown";
            }
            if (std::cmp_not_equal(
                    directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot)
                        .size(),
                    navigation.count)) {
                return "direct-media-count-mismatch";
            }
            return "direct-media-identity-missing";
        case ActiveNavigationSourceKind::ImageDocumentPages:
            if (!imageDocumentPageCandidateSnapshot.known) {
                return "image-page-candidates-unknown";
            }
            if (!imageDocumentPageCandidateSnapshot.source.has_value()) {
                return "image-page-candidate-source-missing";
            }
            if (std::cmp_not_equal(
                    imageDocumentPageCandidateRows(imageDocumentPageCandidateSnapshot).size(),
                    navigation.count)) {
                return "image-page-count-mismatch";
            }
            return "image-page-identity-missing";
        case ActiveNavigationSourceKind::None:
            return "source-kind-none";
        }

        return "unknown";
    }

    bool samePageNavigation(ImageDocumentPageActiveNavigationSnapshot left,
        ImageDocumentPageActiveNavigationSnapshot right)
    {
        return left.known == right.known && left.canOpenPrevious == right.canOpenPrevious
            && left.canOpenNext == right.canOpenNext && left.atKnownFirst == right.atKnownFirst
            && left.atKnownLast == right.atKnownLast && left.currentNumber == right.currentNumber
            && left.count == right.count;
    }

    bool sameDirectNavigation(
        DirectMediaActiveNavigationInput left, DirectMediaActiveNavigationInput right)
    {
        return left.known == right.known
            && left.boundaryState.canOpenPrevious == right.boundaryState.canOpenPrevious
            && left.boundaryState.canOpenNext == right.boundaryState.canOpenNext
            && left.boundaryState.atKnownFirst == right.boundaryState.atKnownFirst
            && left.boundaryState.atKnownLast == right.boundaryState.atKnownLast
            && left.boundaryState.currentNumber == right.boundaryState.currentNumber
            && left.boundaryState.count == right.boundaryState.count;
    }

    bool sameMetadata(const EmbeddedMetadata& left, const EmbeddedMetadata& right)
    {
        if (left.cameraMake != right.cameraMake || left.cameraModel != right.cameraModel
            || left.taken != right.taken || left.location != right.location
            || left.lens != right.lens || left.exposure != right.exposure || left.iso != right.iso
            || left.focalLength != right.focalLength || left.software != right.software
            || left.duration != right.duration || left.frameSize != right.frameSize
            || left.advancedRows.size() != right.advancedRows.size()) {
            return false;
        }
        for (std::size_t index = 0; index < left.advancedRows.size(); ++index) {
            if (left.advancedRows.at(index).label != right.advancedRows.at(index).label
                || left.advancedRows.at(index).value != right.advancedRows.at(index).value) {
                return false;
            }
        }
        return true;
    }

    bool samePublicProjectionDependency(const DocumentSessionPublicSnapshotInput& left,
        const DocumentSessionPublicSnapshotInput& right)
    {
        const auto& leftSession = left.session;
        const auto& rightSession = right.session;
        if (leftSession.sourceUrl != rightSession.sourceUrl
            || leftSession.documentKind != rightSession.documentKind
            || leftSession.sessionErrorString != rightSession.sessionErrorString
            || leftSession.fileDeletionInProgress != rightSession.fileDeletionInProgress
            || leftSession.directImageLoadMayUseImageDocumentSourceScope
                != rightSession.directImageLoadMayUseImageDocumentSourceScope
            || !sameDirectNavigation(
                leftSession.directMediaNavigation, rightSession.directMediaNavigation)
            || leftSession.activeNavigationRevealIntent != rightSession.activeNavigationRevealIntent
            || leftSession.activeNavigationRevealDirection
                != rightSession.activeNavigationRevealDirection
            || leftSession.openedCollectionVideoActive != rightSession.openedCollectionVideoActive
            || left.operations.displayedMediaOpenWithAvailable
                != right.operations.displayedMediaOpenWithAvailable) {
            return false;
        }

        const auto& leftImage = left.image;
        const auto& rightImage = right.image;
        const bool imageRelevant = leftSession.documentKind == DocumentSessionKind::Image
            || leftSession.openedCollectionVideoActive;
        if (imageRelevant
            && (leftImage.sourceMayRepresentDocument != rightImage.sourceMayRepresentDocument
                || !samePageNavigation(leftImage.pageNavigation, rightImage.pageNavigation)
                || leftImage.displayedUrl != rightImage.displayedUrl
                || leftImage.displayedOpenedCollectionScope
                    != rightImage.displayedOpenedCollectionScope
                || leftImage.windowTitleFileName != rightImage.windowTitleFileName
                || leftImage.directMediaSize != rightImage.directMediaSize
                || !sameMetadata(leftImage.embeddedMetadata, rightImage.embeddedMetadata)
                || leftImage.loading != rightImage.loading
                || leftImage.completeAuthoritativeDisplayAvailable
                    != rightImage.completeAuthoritativeDisplayAvailable
                || leftImage.readyForDeletion != rightImage.readyForDeletion
                || leftImage.readyForInformation != rightImage.readyForInformation
                || leftImage.openedCollectionScopeActive != rightImage.openedCollectionScopeActive
                || leftImage.unsupportedOpenedCollectionVideo
                    != rightImage.unsupportedOpenedCollectionVideo
                || leftImage.directImageReplacementPending
                    != rightImage.directImageReplacementPending
                || leftImage.containerNavigationAvailable != rightImage.containerNavigationAvailable
                || leftImage.twoPageModeEnabled != rightImage.twoPageModeEnabled
                || leftImage.twoPageModeAvailable != rightImage.twoPageModeAvailable
                || leftImage.rightToLeftReadingEnabled != rightImage.rightToLeftReadingEnabled
                || leftImage.rightToLeftReadingAvailable != rightImage.rightToLeftReadingAvailable
                || leftImage.fitModeSelected != rightImage.fitModeSelected
                || leftImage.fitHeightModeSelected != rightImage.fitHeightModeSelected
                || leftImage.fitWidthModeSelected != rightImage.fitWidthModeSelected
                || leftImage.zoomPercentKnown != rightImage.zoomPercentKnown
                || leftImage.zoomPercent != rightImage.zoomPercent
                || leftImage.viewportPannable != rightImage.viewportPannable
                || leftImage.errorString != rightImage.errorString)) {
            return false;
        }

        if (leftSession.documentKind != DocumentSessionKind::Video) {
            return true;
        }
        const auto& leftVideo = left.video;
        const auto& rightVideo = right.video;
        return leftVideo.sourceUrl == rightVideo.sourceUrl
            && leftVideo.windowTitleFileName == rightVideo.windowTitleFileName
            && leftVideo.directMediaSize == rightVideo.directMediaSize
            && sameMetadata(leftVideo.embeddedMetadata, rightVideo.embeddedMetadata)
            && leftVideo.ready == rightVideo.ready && leftVideo.hasVideo == rightVideo.hasVideo
            && leftVideo.sourcePresent == rightVideo.sourcePresent
            && leftVideo.error == rightVideo.error
            && leftVideo.videoSeekable == rightVideo.videoSeekable
            && leftVideo.videoDuration == rightVideo.videoDuration
            && leftVideo.zoomPercentKnown == rightVideo.zoomPercentKnown
            && leftVideo.zoomPercent == rightVideo.zoomPercent
            && leftVideo.errorString == rightVideo.errorString;
    }
}

DocumentSessionProjectionRuntime::DocumentSessionProjectionRuntime(
    DocumentSessionProjectionRuntimePorts ports)
    : m_ports(std::move(ports))
{
}

DocumentSessionProjectionRuntime::~DocumentSessionProjectionRuntime()
{
    m_callbackLifetime.reset();
}

void DocumentSessionProjectionRuntime::publish(const DocumentSessionPublicSnapshotInput& input,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    const std::weak_ptr<void> callbackLifetime = m_callbackLifetime;
    const quint64 publicationRevision = m_publicationAdmission.next();
    const DocumentSessionProjectionRuntimePorts ports = m_ports;
    const auto current = [this, callbackLifetime, publicationRevision]() {
        return !callbackLifetime.expired() && m_publicationAdmission.accepts(publicationRevision);
    };
    const bool publicDependencyChanged = !m_publicDependencyInput.has_value()
        || !samePublicProjectionDependency(*m_publicDependencyInput, input);
    if (publicDependencyChanged && ports.updatePublicSnapshot) {
        ports.updatePublicSnapshot(input);
        if (!current()) {
            return;
        }
    }
    if (publicDependencyChanged) {
        m_publicDependencyInput = input;
    }
    const bool thumbnailDependencyChanged = syncActiveNavigationThumbnailRows(
        imageDocumentPageCandidateSnapshot, ports, callbackLifetime, publicationRevision);
    if (!current()) {
        return;
    }
    if ((publicDependencyChanged || thumbnailDependencyChanged)
        && ports.clearActiveNavigationRevealContextIfUnavailable) {
        ports.clearActiveNavigationRevealContextIfUnavailable();
    }
}

void DocumentSessionProjectionRuntime::publishForSourceKind(
    const DocumentSessionPublicSnapshotInput& input, ActiveNavigationSourceKind sourceKind,
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot)
{
    const std::weak_ptr<void> callbackLifetime = m_callbackLifetime;
    const quint64 publicationRevision = m_publicationAdmission.next();
    const DocumentSessionProjectionRuntimePorts ports = m_ports;
    const auto current = [this, callbackLifetime, publicationRevision]() {
        return !callbackLifetime.expired() && m_publicationAdmission.accepts(publicationRevision);
    };
    const bool publicDependencyChanged = !m_publicDependencyInput.has_value()
        || !samePublicProjectionDependency(*m_publicDependencyInput, input);
    bool accepted = false;
    if (ports.activeNavigationSourceKind) {
        accepted = ports.activeNavigationSourceKind() == sourceKind;
        if (!current()) {
            return;
        }
    }
    if (publicDependencyChanged && ports.updatePublicSnapshotForSourceKind) {
        accepted = ports.updatePublicSnapshotForSourceKind(input, sourceKind);
        if (!current()) {
            return;
        }
        if (accepted) {
            m_publicDependencyInput = input;
        }
    }
    if (accepted) {
        syncActiveNavigationThumbnailRows(
            imageDocumentPageCandidateSnapshot, ports, callbackLifetime, publicationRevision);
        if (!current()) {
            return;
        }
    }
    if (ports.clearActiveNavigationRevealContextIfUnavailable) {
        ports.clearActiveNavigationRevealContextIfUnavailable();
    }
}

bool DocumentSessionProjectionRuntime::syncActiveNavigationThumbnailRows(
    const ImageDocumentPageCandidateListSnapshot& imageDocumentPageCandidateSnapshot,
    const DocumentSessionProjectionRuntimePorts& ports, const std::weak_ptr<void>& callbackLifetime,
    quint64 publicationRevision)
{
    const auto current = [this, callbackLifetime, publicationRevision]() {
        return !callbackLifetime.expired() && m_publicationAdmission.accepts(publicationRevision);
    };
    ActiveNavigationSourceKind sourceKind = ActiveNavigationSourceKind::None;
    if (ports.activeNavigationSourceKind) {
        sourceKind = ports.activeNavigationSourceKind();
        if (!current()) {
            return false;
        }
    }
    ActiveNavigationSnapshot navigation;
    if (ports.activeNavigationSnapshot) {
        navigation = ports.activeNavigationSnapshot();
        if (!current()) {
            return false;
        }
    }
    DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot;
    if (ports.directMediaNavigationCandidateSnapshot) {
        const DirectMediaNavigationCandidateSnapshot& candidateSnapshot
            = ports.directMediaNavigationCandidateSnapshot();
        if (!current()) {
            return false;
        }
        directMediaNavigationCandidateSnapshot = candidateSnapshot;
    } else {
        directMediaNavigationCandidateSnapshot = emptyDirectMediaNavigationCandidateSnapshot();
    }
    const std::optional<ActiveNavigationThumbnailRowSetIdentity> rowSetIdentity
        = activeNavigationThumbnailRowSetIdentity(sourceKind, navigation,
            directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot);
    if (!rowSetIdentity.has_value()) {
        if (m_activeNavigationThumbnailProjectionInitialized
            && !m_activeNavigationThumbnailIdentity.has_value()) {
            return false;
        }
        m_activeNavigationThumbnailProjectionInitialized = true;
        qCDebug(kiriviewThumbnailLog)
            << "Active navigation thumbnail row-set unavailable"
            << "reason"
            << missingActiveNavigationThumbnailRowSetReason(sourceKind, navigation,
                   directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot)
            << "sourceKind" << activeNavigationSourceKindLogName(sourceKind)
            << "navigationAvailable" << navigation.available << "navigationKnown"
            << navigation.known << "navigationCurrent" << navigation.currentNumber
            << "navigationCount" << navigation.count << "directCandidatesKnown"
            << directMediaNavigationCandidateSnapshot.known << "directCandidateRevision"
            << directMediaNavigationCandidateSnapshot.revision << "directCandidateRows"
            << directMediaNavigationCandidateRows(directMediaNavigationCandidateSnapshot).size()
            << "imagePageCandidatesKnown" << imageDocumentPageCandidateSnapshot.known
            << "imagePageCandidateHasSource"
            << imageDocumentPageCandidateSnapshot.source.has_value() << "imagePageCandidateRevision"
            << imageDocumentPageCandidateSnapshot.revision << "imagePageCandidateRows"
            << imageDocumentPageCandidateRows(imageDocumentPageCandidateSnapshot).size()
            << "hadPreviousIdentity" << m_activeNavigationThumbnailIdentity.has_value();
        m_activeNavigationThumbnailIdentity.reset();
        m_activeNavigationThumbnailCurrentNumber = 0;
        if (ports.setActiveNavigationThumbnailRows) {
            ports.setActiveNavigationThumbnailRows({});
        }
        return true;
    }

    if (m_activeNavigationThumbnailIdentity.has_value()
        && sameActiveNavigationThumbnailRowSetIdentity(
            *m_activeNavigationThumbnailIdentity, *rowSetIdentity)) {
        if (m_activeNavigationThumbnailCurrentNumber == navigation.currentNumber) {
            return false;
        }
        m_activeNavigationThumbnailCurrentNumber = navigation.currentNumber;
        if (ports.setActiveNavigationThumbnailCurrentNumber) {
            ports.setActiveNavigationThumbnailCurrentNumber(navigation.currentNumber);
        }
        return true;
    }

    std::vector<ActiveNavigationThumbnailRow> rows
        = projectActiveNavigationThumbnailRows(sourceKind, navigation,
            directMediaNavigationCandidateSnapshot, imageDocumentPageCandidateSnapshot);
    m_activeNavigationThumbnailIdentity = rowSetIdentity;
    m_activeNavigationThumbnailProjectionInitialized = true;
    m_activeNavigationThumbnailCurrentNumber = navigation.currentNumber;
    if (ports.setActiveNavigationThumbnailRows) {
        ports.setActiveNavigationThumbnailRows(std::move(rows));
    }
    return true;
}
}
