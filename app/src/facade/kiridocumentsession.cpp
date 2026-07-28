// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiridocumentsession.h"

#include "cache/imagecachepolicy.h"
#include "facade/documentsessionpublicsignals.h"
#include "facade/kiriimagedocument.h"
#include "facade/kirivideodocument.h"
#include "facade/mediaopendialogfilters.h"
#include "localization/activenavigationboundarytext.h"
#include "rendering/displayimagestore.h"
#include "session/activenavigationthumbnaildemand.h"
#include "session/thumbnailimagestore.h"

#include <QPointer>
#include <QSignalBlocker>
#include <QVariantMap>
#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace {
KiriDocumentSession::DocumentKind fromRuntimeKind(kiriview::DocumentSessionKind kind)
{
    switch (kind) {
    case kiriview::DocumentSessionKind::Empty:
        return KiriDocumentSession::DocumentKind::Empty;
    case kiriview::DocumentSessionKind::Image:
        return KiriDocumentSession::DocumentKind::Image;
    case kiriview::DocumentSessionKind::Video:
        return KiriDocumentSession::DocumentKind::Video;
    }

    return KiriDocumentSession::DocumentKind::Empty;
}

kiriview::FileDeletionMode toFileDeletionMode(KiriDocumentSession::DeletionMode mode)
{
    switch (mode) {
    case KiriDocumentSession::DeletionMode::MoveToTrash:
        return kiriview::FileDeletionMode::MoveToTrash;
    case KiriDocumentSession::DeletionMode::DeletePermanently:
        return kiriview::FileDeletionMode::DeletePermanently;
    }

    return kiriview::FileDeletionMode::MoveToTrash;
}

std::vector<kiriview::DocumentSessionPublicSignal> mergePublicSignals(
    std::vector<kiriview::DocumentSessionPublicSignal> preferred,
    const std::vector<kiriview::DocumentSessionPublicSignal>& fallback)
{
    for (kiriview::DocumentSessionPublicSignal signal : fallback) {
        if (!std::ranges::contains(preferred, signal)) {
            preferred.push_back(signal);
        }
    }

    const auto projectionRevision = std::ranges::find(
        preferred, kiriview::DocumentSessionPublicSignal::PublicProjectionRevision);
    if (projectionRevision != preferred.end() && projectionRevision != preferred.begin()) {
        std::rotate(preferred.begin(), projectionRevision, std::next(projectionRevision));
    }
    return preferred;
}

KiriImageDocument::DeletionMode toImageDocumentDeletionMode(kiriview::FileDeletionMode mode)
{
    switch (mode) {
    case kiriview::FileDeletionMode::MoveToTrash:
        return KiriImageDocument::DeletionMode::MoveToTrash;
    case kiriview::FileDeletionMode::DeletePermanently:
        return KiriImageDocument::DeletionMode::DeletePermanently;
    }

    return KiriImageDocument::DeletionMode::MoveToTrash;
}

kiriview::DocumentSessionImageDocumentSnapshot imageDocumentSessionSnapshot(
    KiriImageDocument& document)
{
    return kiriview::DocumentSessionImageDocumentSnapshot {
        document.sourceUrl(),
        document.errorString(),
        document.windowTitleFileName(),
        document.displayedUrl(),
        document.displayedOpenedCollectionScope(),
        document.primaryImageSize(),
        document.status() == KiriImageDocument::Status::Loading,
        document.completeAuthoritativeDisplayAvailable(),
        document.status() == KiriImageDocument::Status::Ready,
        document.status() == KiriImageDocument::Status::Error,
        document.unsupportedOpenedCollectionVideo(),
        document.fileDeletionInProgress(),
        document.openedCollectionScopeActive(),
        document.ordinaryDirectMediaScopeActive(),
        document.containerNavigationAvailable(),
        document.twoPageModeEnabled(),
        document.twoPageModeAvailable(),
        document.rightToLeftReadingEnabled(),
        document.rightToLeftReadingAvailable(),
        document.zoomMode() == KiriImageDocument::ZoomMode::Fit,
        document.zoomMode() == KiriImageDocument::ZoomMode::FitHeight,
        document.zoomMode() == KiriImageDocument::ZoomMode::FitWidth,
        document.viewportPannable(),
        document.zoomPercentKnown(),
        document.zoomPercent(),
        document.embeddedMetadata(),
        document.pageNavigationSnapshot(),
        document.confirmedPageCandidateSnapshot(),
        document.activeNavigationSnapshot(),
        document.primaryDisplayedPredecodeImage(),
        document.firstDisplayDecodeContext(),
        document.sourceKind(),
    };
}

kiriview::DocumentSessionVideoDocumentSnapshot videoDocumentSessionSnapshot(
    KiriVideoDocument& document)
{
    return kiriview::DocumentSessionVideoDocumentSnapshot {
        document.sourceUrl(),
        document.errorString(),
        document.windowTitleFileName(),
        document.videoSize(),
        document.status() == KiriVideoDocument::Status::Ready,
        document.status() == KiriVideoDocument::Status::Error,
        document.hasVideo(),
        document.playbackControls()->timelineInteractive(),
        static_cast<qint64>(document.playbackControls()->sliderMaximumMsec()),
        document.zoomPercentKnown(),
        document.zoomPercent(),
        document.embeddedMetadata(),
    };
}

KiriDocumentSession::ActiveNavigationBoundaryScope fromRuntimeBoundaryScope(
    kiriview::ActiveNavigationBoundaryScope scope)
{
    switch (scope) {
    case kiriview::ActiveNavigationBoundaryScope::DirectMedia:
        return KiriDocumentSession::ActiveNavigationBoundaryScope::DirectMediaNavigationBoundary;
    case kiriview::ActiveNavigationBoundaryScope::ImageDocumentPage:
        return KiriDocumentSession::ActiveNavigationBoundaryScope::
            ImageDocumentPageNavigationBoundary;
    case kiriview::ActiveNavigationBoundaryScope::None:
        return KiriDocumentSession::ActiveNavigationBoundaryScope::NoNavigationBoundary;
    }

    return KiriDocumentSession::ActiveNavigationBoundaryScope::NoNavigationBoundary;
}

KiriDocumentSession::ActiveNavigationRequestResult fromRuntimeRequestOutcome(
    kiriview::ActiveNavigationDispatchOutcome outcome)
{
    switch (outcome) {
    case kiriview::ActiveNavigationDispatchOutcome::NoOp:
        return KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult;
    case kiriview::ActiveNavigationDispatchOutcome::Dispatch:
        return KiriDocumentSession::ActiveNavigationRequestResult::
            ActiveNavigationRequestDispatched;
    case kiriview::ActiveNavigationDispatchOutcome::FirstBoundary:
        return KiriDocumentSession::ActiveNavigationRequestResult::FirstActiveNavigationBoundary;
    case kiriview::ActiveNavigationDispatchOutcome::LastBoundary:
        return KiriDocumentSession::ActiveNavigationRequestResult::LastActiveNavigationBoundary;
    }

    return KiriDocumentSession::ActiveNavigationRequestResult::NoActiveNavigationRequestResult;
}

KiriDocumentSession::ActiveNavigationRevealIntent fromRuntimeRevealIntent(
    kiriview::ActiveNavigationRevealIntent intent)
{
    switch (intent) {
    case kiriview::ActiveNavigationRevealIntent::None:
        return KiriDocumentSession::ActiveNavigationRevealIntent::None;
    case kiriview::ActiveNavigationRevealIntent::ThumbnailActivation:
        return KiriDocumentSession::ActiveNavigationRevealIntent::ThumbnailActivation;
    case kiriview::ActiveNavigationRevealIntent::AdjacentNavigation:
        return KiriDocumentSession::ActiveNavigationRevealIntent::AdjacentNavigation;
    case kiriview::ActiveNavigationRevealIntent::LargeJump:
        return KiriDocumentSession::ActiveNavigationRevealIntent::LargeJump;
    case kiriview::ActiveNavigationRevealIntent::LoadOrOpen:
        return KiriDocumentSession::ActiveNavigationRevealIntent::LoadOrOpen;
    case kiriview::ActiveNavigationRevealIntent::ProgrammaticSync:
        return KiriDocumentSession::ActiveNavigationRevealIntent::ProgrammaticSync;
    }

    return KiriDocumentSession::ActiveNavigationRevealIntent::None;
}

KiriDocumentSession::ActiveNavigationRevealDirection fromRuntimeRevealDirection(
    kiriview::ActiveNavigationRevealDirection direction)
{
    switch (direction) {
    case kiriview::ActiveNavigationRevealDirection::None:
        return KiriDocumentSession::ActiveNavigationRevealDirection::None;
    case kiriview::ActiveNavigationRevealDirection::Previous:
        return KiriDocumentSession::ActiveNavigationRevealDirection::Previous;
    case kiriview::ActiveNavigationRevealDirection::Next:
        return KiriDocumentSession::ActiveNavigationRevealDirection::Next;
    }

    return KiriDocumentSession::ActiveNavigationRevealDirection::None;
}

kiriview::ActiveNavigationThumbnailDemandPriority toRuntimeThumbnailDemandPriority(
    KiriDocumentSession::ThumbnailDemandPriority priority)
{
    switch (priority) {
    case KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand:
        return kiriview::ActiveNavigationThumbnailDemandPriority::Visible;
    case KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand:
        return kiriview::ActiveNavigationThumbnailDemandPriority::Nearby;
    }

    return kiriview::ActiveNavigationThumbnailDemandPriority::Nearby;
}

std::optional<kiriview::ActiveNavigationThumbnailDemandSnapshot> thumbnailDemandSnapshot(
    quint64 navigationGeneration, const QVariantList& values)
{
    if (navigationGeneration == 0) {
        return std::nullopt;
    }
    kiriview::ActiveNavigationThumbnailDemandSnapshot snapshot;
    snapshot.navigationGeneration = navigationGeneration;
    snapshot.demands.reserve(values.size());
    for (const QVariant& value : values) {
        if (!value.canConvert<QVariantMap>()) {
            return std::nullopt;
        }
        const QVariantMap map = value.toMap();
        bool numberValid = false;
        bool edgeValid = false;
        bool priorityValid = false;
        bool generationValid = false;
        const int number = map.value(QStringLiteral("number")).toInt(&numberValid);
        const int physicalMaxEdge = map.value(QStringLiteral("physicalMaxEdge")).toInt(&edgeValid);
        const int priorityValue = map.value(QStringLiteral("priority")).toInt(&priorityValid);
        const quint64 entryGeneration
            = map.value(QStringLiteral("navigationGeneration")).toULongLong(&generationValid);
        const QUrl url = map.value(QStringLiteral("url")).toUrl();
        const auto bucket
            = kiriview::activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(physicalMaxEdge);
        const auto priority
            = static_cast<KiriDocumentSession::ThumbnailDemandPriority>(priorityValue);
        if (!numberValid || number <= 0 || !edgeValid
            || bucket == kiriview::ActiveNavigationThumbnailDemandBucket::None || !priorityValid
            || (priority != KiriDocumentSession::ThumbnailDemandPriority::VisibleThumbnailDemand
                && priority != KiriDocumentSession::ThumbnailDemandPriority::NearbyThumbnailDemand)
            || !generationValid || entryGeneration != navigationGeneration || !url.isValid()
            || url.isEmpty()) {
            return std::nullopt;
        }
        snapshot.demands.push_back(
            { number, url, bucket, toRuntimeThumbnailDemandPriority(priority) });
    }
    return snapshot;
}

template <typename Document>
kiriview::DocumentSessionSnapshotConnector documentSnapshotConnector(
    Document& document, void (Document::*signal)())
{
    return [&document, signal](
               QObject* context, const kiriview::DocumentSessionSnapshotChangeHandler& handler) {
        std::vector<QMetaObject::Connection> connections;
        connections.push_back(QObject::connect(&document, signal, context, [handler]() {
            if (handler) {
                handler();
            }
        }));
        return connections;
    };
}

kiriview::ImageDocumentRuntimeDependencyOverrides imageDocumentDependenciesWithPredecodeFinder(
    kiriview::ImageDocumentRuntimeDependencyOverrides dependencies,
    kiriview::ExternalPredecodedImageFinder predecodedImageFinder)
{
    dependencies.externalPredecodedImageFinder = std::move(predecodedImageFinder);
    dependencies.ordinaryDirectMediaPredecodeEnabled = false;
    return dependencies;
}

void inheritMissingDirectMediaPredecodeDependencies(
    kiriview::KiriDocumentSessionDependencies& dependencies)
{
    kiriview::MediaPredecodeDependencyOverrides& directMediaPredecode
        = dependencies.sessionRuntime.directMediaPredecodeDependencies;
    const kiriview::ImageDocumentRuntimeDependencyOverrides& imageDocument
        = dependencies.imageDocument;

    if (!directMediaPredecode.imageDecode.dataLoader) {
        directMediaPredecode.imageDecode.dataLoader = imageDocument.imageDecode.dataLoader;
    }
    if (!directMediaPredecode.imageDecode.dataDecoder) {
        directMediaPredecode.imageDecode.dataDecoder = imageDocument.imageDecode.dataDecoder;
    }
    if (!directMediaPredecode.powerSaver.monitor) {
        directMediaPredecode.powerSaver.monitor = imageDocument.powerSaver.monitor;
    }
    if (!directMediaPredecode.timerScheduler.currentMonotonicTime) {
        directMediaPredecode.timerScheduler.currentMonotonicTime
            = imageDocument.predecodeTimerScheduler.currentMonotonicTime;
    }
    if (!directMediaPredecode.timerScheduler.singleShotTimer) {
        directMediaPredecode.timerScheduler.singleShotTimer
            = imageDocument.predecodeTimerScheduler.singleShotTimer;
    }
    if (directMediaPredecode.cacheBudgetRequest.predecodeCacheByteBudget <= 0) {
        directMediaPredecode.cacheBudgetRequest.predecodeCacheByteBudget
            = imageDocument.cacheBudgetRequest.predecodeCacheByteBudget;
    }
    if (!directMediaPredecode.systemMemorySnapshot.has_value()) {
        directMediaPredecode.systemMemorySnapshot = imageDocument.systemMemorySnapshot;
    }
}

kiriview::KiriDocumentSessionDependencies documentSessionDependenciesWithComposedDefaults(
    kiriview::KiriDocumentSessionDependencies dependencies)
{
    kiriview::ImageCacheBudgetRequest request
        = kiriview::imageDocumentCacheBudgetRequestWithDefaults(
            dependencies.imageDocument.cacheBudgetRequest);
    if (request.predecodeCacheByteBudget <= 0 || request.displayImageCacheByteBudget <= 0
        || request.thumbnailCacheByteBudget <= 0) {
        const kiriview::SystemMemorySnapshot systemMemory
            = dependencies.imageDocument.systemMemorySnapshot.value_or(
                kiriview::systemMemorySnapshot());
        const kiriview::ImageCacheBudgets cacheBudgets
            = kiriview::resolvedImageCacheBudgets(request, systemMemory);
        request.predecodeCacheByteBudget = cacheBudgets.predecodeCacheByteBudget;
        request.displayImageCacheByteBudget = cacheBudgets.displayImageCacheByteBudget;
        request.thumbnailCacheByteBudget = cacheBudgets.thumbnailCacheByteBudget;
    }
    dependencies.imageDocument.cacheBudgetRequest = request;
    kiriview::configureSharedThumbnailImageStoreByteBudget(request.thumbnailCacheByteBudget);
    inheritMissingDirectMediaPredecodeDependencies(dependencies);
    return dependencies;
}

kiriview::DocumentSessionPublicSignalOperations publicSignalOperations(KiriDocumentSession& session)
{
    kiriview::DocumentSessionPublicSignalOperations operations;
    operations.publicProjectionRevisionChanged
        = [&session]() { Q_EMIT session.publicProjectionRevisionChanged(); };
    operations.sourceUrlChanged = [&session]() { Q_EMIT session.sourceUrlChanged(); };
    operations.documentKindChanged = [&session]() { Q_EMIT session.documentKindChanged(); };
    operations.errorStringChanged = [&session]() { Q_EMIT session.errorStringChanged(); };
    operations.windowTitleSubjectChanged
        = [&session]() { Q_EMIT session.windowTitleSubjectChanged(); };
    operations.displayedFileDeletionAvailabilityChanged
        = [&session]() { Q_EMIT session.displayedFileDeletionAvailabilityChanged(); };
    operations.displayedMediaOpenWithAvailabilityChanged
        = [&session]() { Q_EMIT session.displayedMediaOpenWithAvailabilityChanged(); };
    operations.fileDeletionInProgressChanged
        = [&session]() { Q_EMIT session.fileDeletionInProgressChanged(); };
    operations.activeZoomReadoutChanged
        = [&session]() { Q_EMIT session.activeZoomReadoutChanged(); };
    operations.activeMediaReadinessChanged
        = [&session]() { Q_EMIT session.activeMediaReadinessChanged(); };
    operations.activeNavigationChanged = [&session]() { Q_EMIT session.activeNavigationChanged(); };
    operations.activeNavigationRevealIntentChanged
        = [&session]() { Q_EMIT session.activeNavigationRevealIntentChanged(); };
    operations.activeNavigationRevealDirectionChanged
        = [&session]() { Q_EMIT session.activeNavigationRevealDirectionChanged(); };
    return operations;
}
}

kiriview::DocumentSessionImageDocumentSnapshotPort KiriDocumentSession::imageDocumentSnapshotPort(
    KiriImageDocument& document)
{
    return kiriview::DocumentSessionImageDocumentSnapshotPort {
        [&document]() { return imageDocumentSessionSnapshot(document); },
        documentSnapshotConnector(document, &KiriImageDocument::documentSessionSnapshotChanged),
    };
}

kiriview::DocumentSessionImageDocumentCommandPort KiriDocumentSession::imageDocumentCommandPort(
    KiriImageDocument& document)
{
    return kiriview::DocumentSessionImageDocumentCommandPort {
        { [&document]() { document.setSourceUrl(QUrl()); },
            [&document](const kiriview::OpenedCollectionScopeLocation& openedCollectionScope,
                const QUrl& videoUrl) {
                return document.loadOpenedCollectionVideoPlaybackDevice(
                    openedCollectionScope, videoUrl);
            },
            [&document](
                const kiriview::ResolvedNavigationSource& source) { document.setSource(source); } },
        { [&document]() { document.openPreviousPage(); },
            [&document]() { document.openNextPage(); },
            [&document](int pageNumber) { document.openImageAtPage(pageNumber); } },
        { [&document](kiriview::FileDeletionMode mode) {
            document.deleteDisplayedFile(toImageDocumentDeletionMode(mode));
        } },
    };
}

kiriview::DocumentSessionVideoDocumentSnapshotPort KiriDocumentSession::videoDocumentSnapshotPort(
    KiriVideoDocument& document)
{
    return kiriview::DocumentSessionVideoDocumentSnapshotPort {
        [&document]() { return videoDocumentSessionSnapshot(document); },
        documentSnapshotConnector(document, &KiriVideoDocument::documentSessionSnapshotChanged),
    };
}

kiriview::DocumentSessionVideoDocumentCommandPort KiriDocumentSession::videoDocumentCommandPort(
    KiriVideoDocument& document)
{
    return kiriview::DocumentSessionVideoDocumentCommandPort {
        { [&document]() { document.setSourceUrl(QUrl()); },
            [&document](const kiriview::ResolvedNavigationSource& source) {
                document.setSourceUrl(source.requestedUrl());
            },
            [&document](const QUrl& url, kiriview::VideoPlaybackSourceDevice sourceDevice) {
                document.setSourceDevice(url, std::move(sourceDevice));
            } },
        { [&document]() { document.stop(); } },
        { [&document]() { return document.videoOutput(); },
            [&document](QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect) {
                document.setVideoOutputAttachment(videoOutput, contentRect, sourceRect);
            } },
    };
}

KiriDocumentSession::KiriDocumentSession(QObject* parent)
    : KiriDocumentSession(kiriview::KiriDocumentSessionDependencies {}, parent)
{
}

KiriDocumentSession::KiriDocumentSession(
    kiriview::KiriDocumentSessionDependencies dependencies, QObject* parent)
    : KiriDocumentSession(documentSessionDependenciesWithComposedDefaults(std::move(dependencies)),
          ResolvedDependenciesTag {}, parent)
{
}

KiriDocumentSession::KiriDocumentSession(kiriview::KiriDocumentSessionDependencies dependencies,
    ResolvedDependenciesTag, QObject* parent)
    : QObject(parent)
    , m_imageDocument(new KiriImageDocument(
          imageDocumentDependenciesWithPredecodeFinder(dependencies.imageDocument,
              [this](const QUrl& url) {
                  return m_runtime != nullptr ? m_runtime->findPredecodedImage(url)
                                              : std::optional<kiriview::PredecodedImage>();
              }),
          [this](const QString& message) { Q_EMIT fileDeletionFailed(message); }, this))
    , m_videoDocument(
          new KiriVideoDocument(std::move(dependencies.videoPlaybackControlTimerScheduler),
              std::move(dependencies.videoMediaBackendFactory), this))
{
    dependencies.sessionRuntime.fileDeletionFailed
        = [this](const QString& message) { Q_EMIT fileDeletionFailed(message); };
    m_runtime = std::make_unique<kiriview::DocumentSessionRuntime>(
        this, imageDocumentSnapshotPort(*m_imageDocument),
        imageDocumentCommandPort(*m_imageDocument), videoDocumentSnapshotPort(*m_videoDocument),
        videoDocumentCommandPort(*m_videoDocument),
        [this](const std::vector<kiriview::DocumentSessionChange>& changes) {
            handleSessionChanges(changes);
        },
        std::move(dependencies.sessionRuntime));
    m_mediaInformation = new KiriMediaInformation(*this, this);
}

KiriDocumentSession::~KiriDocumentSession()
{
    const QSignalBlocker sessionSignals(this);
    const QSignalBlocker imageDocumentSignals(m_imageDocument);
    const QSignalBlocker videoDocumentSignals(m_videoDocument);
    m_videoDocument->runWithPublicSignalsSuppressed([this]() { m_runtime.reset(); });
}

QUrl KiriDocumentSession::sourceUrl() const { return m_runtime->sourceUrl(); }

void KiriDocumentSession::setSourceUrl(const QUrl& sourceUrl)
{
    m_runtime->setSourceUrl(sourceUrl);
}

quint64 KiriDocumentSession::publicProjectionRevision() const
{
    return m_runtime->publicProjectionRevision();
}

KiriDocumentSession::DocumentKind KiriDocumentSession::documentKind() const
{
    return fromRuntimeKind(m_runtime->documentKind());
}

QString KiriDocumentSession::errorString() const { return m_runtime->errorString(); }

QString KiriDocumentSession::windowTitleSubject() const { return m_runtime->windowTitleSubject(); }

QStringList KiriDocumentSession::openDialogNameFilters() const
{
    return kiriview::ordinaryMediaOpenDialogNameFilters();
}

bool KiriDocumentSession::displayedFileDeletionAvailable() const
{
    return m_runtime->displayedFileDeletionAvailable();
}

bool KiriDocumentSession::displayedMediaOpenWithAvailable() const
{
    return m_runtime->displayedMediaOpenWithAvailable();
}

bool KiriDocumentSession::fileDeletionInProgress() const
{
    return m_runtime->fileDeletionInProgress();
}

bool KiriDocumentSession::activeZoomPercentAvailable() const
{
    return m_runtime->activeZoomPercentAvailable();
}

bool KiriDocumentSession::activeZoomPercentKnown() const
{
    return m_runtime->activeZoomPercentKnown();
}

double KiriDocumentSession::activeZoomPercent() const { return m_runtime->activeZoomPercent(); }

bool KiriDocumentSession::activeZoomEditable() const { return m_runtime->activeZoomEditable(); }

bool KiriDocumentSession::activeImageReady() const { return m_runtime->activeImageReady(); }

bool KiriDocumentSession::activeImageReplacementFallbackAvailable() const
{
    return m_runtime->activeImageReplacementFallbackAvailable();
}

bool KiriDocumentSession::activeImageUnsupportedOpenedCollectionVideo() const
{
    return m_runtime->activeImageUnsupportedOpenedCollectionVideo();
}

bool KiriDocumentSession::activeImageOpenedCollectionScopeActive() const
{
    return m_runtime->activeImageOpenedCollectionScopeActive();
}

bool KiriDocumentSession::activeImageRightToLeftReadingActive() const
{
    return m_runtime->activeImageRightToLeftReadingActive();
}

bool KiriDocumentSession::activeVideoReady() const { return m_runtime->activeVideoReady(); }

bool KiriDocumentSession::activeVideoControlsReady() const
{
    return m_runtime->activeVideoControlsReady();
}

bool KiriDocumentSession::activeNavigationAvailable() const
{
    return m_runtime->activeNavigationAvailable();
}

bool KiriDocumentSession::activeNavigationKnown() const
{
    return m_runtime->activeNavigationKnown();
}

bool KiriDocumentSession::activeNavigationEditable() const
{
    return m_runtime->activeNavigationEditable();
}

bool KiriDocumentSession::activeNavigationHasTargets() const
{
    return m_runtime->activeNavigationHasTargets();
}

bool KiriDocumentSession::activeNavigationDispatchAvailable() const
{
    return m_runtime->activeNavigationDispatchAvailable();
}

int KiriDocumentSession::activeNavigationCurrentNumber() const
{
    return m_runtime->activeNavigationCurrentNumber();
}

int KiriDocumentSession::activeNavigationCount() const
{
    return m_runtime->activeNavigationCount();
}

bool KiriDocumentSession::canOpenPreviousActiveNavigation() const
{
    return m_runtime->canOpenPreviousActiveNavigation();
}

bool KiriDocumentSession::canOpenNextActiveNavigation() const
{
    return m_runtime->canOpenNextActiveNavigation();
}

bool KiriDocumentSession::atKnownFirstActiveNavigation() const
{
    return m_runtime->atKnownFirstActiveNavigation();
}

bool KiriDocumentSession::atKnownLastActiveNavigation() const
{
    return m_runtime->atKnownLastActiveNavigation();
}

bool KiriDocumentSession::directMediaNavigationBoundaryActive() const
{
    return m_runtime->directMediaNavigationBoundaryActive();
}

KiriDocumentSession::ActiveNavigationBoundaryScope
KiriDocumentSession::activeNavigationBoundaryScope() const
{
    return fromRuntimeBoundaryScope(m_runtime->activeNavigationBoundaryScope());
}

KiriDocumentSession::ActiveNavigationRevealIntent
KiriDocumentSession::activeNavigationRevealIntent() const
{
    return fromRuntimeRevealIntent(m_runtime->activeNavigationRevealIntent());
}

KiriDocumentSession::ActiveNavigationRevealDirection
KiriDocumentSession::activeNavigationRevealDirection() const
{
    return fromRuntimeRevealDirection(m_runtime->activeNavigationRevealDirection());
}

QAbstractListModel* KiriDocumentSession::activeNavigationThumbnailModel() const
{
    return m_runtime->activeNavigationThumbnailModel();
}

KiriMediaInformation* KiriDocumentSession::mediaInformation() const { return m_mediaInformation; }

const kiriview::MediaInformationProjectionSnapshot&
KiriDocumentSession::mediaInformationSnapshot() const
{
    return m_runtime->mediaInformationSnapshot();
}

const kiriview::DocumentSessionActionStateSnapshot& KiriDocumentSession::actionStateSnapshot() const
{
    return m_runtime->actionStateSnapshot();
}

kiriview::DocumentSessionActionStateSnapshotPort KiriDocumentSession::actionStateSnapshotPort()
{
    QPointer<KiriDocumentSession> session(this);
    return kiriview::DocumentSessionActionStateSnapshotPort {
        [session]() {
            return session == nullptr ? kiriview::DocumentSessionActionStateSnapshot {}
                                      : session->actionStateSnapshot();
        },
        [session](QObject* context, kiriview::DocumentSessionSnapshotChangeHandler refresh) {
            if (session == nullptr) {
                return std::vector<QMetaObject::Connection> {};
            }

            return std::vector<QMetaObject::Connection> { QObject::connect(session,
                &KiriDocumentSession::publicProjectionRevisionChanged, context,
                [refresh = std::move(refresh)]() { refresh(); }) };
        },
    };
}

const kiriview::DocumentSessionActionAvailabilityFacts&
KiriDocumentSession::actionAvailabilityFacts() const
{
    return m_runtime->actionAvailabilityFacts();
}

KiriImageDocument* KiriDocumentSession::imageDocument() const { return m_imageDocument; }

KiriVideoDocument* KiriDocumentSession::videoDocument() const { return m_videoDocument; }

void KiriDocumentSession::openPreviousActiveNavigation()
{
    m_runtime->openPreviousActiveNavigation();
}

void KiriDocumentSession::openNextActiveNavigation() { m_runtime->openNextActiveNavigation(); }

void KiriDocumentSession::openFirstActiveNavigation() { m_runtime->openFirstActiveNavigation(); }

void KiriDocumentSession::openLastActiveNavigation() { m_runtime->openLastActiveNavigation(); }

void KiriDocumentSession::openActiveNavigationAtNumber(int number)
{
    m_runtime->openActiveNavigationAtNumber(number);
}

void KiriDocumentSession::openActiveNavigationThumbnailAtNumber(int number)
{
    m_runtime->openActiveNavigationThumbnailAtNumber(number);
}

KiriDocumentSession::ActiveNavigationRequestResult
KiriDocumentSession::requestPreviousActiveNavigation()
{
    return fromRuntimeRequestOutcome(m_runtime->requestPreviousActiveNavigation());
}

KiriDocumentSession::ActiveNavigationRequestResult
KiriDocumentSession::requestNextActiveNavigation()
{
    return fromRuntimeRequestOutcome(m_runtime->requestNextActiveNavigation());
}

QString KiriDocumentSession::requestPreviousActiveNavigationBoundaryText()
{
    const auto boundaryScope = m_runtime->activeNavigationBoundaryScope();
    const auto outcome = m_runtime->requestPreviousActiveNavigation();
    return kiriview::activeNavigationBoundaryFeedbackText(boundaryScope, outcome);
}

QString KiriDocumentSession::requestNextActiveNavigationBoundaryText()
{
    const auto boundaryScope = m_runtime->activeNavigationBoundaryScope();
    const auto outcome = m_runtime->requestNextActiveNavigation();
    return kiriview::activeNavigationBoundaryFeedbackText(boundaryScope, outcome);
}

bool KiriDocumentSession::replaceActiveNavigationThumbnailDemandSnapshot(
    quint64 navigationGeneration, const QVariantList& demands)
{
    auto snapshot = thumbnailDemandSnapshot(navigationGeneration, demands);
    return snapshot.has_value()
        && m_runtime->replaceActiveNavigationThumbnailDemandSnapshot(*snapshot);
}

QString KiriDocumentSession::nextVideoOutputSurfaceClaimToken()
{
    return m_runtime->nextVideoOutputSurfaceClaimToken();
}

bool KiriDocumentSession::reportVideoOutputSurfaceClaim(const QString& claimToken,
    quint64 projectionRevision, QObject* surfaceOwner, QObject* videoOutput, bool active,
    QRectF contentRect, QRectF sourceRect)
{
    return m_runtime->reportVideoOutputSurfaceClaim(
        claimToken, projectionRevision, surfaceOwner, videoOutput, active, contentRect, sourceRect);
}

void KiriDocumentSession::deleteDisplayedFile(DeletionMode mode)
{
    m_runtime->deleteDisplayedFile(toFileDeletionMode(mode));
}

void KiriDocumentSession::openCurrentMediaWith()
{
    m_runtime->openCurrentMediaWith(
        [this](kiriview::MediaOpenWithResult result, const kiriview::KioOperationFailure& failure) {
            if (result == kiriview::MediaOpenWithResult::Failed) {
                Q_EMIT openWithFailed(failure.userMessage);
            }
        });
}

void KiriDocumentSession::handleSessionChanges(
    const std::vector<kiriview::DocumentSessionChange>& changes)
{
    enqueuePublicSignals(kiriview::documentSessionPublicSignalsForChanges(changes));
}

void KiriDocumentSession::enqueuePublicSignals(
    std::vector<kiriview::DocumentSessionPublicSignal> signals)
{
    m_pendingPublicSignals = mergePublicSignals(std::move(signals), m_pendingPublicSignals);
    if (m_publicSignalDispatchActive) {
        return;
    }
    drainPublicSignals();
}

void KiriDocumentSession::drainPublicSignals()
{
    const QPointer<KiriDocumentSession> owner(this);
    m_publicSignalDispatchActive = true;
    const kiriview::DocumentSessionPublicSignalEmitter emitter(publicSignalOperations(*this));

    while (!m_pendingPublicSignals.empty()) {
        const kiriview::DocumentSessionPublicSignal signal = m_pendingPublicSignals.front();
        m_pendingPublicSignals.erase(m_pendingPublicSignals.begin());
        emitter.emitSignal(signal);
        if (owner.isNull()) {
            return;
        }
    }
    m_publicSignalDispatchActive = false;
}
