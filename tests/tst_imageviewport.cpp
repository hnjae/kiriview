#include "imageviewport.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QMetaEnum>
#include <QtCore/QPointF>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QSGSimpleRectNode>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <limits>
#include <memory>
#include <type_traits>

static_assert(std::is_abstract_v<ImageSequenceProviderAdapter>,
    "ImageSequenceProviderAdapter must remain an abstract public extension-point base");

class ImageViewportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructsAsQuickItem();
    void doesNotExposeSourceProperty();
    void unsupportedSequencePropertyWritesPreserveState();
    void qmlUnsupportedSequenceAssignmentsPreserveState();
    void exposesDocumentedQmlSurface();
    void hasDocumentedDefaultState();
    void qmlImportsDocumentedSurface();
    void imageSequenceIsNotQmlCreatable();
    void imageSequenceProviderAdapterIsNotQmlCreatable();
    void exposesTypedSequenceFactorySurface();
    void factoryResultDiagnosticsArePublicSafe();
    void exposesImageSequenceLimits();
    void imageFrameRetainsImmutablePayload();
    void stillImageSequenceRetainsFactoryPayload();
    void timedFrameListSequenceRetainsFactoryPayloads();
    void commandsWithoutRequestAreIgnoredDiagnostics();
    void resetViewWithoutRequestClearsTransformAndCommandDiagnostic();
    void stillImageSequenceAssignmentPublishesReadyState();
    void nullSequenceAssignmentClearsDisplayObservations();
    void clearPreservesPresentationState();
    void clearReadyDisplayEmitsGeometryStateChanged();
    void clearNonPresentableDisplayDoesNotEmitGeometryStateChanged();
    void stillImageReadyReplacementIncrementsDisplayRevision();
    void stillImageReplacementPreservesPresentationState();
    void stillImageCommandsPreserveOrReplaceDocumentedState();
    void stillImageFillModesAndMirroringUseDocumentedGeometry();
    void stillImageMirroredCoverUsesMirroredVisibleImageRect();
    void stillImageAssignmentWaitsForPositiveGeometry();
    void stillImageFactoryRejectsPublishedLimitViolations();
    void stillImageFactoryRejectsInvalidPayloadByteSize();
    void timedFrameListBuilderValidatesEntries();
    void timedFrameListRejectsPublishedDurationLimits();
    void timedFrameListRejectsPublishedFrameCountLimit();
    void timedFrameListAllowsCumulativePayloadsAbovePerFrameLimit();
    void timedFrameListClearDiagnosticOnlyPreservesCountNotification();
    void timedFrameListAssignmentPublishesInitialTimedState();
    void timedFrameListSeekCommandsSelectDocumentedTargets();
    void timedFrameListSeekWhilePlayingWaitsForRenderCommit();
    void timedFrameListSeekWithUnchangedGeometryDoesNotNotifyGeometryState();
    void timedFrameListPlaybackCommandsUpdatePhase();
    void timedFrameListBackgroundOnlyChangesPreserveRequestAndPlayback();
    void timedFrameListPlaybackAdvancesDeterministically();
    void timedFrameListPlaybackWithUnchangedGeometryDoesNotNotifyGeometryState();
    void timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay();
    void timedFrameListStopAfterPauseWhileRenderWaitingRestoresPreviousDisplay();
    void timedFrameListLoopingPlaybackWrapsToFirstFrame();
    void replacementRetainsPreviousDisplayWhileWaitingForGeometry();
    void providerFactoryRejectsBaseAdapterWithoutSessionFactory();
    void providerFactoryRejectsContradictoryConstructionFacts();
    void providerFactoryRejectsPublishedKnownMetadataLimits();
    void providerSequenceOpensSessionAfterAdapterDestruction();
    void providerSessionOpenFailureKeepsReplacementObservable();
    void reassigningSameProviderSequenceStartsNewGeneration();
    void providerSessionClosesWhenViewportIsDestroyed();
    void providerClearCancelsActiveFrameRequestBeforeClose();
    void providerClearIgnoresCancelledMetadataAcknowledgement();
    void providerClearIgnoresCancelledFrameAcknowledgement();
    void providerResultsAreQueuedFromSessionEntryPoint();
    void providerFrameResultsAreQueuedFromSessionEntryPoint();
    void providerTerminalResultsAreQueuedFromSessionEntryPoint();
    void providerConstructionMetadataSelectsInitialFrameRequest();
    void providerFixedDurationConstructionMetadataSelectsInitialFrameRequest();
    void providerPartialConstructionMetadataWaitsForRuntimeMetadata();
    void providerPartialConstructionMetadataBindsAcceptedSeekAfterRuntimeMetadata();
    void providerPartialStillConstructionMetadataConstrainsCommands();
    void providerPartialConstructionMetadataRejectsSeeksPastKnownBounds();
    void providerDeclaredCapabilityProjectsBeforeMetadata();
    void providerKnownCapabilityProjectsBeforeMetadata();
    void providerDeclaredCapabilityContradictionRejectsMetadata();
    void providerDeclaredNoPlaybackRejectsPlayBeforeMetadata();
    void providerKnownNoPlaybackRejectsPlayBeforeMetadata();
    void providerDeclaredNoFrameSeekRejectsSeekBeforeMetadata();
    void providerDeclaredNoPositionSeekRejectsPositionSeekBeforeMetadata();
    void providerMetadataRejectsNonFiniteLogicalSize();
    void providerMetadataRejectsHugeFiniteLogicalSize();
    void providerMetadataRejectsPublishedFrameCountLimit();
    void providerMetadataRejectsPublishedDurationLimits();
    void providerStillMetadataSelectsInitialFrameRequest();
    void providerTimedMetadataSelectsInitialFrameRequest();
    void providerFixedDurationMetadataSelectsInitialFrameRequest();
    void providerProgressResultsAreAdvisory();
    void providerPositiveResizeWhileMetadataWaitingKeepsProviderWaiting();
    void providerMetadataReadySealsMetadataToken();
    void providerFrameSeekBeforeMetadataResolvesAfterMetadata();
    void providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata();
    void providerPositionSeekBeforeMetadataResolvesAfterMetadata();
    void providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable();
    void providerPlaybackBeforeStillMetadataKeepsGenerationSeekable();
    void providerStillFrameReadyCommitsDisplay();
    void providerTimedFrameReadyCommitsTimedDisplay();
    void providerTimedFrameEnvelopeMismatchRejectsPayload();
    void providerTimedFramePayloadLimitReportsUnsupportedPayload();
    void providerFrameRejectsInvalidPayloadByteSize();
    void providerTimedFrameSeekRequestsSelectedFrame();
    void providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify();
    void providerTimedFrameCommitWithUnchangedGeometryDoesNotNotifyGeometryState();
    void providerTimedFrameSeekCancelsSupersededRequest();
    void providerTimedPositionSeekRequestsResolvedFrame();
    void providerTimedPlaybackCommandsUpdatePhase();
    void providerTimedPlaybackAdvancesDeterministically();
    void providerTimedPlaybackFrameReadyWaitsForRenderCommit();
    void providerTimedPlaybackEndOfSequenceRequestsFinalFrame();
    void providerTimedPlaybackEndOfSequenceDoesNotPromoteRetainedPreviousGeneration();
    void providerTimedPlaybackEndOfSequenceFinalUsesPlaybackEntryPoint();
    void providerTimedLoopingPlaybackWrapsToFirstFrame();
    void providerMetadataEndOfSequenceReportsProtocolViolation();
    void providerFrameEndOfSequenceReportsProtocolViolation();
    void providerTimedPlaybackAdvancementUsesPlaybackEntryPoint();
    void providerTimedPlaybackStopsOnFrameFailure();
    void providerTimedPlaybackWaitsForMetadata();
    void providerTimedPlaybackAfterMetadataUsesPlaybackEntryPoint();
    void providerTimedPausedPlaybackAfterMetadataUsesPlaybackEntryPoint();
    void providerTimedStopAfterPausedMetadataWaitRestoresInitialRequest();
    void providerTimedStopWhileWaitingForMetadataRestoresInitialRequest();
    void providerTimedStopWhileWaitingForMetadataRestoresExplicitSeek();
    void providerTimedStopAfterMetadataPlaybackCreatesNonPlaybackRequest();
    void providerTimedStopAfterMetadataPlaybackRestoresSupersededExplicitSeek();
    void providerTimedStopCancelsPlaybackRequest();
    void providerTimedStopSupersedesPlaybackRequest();
    void providerTimedSeekWhilePlayingWaitsForFrame();
    void providerMetadataFailureReportsProviderFailure();
    void providerInvalidTerminalTokenAfterMetadataIsIgnored();
    void providerDiagnosticsUseUnicodeScalarLimit();
    void providerDiagnosticsRedactPrivateDetails();
    void providerDiagnosticsArePlainText();
    void providerMetadataFailureStopsPendingPlayback();
    void providerGenerationTerminalFailureRejectsDisplayCommands();
    void providerGenerationTerminalFailureAcceptsControlCommands();
    void providerFrameFailureKeepsGenerationSeekable();
    void providerMetadataUnsupportedReportsUnsupportedRequest();
    void providerFrameUnsupportedKeepsGenerationSeekable();
    void providerFrameUnsupportedRetainsDisplayAndClearsOnSeek();
    void providerMetadataCancellationReportsProviderFailure();
    void providerFrameCancellationReportsProviderFailure();
    void providerFrameCancellationRetainsDisplayAndClearsOnSeek();
    void solidBackgroundCreatesPaintNode();
    void checkerboardBackgroundCreatesPaintNode();
    void stillImageCreatesTexturePaintNode();
    void stillImagePaintFailureReportsRenderFailure();
    void timedFrameListPaintFailureRetainsPreviousDisplay();
    void timedFrameListPlayAfterPaintFailureRestartsDisplayRequest();
    void successfulPaintClearsRenderFailureInterest();
    void coverImageTextureNodeUsesVisibleSourceRect();
    void providerStillFrameCreatesTexturePaintNode();
    void providerStillFrameWaitingForGeometryCreatesTexturePaintNode();
    void providerTimedFramePaintFailureRetainsPreviousDisplay();
    void providerTimedPlayAfterPaintFailureRestartsPlaybackRequest();
    void invalidPresentationEnumValuesAreIgnored();
    void presentationZoomUsesExactValueChanges();
    void presentationChangesWithoutDisplayDoNotNotifyGeometryState();
    void presentationChangesNotifyGeometryState();
};

namespace {

QString componentErrors(const QQmlComponent &component)
{
    QStringList messages;
    for (const QQmlError &error : component.errors()) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

int enumValue(const QMetaObject *metaObject, const char *enumName, const char *key)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

class PaintProbeViewport final : public ImageViewport
{
public:
    using ImageViewport::ImageViewport;

    QSGNode *takePaintNode(QSGNode *oldNode = nullptr)
    {
        return updatePaintNode(oldNode, nullptr);
    }
};

bool commitPaintNode(PaintProbeViewport &item)
{
    QScopedPointer<QSGNode> root(item.takePaintNode());
    return !root.isNull();
}

class CountingProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit CountingProviderSession(const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &lastRequestedFrame,
        const std::shared_ptr<int> &closeCount,
        const std::shared_ptr<int> &playbackRequestCount = {},
        const std::shared_ptr<int> &lastPlaybackFrame = {},
        const std::shared_ptr<int> &lastPlaybackPosition = {},
        const std::shared_ptr<int> &cancelRequestCount = {},
        const std::shared_ptr<quint64> &lastCancelledTokenId = {},
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
        , m_playbackRequestCount(playbackRequestCount)
        , m_lastPlaybackFrame(lastPlaybackFrame)
        , m_lastPlaybackPosition(lastPlaybackPosition)
        , m_cancelRequestCount(cancelRequestCount)
        , m_lastCancelledTokenId(lastCancelledTokenId)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(const ImageSequenceProviderRequestToken &token, int frame) override
    {
        m_lastFrameToken = token;
        *m_lastRequestedFrame = frame;
        ++*m_frameRequestCount;
    }

    void requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int position) override
    {
        if (m_playbackRequestCount) {
            ++*m_playbackRequestCount;
        }
        if (m_lastPlaybackFrame) {
            *m_lastPlaybackFrame = frame;
        }
        if (m_lastPlaybackPosition) {
            *m_lastPlaybackPosition = position;
        }
        ImageSequenceProviderSession::requestPlayback(token, frame, position);
    }

    void cancelRequest(const ImageSequenceProviderRequestToken &token) override
    {
        m_lastCancelledToken = token;
        if (m_cancelRequestCount) {
            ++*m_cancelRequestCount;
        }
        if (m_lastCancelledTokenId) {
            *m_lastCancelledTokenId = token.id();
        }
    }

    void close() override
    {
        ++*m_closeCount;
    }

    ImageSequenceProviderRequestToken lastMetadataToken() const
    {
        return m_lastMetadataToken;
    }

    ImageSequenceProviderRequestToken lastFrameToken() const
    {
        return m_lastFrameToken;
    }

    ImageSequenceProviderRequestToken lastCancelledToken() const
    {
        return m_lastCancelledToken;
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    std::shared_ptr<int> m_playbackRequestCount;
    std::shared_ptr<int> m_lastPlaybackFrame;
    std::shared_ptr<int> m_lastPlaybackPosition;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<quint64> m_lastCancelledTokenId;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
    ImageSequenceProviderRequestToken m_lastCancelledToken;
};

class CountingProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit CountingProviderSessionFactory(const std::shared_ptr<int> &sessionCount,
        const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &lastRequestedFrame,
        const std::shared_ptr<int> &closeCount,
        const std::shared_ptr<int> &playbackRequestCount = {},
        const std::shared_ptr<int> &lastPlaybackFrame = {},
        const std::shared_ptr<int> &lastPlaybackPosition = {},
        const std::shared_ptr<int> &cancelRequestCount = {},
        const std::shared_ptr<quint64> &lastCancelledTokenId = {})
        : m_sessionCount(sessionCount)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
        , m_playbackRequestCount(playbackRequestCount)
        , m_lastPlaybackFrame(lastPlaybackFrame)
        , m_lastPlaybackPosition(lastPlaybackPosition)
        , m_cancelRequestCount(cancelRequestCount)
        , m_lastCancelledTokenId(lastCancelledTokenId)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        ++*m_sessionCount;
        CountingProviderSession *session = new CountingProviderSession(m_metadataRequestCount,
            m_frameRequestCount,
            m_lastRequestedFrame,
            m_closeCount,
            m_playbackRequestCount,
            m_lastPlaybackFrame,
            m_lastPlaybackPosition,
            m_cancelRequestCount,
            m_lastCancelledTokenId,
            parent);
        m_lastSession = session;
        return session;
    }

    CountingProviderSession *lastSession() const
    {
        return m_lastSession;
    }

private:
    std::shared_ptr<int> m_sessionCount;
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    std::shared_ptr<int> m_playbackRequestCount;
    std::shared_ptr<int> m_lastPlaybackFrame;
    std::shared_ptr<int> m_lastPlaybackPosition;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<quint64> m_lastCancelledTokenId;
    QPointer<CountingProviderSession> m_lastSession;
};

class CountingProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit CountingProviderAdapter(std::shared_ptr<ImageSequenceProviderSessionFactory> factory,
        ImageSequenceProviderMetadata knownMetadata = {},
        CapabilitySupport timedPlaybackSupport = CapabilitySupport::Unavailable,
        CapabilitySupport frameSeekSupport = CapabilitySupport::Unavailable,
        CapabilitySupport positionSeekSupport = CapabilitySupport::Unavailable,
        QObject *parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
        , m_knownMetadata(std::move(knownMetadata))
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return m_factory;
    }

    ImageSequenceProviderMetadata knownMetadata() const override
    {
        return m_knownMetadata;
    }

    CapabilitySupport timedPlaybackCapability() const override
    {
        return m_timedPlaybackSupport;
    }

    CapabilitySupport frameSeekCapability() const override
    {
        return m_frameSeekSupport;
    }

    CapabilitySupport positionSeekCapability() const override
    {
        return m_positionSeekSupport;
    }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_factory;
    ImageSequenceProviderMetadata m_knownMetadata;
    CapabilitySupport m_timedPlaybackSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_frameSeekSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_positionSeekSupport = CapabilitySupport::Unavailable;
};

class FailingProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit FailingProviderSessionFactory(const std::shared_ptr<int> &sessionCount)
        : m_sessionCount(sessionCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *) override
    {
        ++*m_sessionCount;
        return nullptr;
    }

private:
    std::shared_ptr<int> m_sessionCount;
};

class CancellingAcknowledgementProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit CancellingAcknowledgementProviderSession(const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &cancelRequestCount,
        const std::shared_ptr<int> &closeCount,
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(const ImageSequenceProviderRequestToken &, int) override
    {
        ++*m_frameRequestCount;
    }

    void cancelRequest(const ImageSequenceProviderRequestToken &token) override
    {
        ++*m_cancelRequestCount;
        emit providerCancelled(token, QStringLiteral("request cleanup complete"));
    }

    void close() override
    {
        ++*m_closeCount;
    }

    ImageSequenceProviderRequestToken lastMetadataToken() const
    {
        return m_lastMetadataToken;
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
};

class CancellingAcknowledgementProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit CancellingAcknowledgementProviderSessionFactory(const std::shared_ptr<int> &sessionCount,
        const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        const std::shared_ptr<int> &cancelRequestCount,
        const std::shared_ptr<int> &closeCount)
        : m_sessionCount(sessionCount)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        ++*m_sessionCount;
        auto *session = new CancellingAcknowledgementProviderSession(m_metadataRequestCount,
            m_frameRequestCount,
            m_cancelRequestCount,
            m_closeCount,
            parent);
        m_lastSession = session;
        return session;
    }

    CancellingAcknowledgementProviderSession *lastSession() const
    {
        return m_lastSession;
    }

private:
    std::shared_ptr<int> m_sessionCount;
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    QPointer<CancellingAcknowledgementProviderSession> m_lastSession;
};

class NullSessionFactoryProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit NullSessionFactoryProviderAdapter(QObject *parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return {};
    }
};

class SynchronousMetadataProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousMetadataProviderSession(const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount,
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        ++*m_metadataRequestCount;
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    }

    void requestFrame(const ImageSequenceProviderRequestToken &, int) override
    {
        ++*m_frameRequestCount;
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousMetadataProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousMetadataProviderSessionFactory(const std::shared_ptr<int> &metadataRequestCount,
        const std::shared_ptr<int> &frameRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        return new SynchronousMetadataProviderSession(m_metadataRequestCount, m_frameRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousFrameProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousFrameProviderSession(const std::shared_ptr<int> &frameRequestCount,
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_frameRequestCount(frameRequestCount)
    {
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        m_frame = std::make_unique<ImageFrame>(image);
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &) override
    {
        QFAIL("complete construction metadata should not request runtime metadata");
    }

    void requestFrame(const ImageSequenceProviderRequestToken &token, int) override
    {
        ++*m_frameRequestCount;
        emit frameReady(token, m_frame.get());
    }

private:
    std::shared_ptr<int> m_frameRequestCount;
    std::unique_ptr<ImageFrame> m_frame;
};

class SynchronousFrameProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousFrameProviderSessionFactory(const std::shared_ptr<int> &frameRequestCount)
        : m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        return new SynchronousFrameProviderSession(m_frameRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousFailureProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousFailureProviderSession(const std::shared_ptr<int> &metadataRequestCount,
        QObject *parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken &token) override
    {
        ++*m_metadataRequestCount;
        emit providerFailed(token, QStringLiteral("metadata failed synchronously"));
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousFailureProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousFailureProviderSessionFactory(const std::shared_ptr<int> &metadataRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
    {
    }

    ImageSequenceProviderSession *createSession(QObject *parent) override
    {
        return new SynchronousFailureProviderSession(m_metadataRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

void drainQueuedProviderResults()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

void emitTimedProviderFrameReady(CountingProviderSession *session,
    const ImageSequenceProviderRequestToken &token,
    ImageFrame *frame,
    int frameIndex,
    int frameStartPosition)
{
    emit session->frameReady(token,
        frame,
        ImageSequenceProviderFrameMetadata::timedFrame(frameIndex, frameStartPosition));
    drainQueuedProviderResults();
}

void emitTimedProviderFrameReady(CountingProviderSession *session, ImageFrame *frame, int frameIndex, int frameStartPosition)
{
    emitTimedProviderFrameReady(session, session->lastFrameToken(), frame, frameIndex, frameStartPosition);
}

}

void ImageViewportTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

void ImageViewportTest::doesNotExposeSourceProperty()
{
    ImageViewport item;

    QCOMPARE(item.metaObject()->indexOfProperty("source"), -1);
}

void ImageViewportTest::unsupportedSequencePropertyWritesPreserveState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();
    QSignalSpy sequenceSpy(&item, &ImageViewport::sequenceChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);

    const QList<QVariant> unsupportedValues = {
        QVariant(QStringLiteral("image.png")),
        QVariant(QUrl(QStringLiteral("file:///tmp/image.png"))),
        QVariant(QByteArray("not image data")),
        QVariantMap{{QStringLiteral("url"), QStringLiteral("image.png")}},
        QVariant::fromValue<QObject *>(&adapter),
    };

    for (const QVariant &value : unsupportedValues) {
        QCOMPARE(item.setProperty("sequence", value), false);
        QCOMPARE(item.sequence(), result->sequence());
        QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
        QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
        QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
        QCOMPARE(item.property("requestedFrame").toInt(), 0);
        QCOMPARE(item.property("displayedFrame").toInt(), 0);
        QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
        QCOMPARE(item.property("displayRevision").toUInt(), displayRevision);
    }

    QCOMPARE(sequenceSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(*sessionCount, 0);
}

void ImageViewportTest::qmlUnsupportedSequenceAssignmentsPreserveState()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    id: viewport
    QtObject { id: rawObject }
    property bool stringAssignmentPreserved: false
    property bool urlAssignmentPreserved: false
    property bool jsObjectAssignmentPreserved: false
    property bool objectAssignmentPreserved: false

    Component.onCompleted: {
        try {
            sequence = "image.png"
        } catch (error) {
        }
        stringAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = Qt.resolvedUrl("image.png")
        } catch (error) {
        }
        urlAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = ({ url: "image.png" })
        } catch (error) {
        }
        jsObjectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
        try {
            sequence = rawObject
        } catch (error) {
        }
        objectAssignmentPreserved = sequence === null
            && requestStatus === ImageViewport.RequestStatus.NoRequest
            && displayStatus === ImageViewport.DisplayStatus.Empty
            && requestRevision === 0
            && displayRevision === 0
            && errorString === ""
    }
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    QCOMPARE(object->property("stringAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("urlAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("jsObjectAssignmentPreserved").toBool(), true);
    QCOMPARE(object->property("objectAssignmentPreserved").toBool(), true);
}

void ImageViewportTest::exposesDocumentedQmlSurface()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    const QList<QByteArray> properties = {
        "sequence",
        "requestStatus",
        "requestReason",
        "commandReason",
        "displayStatus",
        "playbackPhase",
        "displayedFrame",
        "requestedFrame",
        "displayedPosition",
        "requestedPosition",
        "frameCount",
        "totalDuration",
        "frameSeekBounds",
        "positionSeekBounds",
        "timedPlaybackSupport",
        "frameSeekSupport",
        "positionSeekSupport",
        "displayedImageSize",
        "fillMode",
        "horizontalAlignment",
        "verticalAlignment",
        "contentRect",
        "visibleImageRect",
        "displayRevision",
        "requestRevision",
        "commandRevision",
        "errorString",
        "warningString",
        "zoom",
        "pan",
        "smoothing",
        "mipmap",
        "mirrorHorizontally",
        "mirrorVertically",
        "backgroundMode",
        "backgroundColor",
        "looping",
    };

    for (const QByteArray &property : properties) {
        QVERIFY2(metaObject->indexOfProperty(property.constData()) >= 0, property.constData());
    }

    const QList<QByteArray> enumerators = {
        "RequestStatus",
        "RequestReason",
        "CommandReason",
        "DisplayStatus",
        "PlaybackPhase",
        "TriState",
        "CommandOutcome",
        "FillMode",
        "HorizontalAlignment",
        "VerticalAlignment",
        "BackgroundMode",
    };

    for (const QByteArray &enumerator : enumerators) {
        QVERIFY2(metaObject->indexOfEnumerator(enumerator.constData()) >= 0, enumerator.constData());
    }

    const QList<QByteArray> methods = {
        "play()",
        "pause()",
        "stop()",
        "seek(int)",
        "seekToPosition(int)",
        "clear()",
        "resetView()",
        "itemToImage(double,double)",
        "imageToItem(double,double)",
        "containsVisibleImagePoint(double,double)",
    };

    for (const QByteArray &method : methods) {
        QVERIFY2(metaObject->indexOfMethod(QMetaObject::normalizedSignature(method.constData())) >= 0, method.constData());
    }
}

void ImageViewportTest::hasDocumentedDefaultState()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("commandRevision").toUInt(), 0U);
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(item.property("fillMode").toInt(), enumValue(metaObject, "FillMode", "Contain"));
    QCOMPARE(item.property("horizontalAlignment").toInt(), enumValue(metaObject, "HorizontalAlignment", "AlignHCenter"));
    QCOMPARE(item.property("verticalAlignment").toInt(), enumValue(metaObject, "VerticalAlignment", "AlignVCenter"));
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("smoothing").toBool(), true);
    QCOMPARE(item.property("mipmap").toBool(), false);
    QCOMPARE(item.property("mirrorHorizontally").toBool(), false);
    QCOMPARE(item.property("mirrorVertically").toBool(), false);
    QCOMPARE(item.property("backgroundMode").toInt(), enumValue(metaObject, "BackgroundMode", "Transparent"));
    QCOMPARE(item.property("backgroundColor").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(item.property("looping").toBool(), false);
}

void ImageViewportTest::qmlImportsDocumentedSurface()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import ImageViewport 1.0

ImageViewport {
    property int noRequest: ImageViewport.RequestStatus.NoRequest
    property int loading: ImageViewport.RequestStatus.Loading
    property int retained: ImageViewport.DisplayStatus.Retained
    property int waiting: ImageViewport.PlaybackPhase.Waiting
    property int accepted: ImageViewport.CommandOutcome.Accepted
    property int unsupported: ImageViewport.CommandOutcome.Unsupported
    property int invalid: ImageViewport.CommandOutcome.Invalid
    property int ignoredNoRequest: ImageViewport.CommandOutcome.IgnoredNoRequest
    property int cover: ImageViewport.FillMode.Cover
    property int center: ImageViewport.FillMode.Center
    property bool factoryReturnsNull: ImageSequenceFactory.fromFrame(null).sequence === null
    property bool mappingInvalid: itemToImage(1, 1).valid === false
    property bool mappingHasFlatFields: imageToItem(1, 1).x === 0 && imageToItem(1, 1).y === 0
    property bool limitsAvailable: ImageSequenceLimits.maximumLogicalWidth >= 8192
        && ImageSequenceLimits.maximumLogicalHeight >= 8192
        && ImageSequenceLimits.maximumPixelsPerFrame >= 67108864
        && ImageSequenceLimits.maximumPayloadBytesPerFrame >= 268435456
        && ImageSequenceLimits.maximumTimedListFrameCount >= 10000
        && ImageSequenceLimits.maximumFrameDuration >= 86400000
        && ImageSequenceLimits.maximumTotalSequenceDuration >= 86400000
        && ImageSequenceLimits.maximumDiagnosticStringLength >= 4096
}
)",
        QUrl());

    QVERIFY2(component.isReady(), qPrintable(componentErrors(component)));
    QScopedPointer<QObject> object(component.create());
    QVERIFY2(object, qPrintable(componentErrors(component)));
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(object->property("noRequest").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(object->property("loading").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(object->property("retained").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(object->property("waiting").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(object->property("accepted").toInt(), enumValue(metaObject, "CommandOutcome", "Accepted"));
    QCOMPARE(object->property("unsupported").toInt(), enumValue(metaObject, "CommandOutcome", "Unsupported"));
    QCOMPARE(object->property("invalid").toInt(), enumValue(metaObject, "CommandOutcome", "Invalid"));
    QCOMPARE(object->property("ignoredNoRequest").toInt(), enumValue(metaObject, "CommandOutcome", "IgnoredNoRequest"));
    QCOMPARE(object->property("cover").toInt(), enumValue(metaObject, "FillMode", "Cover"));
    QCOMPARE(object->property("center").toInt(), enumValue(metaObject, "FillMode", "Center"));
    QCOMPARE(object->property("factoryReturnsNull").toBool(), true);
    QCOMPARE(object->property("mappingInvalid").toBool(), true);
    QCOMPARE(object->property("mappingHasFlatFields").toBool(), true);
    QCOMPARE(object->property("limitsAvailable").toBool(), true);
}

void ImageViewportTest::imageSequenceIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequence {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::imageSequenceProviderAdapterIsNotQmlCreatable()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(IMAGEVIEWPORT_QML_IMPORT_PATH));

    QQmlComponent component(&engine);
    component.setData(R"(
import ImageViewport 1.0

ImageSequenceProviderAdapter {}
)",
        QUrl());

    QVERIFY(component.isError());
}

void ImageViewportTest::exposesTypedSequenceFactorySurface()
{
    ImageSequenceFactory factory;
    const QMetaObject *metaObject = factory.metaObject();

    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromFrame(ImageFrame*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromTimedFrameList(TimedImageFrameList*)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("fromProvider(ImageSequenceProviderAdapter*)")) >= 0);

    QObject *result = factory.fromFrame(nullptr);
    QVERIFY(result);
    QCOMPARE(result->property("sequence").value<QObject *>(), nullptr);
    QCOMPARE(result->property("outcome").toInt(), enumValue(result->metaObject(), "FactoryOutcome", "Invalid"));
    QVERIFY(!result->property("errorString").toString().isEmpty());
}

void ImageViewportTest::factoryResultDiagnosticsArePublicSafe()
{
    const int limit = ImageSequenceLimits::maximumDiagnosticStringLength();
    QString diagnostic = QStringLiteral("failed for https://user:secret@example.test/image.png token=abc123 path /home/ops/private/image.png ");
    diagnostic += QString(limit + 100, QLatin1Char('x'));

    ImageSequenceFactoryResult result(nullptr,
        ImageSequenceFactoryResult::FactoryOutcome::Invalid,
        diagnostic,
        diagnostic);

    const QString errorString = result.errorString();
    const QString warningString = result.warningString();
    QCOMPARE(errorString.toUcs4().size(), limit);
    QCOMPARE(warningString.toUcs4().size(), limit);
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(errorString.contains(QStringLiteral("[redacted")));
    QVERIFY(!warningString.contains(QStringLiteral("https://")));
    QVERIFY(!warningString.contains(QStringLiteral("user:secret")));
    QVERIFY(!warningString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!warningString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(warningString.contains(QStringLiteral("[redacted")));
}

void ImageViewportTest::exposesImageSequenceLimits()
{
    ImageSequenceLimits limits;

    QCOMPARE(limits.property("maximumLogicalWidth").toInt(), ImageSequenceLimits::maximumLogicalWidth());
    QVERIFY(limits.property("maximumLogicalWidth").toInt() >= 8192);
    QVERIFY(limits.property("maximumLogicalHeight").toInt() >= 8192);
    QVERIFY(limits.property("maximumPixelsPerFrame").toLongLong() >= 67108864LL);
    QVERIFY(limits.property("maximumPayloadBytesPerFrame").toLongLong() >= 268435456LL);
    QVERIFY(limits.property("maximumTimedListFrameCount").toInt() >= 10000);
    QVERIFY(limits.property("maximumFrameDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumTotalSequenceDuration").toInt() >= 86400000);
    QVERIFY(limits.property("maximumDiagnosticStringLength").toInt() >= 4096);
}

void ImageViewportTest::imageFrameRetainsImmutablePayload()
{
    QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
    image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    image.setPixelColor(1, 0, QColor(0, 255, 0, 255));

    ImageFrame frame(image);
    image.fill(QColor(0, 0, 255, 255));

    const QImage retained = frame.imageForTest();
    QCOMPARE(retained.size(), QSize(2, 1));
    QCOMPARE(retained.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(retained.pixelColor(1, 0), QColor(0, 255, 0, 255));
}

void ImageViewportTest::stillImageSequenceRetainsFactoryPayload()
{
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        QImage image(2, 1, QImage::Format_ARGB32_Premultiplied);
        image.setPixelColor(0, 0, QColor(255, 0, 0, 255));
        image.setPixelColor(1, 0, QColor(0, 255, 0, 255));
        ImageFrame frame(image);
        result.reset(factory.fromFrame(&frame));
        QVERIFY(result->sequence());
    }

    const QImage retained = result->sequence()->frameImageForTest(0);
    QCOMPARE(retained.size(), QSize(2, 1));
    QCOMPARE(retained.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(retained.pixelColor(1, 0), QColor(0, 255, 0, 255));
}

void ImageViewportTest::timedFrameListSequenceRetainsFactoryPayloads()
{
    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        QImage firstImage(2, 1, QImage::Format_ARGB32_Premultiplied);
        firstImage.fill(QColor(255, 0, 0, 255));
        QImage secondImage(2, 1, QImage::Format_ARGB32_Premultiplied);
        secondImage.fill(QColor(0, 255, 0, 255));
        ImageFrame firstFrame(firstImage);
        ImageFrame secondFrame(secondImage);
        TimedImageFrameList list;
        QVERIFY(list.appendFrame(&firstFrame, 100));
        QVERIFY(list.appendFrame(&secondFrame, 250));

        result.reset(factory.fromTimedFrameList(&list));
        QVERIFY(result->sequence());
    }

    const QImage firstRetained = result->sequence()->frameImageForTest(0);
    QCOMPARE(firstRetained.size(), QSize(2, 1));
    QCOMPARE(firstRetained.pixelColor(0, 0), QColor(255, 0, 0, 255));

    const QImage secondRetained = result->sequence()->frameImageForTest(1);
    QCOMPARE(secondRetained.size(), QSize(2, 1));
    QCOMPARE(secondRetained.pixelColor(0, 0), QColor(0, 255, 0, 255));
}

void ImageViewportTest::commandsWithoutRequestAreIgnoredDiagnostics()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    QCOMPARE(item.seek(-1), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);

    QSignalSpy sequenceSpy(&item, &ImageViewport::sequenceChanged);
    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);
    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 3U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QCOMPARE(item.property("displayRevision").toUInt(), 0U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(sequenceSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportTest::resetViewWithoutRequestClearsTransformAndCommandDiagnostic()
{
    ImageViewport item;
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::IgnoredNoRequest);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "IgnoredNoRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, -3.0));
    const uint displayRevisionBeforeReset = item.property("displayRevision").toUInt();

    QSignalSpy requestSpy(&item, &ImageViewport::requestStateChanged);
    QSignalSpy displayRevisionSpy(&item, &ImageViewport::displayRevisionChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);
    QSignalSpy commandSpy(&item, &ImageViewport::commandStateChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.pan(), QPointF());
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), 0U);
    QVERIFY(item.property("displayRevision").toUInt() > displayRevisionBeforeReset);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(displayRevisionSpy.count(), 1);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(presentationSpy.count(), 1);
    QCOMPARE(commandSpy.count(), 1);
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportTest::stillImageSequenceAssignmentPublishesReadyState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 0);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    const QVariantMap centerImage = item.itemToImage(50.0, 50.0);
    QCOMPARE(centerImage.value("valid").toBool(), true);
    QCOMPARE(centerImage.value("x").toDouble(), 8.0);
    QCOMPARE(centerImage.value("y").toDouble(), 4.0);

    const QVariantMap rightEdgeImage = item.itemToImage(100.0, 50.0);
    QCOMPARE(rightEdgeImage.value("valid").toBool(), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(16.0, 4.0), false);

    const QVariantMap centerItem = item.imageToItem(8.0, 4.0);
    QCOMPARE(centerItem.value("valid").toBool(), true);
    QCOMPARE(centerItem.value("x").toDouble(), 50.0);
    QCOMPARE(centerItem.value("y").toDouble(), 50.0);
}

void ImageViewportTest::nullSequenceAssignmentClearsDisplayObservations()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint readyRequestRevision = item.property("requestRevision").toUInt();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    item.setSequence(providerResult->sequence());

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportTest::clearPreservesPresentationState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignRight);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignBottom);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setZoom(2.5);
    item.setPan(QPointF(12.0, -6.0));
    item.setLooping(true);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.sequence(), nullptr);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Cover);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignRight);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignBottom);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.zoom(), 2.5);
    QCOMPARE(item.pan(), QPointF(12.0, -6.0));
    QCOMPARE(item.looping(), true);
}

void ImageViewportTest::clearReadyDisplayEmitsGeometryStateChanged()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSequence(nullptr);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(geometrySpy.count(), 1);
}

void ImageViewportTest::clearNonPresentableDisplayDoesNotEmitGeometryStateChanged()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    item.setSize(QSizeF(0.0, 100.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setSequence(nullptr);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::stillImageReadyReplacementIncrementsDisplayRevision()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    const uint readyRequestRevision = item.property("requestRevision").toUInt();

    item.setSequence(replacementResult->sequence());

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
}

void ImageViewportTest::stillImageReplacementPreservesPresentationState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QVERIFY(firstResult->sequence());

    QImage replacementImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::black);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromFrame(&replacementFrame));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    const QMetaObject *metaObject = item.metaObject();

    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setVerticalAlignment(ImageViewport::VerticalAlignment::AlignTop);
    item.setSmoothing(false);
    item.setMipmap(true);
    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    item.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);
    item.setBackgroundColor(QColor(20, 40, 60, 255));
    item.setZoom(1.5);
    item.setPan(QPointF(-8.0, 6.0));
    item.setLooping(true);

    item.setSequence(replacementResult->sequence());

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Cover);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignTop);
    QCOMPARE(item.smoothing(), false);
    QCOMPARE(item.mipmap(), true);
    QCOMPARE(item.mirrorHorizontally(), true);
    QCOMPARE(item.mirrorVertically(), true);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Checkerboard);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QCOMPARE(item.zoom(), 1.5);
    QCOMPARE(item.pan(), QPointF(-8.0, 6.0));
    QCOMPARE(item.looping(), true);
}

void ImageViewportTest::stillImageCommandsPreserveOrReplaceDocumentedState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    const uint readyRequestRevision = item.property("requestRevision").toUInt();
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > readyRequestRevision);
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    const uint afterAcceptedSeekRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 1U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 2U);
    QCOMPARE(item.property("requestRevision").toUInt(), afterAcceptedSeekRequestRevision);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("commandRevision").toUInt(), 3U);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("zoom").toDouble(), 1.0);
    QCOMPARE(item.property("pan").toPointF(), QPointF(0.0, 0.0));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), 4U);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::stillImageFillModesAndMirroringUseDocumentedGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    item.setFillMode(ImageViewport::FillMode::Cover);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(-50.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(4.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(0.0, 50.0).value("x").toDouble(), 4.0);
    QCOMPARE(item.itemToImage(99.0, 50.0).value("valid").toBool(), true);
    QCOMPARE(item.containsVisibleImagePoint(3.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(12.0, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(11.999, 4.0), true);
    QCOMPARE(item.imageToItem(4.0, 4.0).value("x").toDouble(), 0.0);
    QCOMPARE(item.imageToItem(12.0, 4.0).value("valid").toBool(), false);

    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.itemToImage(99.0, 50.0).value("x").toDouble(), 7.92);

    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(50.0, 50.0).value("x").toDouble(), 8.0);
    QCOMPARE(item.itemToImage(50.0, 50.0).value("y").toDouble(), 4.0);

    item.setFillMode(ImageViewport::FillMode::Center);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(42.0, 46.0, 16.0, 8.0));
    QCOMPARE(item.itemToImage(42.0, 46.0).value("valid").toBool(), true);
    QCOMPARE(item.itemToImage(58.0, 50.0).value("valid").toBool(), false);

    item.setMirrorHorizontally(true);
    item.setMirrorVertically(true);
    const QVariantMap mirrored = item.itemToImage(42.001, 46.001);
    QCOMPARE(mirrored.value("valid").toBool(), true);
    QCOMPARE(mirrored.value("x").toDouble(), 15.999);
    QCOMPARE(mirrored.value("y").toDouble(), 7.999);

    const QVariantMap mirroredItem = item.imageToItem(15.999, 7.999);
    QCOMPARE(mirroredItem.value("valid").toBool(), true);
    QCOMPARE(mirroredItem.value("x").toDouble(), 42.001);
    QCOMPARE(mirroredItem.value("y").toDouble(), 46.001);

    const QVariantMap mirroredOriginItem = item.imageToItem(0.0, 0.0);
    QCOMPARE(mirroredOriginItem.value("valid").toBool(), true);
    QCOMPARE(mirroredOriginItem.value("x").toDouble(), 58.0);
    QCOMPARE(mirroredOriginItem.value("y").toDouble(), 54.0);
}

void ImageViewportTest::stillImageMirroredCoverUsesMirroredVisibleImageRect()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    item.setFillMode(ImageViewport::FillMode::Cover);
    item.setHorizontalAlignment(ImageViewport::HorizontalAlignment::AlignLeft);
    item.setMirrorHorizontally(true);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 200.0, 100.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(8.0, 0.0, 8.0, 8.0));
    QCOMPARE(item.containsVisibleImagePoint(7.999, 4.0), false);
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), true);
    QCOMPARE(item.containsVisibleImagePoint(15.999, 4.0), true);

    const QVariantMap leftItem = item.itemToImage(0.001, 50.0);
    QCOMPARE(leftItem.value("valid").toBool(), true);
    QCOMPARE(leftItem.value("x").toDouble(), 15.99992);

    const QVariantMap rightHalfImage = item.imageToItem(12.0, 4.0);
    QCOMPARE(rightHalfImage.value("valid").toBool(), true);
    QCOMPARE(rightHalfImage.value("x").toDouble(), 50.0);
    QCOMPARE(item.imageToItem(4.0, 4.0).value("valid").toBool(), false);
}

void ImageViewportTest::stillImageAssignmentWaitsForPositiveGeometry()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.itemToImage(0.0, 0.0).value("valid").toBool(), false);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    const uint readyDisplayRevision = item.property("displayRevision").toUInt();
    item.setSize(QSizeF(0.0, 100.0));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.containsVisibleImagePoint(8.0, 4.0), false);
}

void ImageViewportTest::stillImageFactoryRejectsPublishedLimitViolations()
{
    ImageSequenceFactory factory;
    QImage oversized(ImageSequenceLimits::maximumLogicalWidth() + 1,
        1,
        QImage::Format_ARGB32_Premultiplied);
    oversized.fill(Qt::transparent);
    ImageFrame frame(oversized);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("maximumLogicalWidth")));
}

void ImageViewportTest::stillImageFactoryRejectsInvalidPayloadByteSize()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image, -1);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("payload byte size")));
}

void ImageViewportTest::timedFrameListBuilderValidatesEntries()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QImage differentSizeImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    differentSizeImage.fill(Qt::transparent);
    ImageFrame differentSizeFrame(differentSizeImage);

    TimedImageFrameList list;
    const QMetaObject *metaObject = list.metaObject();
    QVERIFY(metaObject->indexOfProperty("count") >= 0);
    QVERIFY(metaObject->indexOfProperty("errorString") >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("appendFrame(ImageFrame*,int)")) >= 0);
    QVERIFY(metaObject->indexOfMethod(QMetaObject::normalizedSignature("clear()")) >= 0);

    QCOMPARE(list.count(), 0);
    QCOMPARE(list.appendFrame(nullptr, 100), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("ImageFrame")));

    QCOMPARE(list.appendFrame(&frame, 0), false);
    QCOMPARE(list.count(), 0);
    QVERIFY(list.errorString().contains(QStringLiteral("duration")));

    QCOMPARE(list.appendFrame(&frame, 100), true);
    QCOMPARE(list.appendFrame(&differentSizeFrame, 100), false);
    QCOMPARE(list.count(), 1);
    QVERIFY(list.errorString().contains(QStringLiteral("logical size")));

    list.clear();
    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
}

void ImageViewportTest::timedFrameListRejectsPublishedDurationLimits()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    TimedImageFrameList frameDurationList;
    QCOMPARE(frameDurationList.appendFrame(&frame, ImageSequenceLimits::maximumFrameDuration() + 1), false);
    QCOMPARE(frameDurationList.count(), 0);
    QVERIFY(frameDurationList.errorString().contains(QStringLiteral("maximumFrameDuration")));

    TimedImageFrameList totalDurationList;
    QCOMPARE(totalDurationList.appendFrame(&frame, ImageSequenceLimits::maximumTotalSequenceDuration()), true);
    QCOMPARE(totalDurationList.appendFrame(&frame, 1), false);
    QCOMPARE(totalDurationList.count(), 1);
    QVERIFY(totalDurationList.errorString().contains(QStringLiteral("maximumTotalSequenceDuration")));
}

void ImageViewportTest::timedFrameListRejectsPublishedFrameCountLimit()
{
    QImage image(1, 1, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);

    TimedImageFrameList list;
    for (int index = 0; index < ImageSequenceLimits::maximumTimedListFrameCount(); ++index) {
        QCOMPARE(list.appendFrame(&frame, 1), true);
    }

    QCOMPARE(list.count(), ImageSequenceLimits::maximumTimedListFrameCount());
    QCOMPARE(list.appendFrame(&frame, 1), false);
    QCOMPARE(list.count(), ImageSequenceLimits::maximumTimedListFrameCount());
    QVERIFY(list.errorString().contains(QStringLiteral("maximumTimedListFrameCount")));
}

void ImageViewportTest::timedFrameListAllowsCumulativePayloadsAbovePerFrameLimit()
{
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const qsizetype admittedPayloadSize = ImageSequenceLimits::maximumPayloadBytesPerFrame() / 2 + 1;
    QVERIFY(admittedPayloadSize <= ImageSequenceLimits::maximumPayloadBytesPerFrame());
    ImageFrame firstFrame(image, admittedPayloadSize);
    ImageFrame secondFrame(image, admittedPayloadSize);

    TimedImageFrameList list;
    QCOMPARE(list.appendFrame(&firstFrame, 100), true);
    QCOMPARE(list.appendFrame(&secondFrame, 100), true);
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.errorString(), QString());

    ImageSequenceFactory factory;
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
}

void ImageViewportTest::timedFrameListClearDiagnosticOnlyPreservesCountNotification()
{
    TimedImageFrameList list;

    QVERIFY(!list.appendFrame(nullptr, 100));
    QCOMPARE(list.count(), 0);
    QVERIFY(!list.errorString().isEmpty());

    QSignalSpy countSpy(&list, &TimedImageFrameList::countChanged);
    QSignalSpy diagnosticsSpy(&list, &TimedImageFrameList::diagnosticsChanged);

    list.clear();

    QCOMPARE(list.count(), 0);
    QCOMPARE(list.errorString(), QString());
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportTest::timedFrameListAssignmentPublishesInitialTimedState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
}

void ImageViewportTest::timedFrameListSeekCommandsSelectDocumentedTargets()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    const uint initialRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > initialRequestRevision);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    const uint acceptedFrameSeekRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("requestRevision").toUInt(), acceptedFrameSeekRevision);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    const uint acceptedPositionSeekRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seekToPosition(351), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("requestRevision").toUInt(), acceptedPositionSeekRevision);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::timedFrameListSeekWhilePlayingWaitsForRenderCommit()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(0.0, 100.0));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
}

void ImageViewportTest::timedFrameListSeekWithUnchangedGeometryDoesNotNotifyGeometryState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(geometrySpy.count(), 0);

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::timedFrameListPlaybackCommandsUpdatePhase()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::timedFrameListBackgroundOnlyChangesPreserveRequestAndPlayback()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    const int requestStatus = item.property("requestStatus").toInt();
    const int requestReason = item.property("requestReason").toInt();
    const int displayStatus = item.property("displayStatus").toInt();
    const int playbackPhase = item.property("playbackPhase").toInt();
    const int requestedFrame = item.property("requestedFrame").toInt();
    const int displayedFrame = item.property("displayedFrame").toInt();
    const int requestedPosition = item.property("requestedPosition").toInt();
    const int displayedPosition = item.property("displayedPosition").toInt();
    const uint requestRevision = item.property("requestRevision").toUInt();
    const uint commandRevision = item.property("commandRevision").toUInt();
    const uint displayRevision = item.property("displayRevision").toUInt();

    QSignalSpy requestRevisionSpy(&item, &ImageViewport::requestRevisionChanged);
    QSignalSpy playbackSpy(&item, &ImageViewport::playbackPhaseChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));

    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::SolidColor);
    QCOMPARE(item.backgroundColor(), QColor(20, 40, 60, 255));
    QVERIFY(item.property("displayRevision").toUInt() > displayRevision);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(item.property("commandRevision").toUInt(), commandRevision);
    QCOMPARE(item.property("requestStatus").toInt(), requestStatus);
    QCOMPARE(item.property("requestReason").toInt(), requestReason);
    QCOMPARE(item.property("displayStatus").toInt(), displayStatus);
    QCOMPARE(item.property("playbackPhase").toInt(), playbackPhase);
    QCOMPARE(item.property("requestedFrame").toInt(), requestedFrame);
    QCOMPARE(item.property("displayedFrame").toInt(), displayedFrame);
    QCOMPARE(item.property("requestedPosition").toInt(), requestedPosition);
    QCOMPARE(item.property("displayedPosition").toInt(), displayedPosition);
    QCOMPARE(requestRevisionSpy.count(), 0);
    QCOMPARE(playbackSpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::timedFrameListPlaybackAdvancesDeterministically()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(99);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(0.0, 100.0));
    item.advancePlaybackForTest(1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.advancePlaybackForTest(249);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);

    item.advancePlaybackForTest(1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::timedFrameListPlaybackWithUnchangedGeometryDoesNotNotifyGeometryState()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.advancePlaybackForTest(100);

    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
}

void ImageViewportTest::timedFrameListStopAfterPauseWhileRenderWaitingRestoresPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    item.setSize(QSizeF(0.0, 100.0));
    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::timedFrameListLoopingPlaybackWrapsToFirstFrame()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    item.setLooping(true);
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(350);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::replacementRetainsPreviousDisplayWhileWaitingForGeometry()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage replacementImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    replacementImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    ImageFrame replacementFrame(replacementImage);
    QScopedPointer<ImageSequenceFactoryResult> firstResult(factory.fromFrame(&firstFrame));
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromFrame(&replacementFrame));
    QVERIFY(firstResult->sequence());
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(firstResult->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(replacementResult->sequence());
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.itemToImage(1.0, 1.0).value("valid").toBool(), false);

    const uint retainedRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(item.property("requestRevision").toUInt() > retainedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    item.setSize(QSizeF(100.0, 100.0));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(8.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 0.0, 100.0, 100.0));
}

void ImageViewportTest::providerFactoryRejectsBaseAdapterWithoutSessionFactory()
{
    ImageSequenceFactory factory;
    NullSessionFactoryProviderAdapter adapter;

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("session")));
}

void ImageViewportTest::providerFactoryRejectsContradictoryConstructionFacts()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}),
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("provider metadata")));
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerFactoryRejectsPublishedKnownMetadataLimits()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0),
            {ImageSequenceLimits::maximumFrameDuration() + 1}));

    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));

    QVERIFY(result);
    QCOMPARE(result->sequence(), nullptr);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Invalid);
    QVERIFY(result->errorString().contains(QStringLiteral("maximumFrameDuration")));
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerSequenceOpensSessionAfterAdapterDestruction()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);

    QScopedPointer<ImageSequenceFactoryResult> result;
    {
        CountingProviderAdapter adapter(sessionFactory);
        result.reset(factory.fromProvider(&adapter));
    }

    QVERIFY(result);
    QCOMPARE(result->outcome(), ImageSequenceFactoryResult::FactoryOutcome::Created);
    QVERIFY(result->sequence());
    QCOMPARE(*sessionCount, 0);
    QCOMPARE(*metadataRequestCount, 0);

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerSessionOpenFailureKeepsReplacementObservable()
{
    ImageSequenceFactory factory;
    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromTimedFrameList(&list));
    QVERIFY(previousResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<FailingProviderSessionFactory>(sessionCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> replacementResult(factory.fromProvider(&adapter));
    QVERIFY(replacementResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(previousResult->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();

    item.setSequence(replacementResult->sequence());

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(item.sequence(), replacementResult->sequence());
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("session")));

    const uint failedRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestRevision").toUInt(), failedRequestRevision);
}

void ImageViewportTest::reassigningSameProviderSequenceStartsNewGeneration()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint initialRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*closeCount, 0);

    item.setSequence(result->sequence());

    QCOMPARE(*sessionCount, 2);
    QCOMPARE(*metadataRequestCount, 2);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("requestRevision").toUInt() > initialRequestRevision);
}

void ImageViewportTest::providerSessionClosesWhenViewportIsDestroyed()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    {
        ImageViewport item;
        item.setSequence(result->sequence());
        QCOMPARE(*sessionCount, 1);
        QCOMPARE(*metadataRequestCount, 1);
        QCOMPARE(*closeCount, 0);
    }

    QCOMPARE(*closeCount, 1);
}

void ImageViewportTest::providerClearCancelsActiveFrameRequestBeforeClose()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto lastCancelledTokenId = std::make_shared<quint64>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount,
        lastCancelledTokenId);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    const ImageSequenceProviderRequestToken frameToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*lastCancelledTokenId, frameToken.id());
    QCOMPARE(*closeCount, 1);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
}

void ImageViewportTest::providerClearIgnoresCancelledMetadataAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        cancelRequestCount,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    const uint clearedRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(item.property("requestRevision").toUInt(), clearedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerClearIgnoresCancelledFrameAcknowledgement()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CancellingAcknowledgementProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        cancelRequestCount,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    QCOMPARE(item.clear(), ImageViewport::CommandOutcome::Accepted);
    const uint clearedRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(*cancelRequestCount, 1);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(item.property("requestRevision").toUInt(), clearedRequestRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "NoRequest"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "NoRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SynchronousMetadataProviderSessionFactory>(metadataRequestCount,
        frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);

    QCoreApplication::processEvents();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
}

void ImageViewportTest::providerFrameResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto frameRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SynchronousFrameProviderSessionFactory>(frameRequestCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));

    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
}

void ImageViewportTest::providerTerminalResultsAreQueuedFromSessionEntryPoint()
{
    ImageSequenceFactory factory;
    const auto metadataRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<SynchronousFailureProviderSessionFactory>(metadataRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("errorString").toString(), QString());

    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("metadata failed synchronously")));
}

void ImageViewportTest::providerConstructionMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerFixedDurationConstructionMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100),
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 3);
    QCOMPARE(item.property("totalDuration").toInt(), 300);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 2);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 300);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerPartialConstructionMetadataWaitsForRuntimeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerPartialConstructionMetadataBindsAcceptedSeekAfterRuntimeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    const uint acceptedSeekRevision = item.property("requestRevision").toUInt();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QVERIFY(item.property("requestRevision").toUInt() > acceptedSeekRevision);
}

void ImageViewportTest::providerPartialStillConstructionMetadataConstrainsCommands()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint requestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*lastRequestedFrame, -1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));

    QCOMPARE(item.seekToPosition(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(*frameRequestCount, 0);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerPartialConstructionMetadataRejectsSeeksPastKnownBounds()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    const uint requestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(*frameRequestCount, 0);

    QCOMPARE(item.seekToPosition(351), ImageViewport::CommandOutcome::Invalid);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "InvalidRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 0);
}

void ImageViewportTest::providerDeclaredCapabilityProjectsBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    QSignalSpy displaySpy(&item, &ImageViewport::displayStateChanged);
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(displaySpy.count(), 0);
    QCOMPARE(geometrySpy.count(), 0);
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), -1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "Unavailable"));
}

void ImageViewportTest::providerKnownCapabilityProjectsBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse,
        ImageSequenceProviderAdapter::CapabilitySupport::KnownTrue);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerDeclaredCapabilityContradictionRejectsMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider metadata")));
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
}

void ImageViewportTest::providerDeclaredNoPlaybackRejectsPlayBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerKnownNoPlaybackRejectsPlayBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::KnownFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerDeclaredNoFrameSeekRejectsSeekBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(2), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerDeclaredNoPositionSeekRejectsPositionSeekBeforeMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory,
        {},
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::Unavailable,
        ImageSequenceProviderAdapter::CapabilitySupport::DeclaredFalse);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(250), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerMetadataRejectsNonFiniteLogicalSize()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(std::numeric_limits<double>::infinity(), 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider metadata is invalid")));
}

void ImageViewportTest::providerMetadataRejectsHugeFiniteLogicalSize()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(1.0e20, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("maximumLogicalWidth")));
}

void ImageViewportTest::providerMetadataRejectsPublishedFrameCountLimit()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVector<int> durations(ImageSequenceLimits::maximumTimedListFrameCount() + 1, 1);
    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("maximumTimedListFrameCount")));
}

void ImageViewportTest::providerMetadataRejectsPublishedDurationLimits()
{
    auto verifyRejectedDurations = [](const QVector<int> &durations, const QString &expectedDiagnostic) {
        ImageSequenceFactory factory;
        const auto sessionCount = std::make_shared<int>(0);
        const auto metadataRequestCount = std::make_shared<int>(0);
        const auto frameRequestCount = std::make_shared<int>(0);
        const auto lastRequestedFrame = std::make_shared<int>(-1);
        const auto closeCount = std::make_shared<int>(0);
        auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
            metadataRequestCount,
            frameRequestCount,
            lastRequestedFrame,
            closeCount);
        CountingProviderAdapter adapter(sessionFactory);
        QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
        QVERIFY(result->sequence());

        ImageViewport item;
        item.setSequence(result->sequence());
        const QMetaObject *metaObject = item.metaObject();

        QVERIFY(sessionFactory->lastSession());
        emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
            ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), durations));
        drainQueuedProviderResults();

        QCOMPARE(*frameRequestCount, 0);
        QCOMPARE(*closeCount, 1);
        QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
        QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
        QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
        QCOMPARE(item.property("requestedFrame").toInt(), -1);
        QCOMPARE(item.property("displayedFrame").toInt(), -1);
        QVERIFY(item.property("errorString").toString().contains(expectedDiagnostic));
    };

    verifyRejectedDurations({ImageSequenceLimits::maximumFrameDuration() + 1},
        QStringLiteral("maximumFrameDuration"));
    verifyRejectedDurations({ImageSequenceLimits::maximumTotalSequenceDuration(), 1},
        QStringLiteral("maximumTotalSequenceDuration"));
}

void ImageViewportTest::providerStillMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    QCOMPARE(*frameRequestCount, 0);

    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0));
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), -1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));
}

void ImageViewportTest::providerTimedMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderMetadata metadata = ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250});
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(), metadata);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 1);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("minimum").toInt(), 0);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 350);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));
}

void ImageViewportTest::providerFixedDurationMetadataSelectsInitialFrameRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::fixedDurationFrames(QSizeF(16.0, 8.0), 3, 100));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 3);
    QCOMPARE(item.property("totalDuration").toInt(), 300);
    QCOMPARE(item.property("frameSeekBounds").toMap().value("maximum").toInt(), 2);
    QCOMPARE(item.property("positionSeekBounds").toMap().value("maximum").toInt(), 300);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "True"));

    QCOMPARE(item.seekToPosition(250), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 2);
    QCOMPARE(item.property("requestedFrame").toInt(), 2);
    QCOMPARE(item.property("requestedPosition").toInt(), 250);
}

void ImageViewportTest::providerProgressResultsAreAdvisory()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken = sessionFactory->lastSession()->lastMetadataToken();
    emit sessionFactory->lastSession()->providerProgress(metadataToken, 0.5);
    drainQueuedProviderResults();
    emit sessionFactory->lastSession()->providerWaiting(metadataToken);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(*frameRequestCount, 0);

    emit sessionFactory->lastSession()->metadataReady(metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    emit sessionFactory->lastSession()->providerProgress(metadataToken, 1.0);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(*frameRequestCount, 1);
}

void ImageViewportTest::providerPositiveResizeWhileMetadataWaitingKeepsProviderWaiting()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(0.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(*metadataRequestCount, 1);
    QCOMPARE(*frameRequestCount, 0);
}

void ImageViewportTest::providerMetadataReadySealsMetadataToken()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    const ImageSequenceProviderRequestToken metadataToken = sessionFactory->lastSession()->lastMetadataToken();
    emit sessionFactory->lastSession()->metadataReady(metadataToken,
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    emit sessionFactory->lastSession()->providerFailed(metadataToken, QStringLiteral("late metadata failure"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerFrameSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
}

void ImageViewportTest::providerInvalidPreMetadataSeekCanStartPlaybackAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(3), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 3);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "InvalidRequest"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(*frameRequestCount, 0);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
}

void ImageViewportTest::providerPositionSeekBeforeMetadataResolvesAfterMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("frameCount").toInt(), 2);
    QCOMPARE(item.property("totalDuration").toInt(), 350);
}

void ImageViewportTest::providerPositionSeekBeforeStillMetadataKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seekToPosition(10), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 10);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), 10);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("positionSeekSupport").toInt(), enumValue(metaObject, "TriState", "False"));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
}

void ImageViewportTest::providerPlaybackBeforeStillMetadataKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QCOMPARE(item.property("totalDuration").toInt(), -1);
    QCOMPARE(item.property("timedPlaybackSupport").toInt(), enumValue(metaObject, "TriState", "False"));
    QCOMPARE(item.property("frameSeekSupport").toInt(), enumValue(metaObject, "TriState", "True"));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
}

void ImageViewportTest::providerStillFrameReadyCommitsDisplay()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportTest::providerTimedFrameReadyCommitsTimedDisplay()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QCOMPARE(item.property("displayedPosition").toInt(), -1);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(0.0, 0.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF());

    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
}

void ImageViewportTest::providerTimedFrameEnvelopeMismatchRejectsPayload()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    const ImageSequenceProviderRequestToken frameToken = sessionFactory->lastSession()->lastFrameToken();
    emitTimedProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame, 1, 100);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider frame payload is invalid")));

    emitTimedProviderFrameReady(sessionFactory->lastSession(), frameToken, &frame, 0, 0);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
}

void ImageViewportTest::providerTimedFramePayloadLimitReportsUnsupportedPayload()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    uchar pixel = 0;
    const qsizetype excessiveStride = ImageSequenceLimits::maximumPayloadBytesPerFrame() / 8 + 1;
    QImage image(&pixel, 16, 8, excessiveStride, QImage::Format_ARGB32_Premultiplied);
    QVERIFY(image.sizeInBytes() > ImageSequenceLimits::maximumPayloadBytesPerFrame());
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider frame payload exceeds maximumPayloadBytesPerFrame")));
}

void ImageViewportTest::providerFrameRejectsInvalidPayloadByteSize()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image, -1);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider frame payload is invalid")));
}

void ImageViewportTest::providerTimedFrameSeekRequestsSelectedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    const uint readyDisplayRevision = item.property("displayRevision").toUInt();

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));
    QVERIFY(item.property("displayRevision").toUInt() > readyDisplayRevision);
}

void ImageViewportTest::providerTimedFrameSeekWithoutDiagnosticsDoesNotNotify()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());

    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(item.property("warningString").toString(), QString());
    QCOMPARE(diagnosticsSpy.count(), 0);
}

void ImageViewportTest::providerTimedFrameCommitWithUnchangedGeometryDoesNotNotifyGeometryState()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage firstImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(Qt::transparent);
    ImageFrame firstFrame(firstImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &firstFrame, 0, 0);
    QVERIFY(commitPaintNode(item));
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));

    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(geometrySpy.count(), 0);

    QImage secondImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(Qt::black);
    ImageFrame secondFrame(secondImage);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &secondFrame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("contentRect").toRectF(), QRectF(0.0, 25.0, 100.0, 50.0));
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF(0.0, 0.0, 16.0, 8.0));
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::providerTimedFrameSeekCancelsSupersededRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken firstSeekToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == firstSeekToken);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), firstSeekToken, &frame, 1, 100);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    emit sessionFactory->lastSession()->providerUnsupported(firstSeekToken,
        QStringLiteral("superseded request unsupported late"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerCancelled(firstSeekToken,
        QStringLiteral("superseded request cleanup complete"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerFailed(firstSeekToken,
        QStringLiteral("superseded request failed late"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerTimedPositionSeekRequestsResolvedFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QCOMPARE(item.seekToPosition(349), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 349);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(16.0, 8.0));

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.seekToPosition(350), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("requestedPosition").toInt(), 350);
}

void ImageViewportTest::providerTimedPlaybackCommandsUpdatePhase()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
}

void ImageViewportTest::providerTimedPlaybackAdvancesDeterministically()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(99);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(*frameRequestCount, 1);

    item.advancePlaybackForTest(1);

    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.advancePlaybackForTest(1000);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.advancePlaybackForTest(249);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.advancePlaybackForTest(1);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedPlaybackFrameReadyWaitsForRenderCommit()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QScopedPointer<QSGNode> initialRoot(item.takePaintNode());
    QVERIFY(initialRoot);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
}

void ImageViewportTest::providerTimedPlaybackEndOfSequenceRequestsFinalFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(350);

    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->endOfSequence(playbackToken);
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedPlaybackEndOfSequenceDoesNotPromoteRetainedPreviousGeneration()
{
    ImageSequenceFactory factory;

    QImage previousImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    previousImage.fill(Qt::transparent);
    ImageFrame previousFrame(previousImage);
    TimedImageFrameList previousList;
    QVERIFY(previousList.appendFrame(&previousFrame, 100));
    QVERIFY(previousList.appendFrame(&previousFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromTimedFrameList(&previousList));
    QVERIFY(previousResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    PaintProbeViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(previousResult->sequence());
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    item.setSequence(providerResult->sequence());
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    emit sessionFactory->lastSession()->endOfSequence(playbackToken);
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedPlaybackEndOfSequenceFinalUsesPlaybackEntryPoint()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(350);
    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);

    emit sessionFactory->lastSession()->endOfSequence(playbackToken);
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedLoopingPlaybackWrapsToFirstFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    item.setLooping(true);
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.advancePlaybackForTest(250);

    QCOMPARE(*playbackRequestCount, 2);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::providerMetadataEndOfSequenceReportsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->endOfSequence(sessionFactory->lastSession()->lastMetadataToken());
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider protocol violation")));

    const uint requestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
}

void ImageViewportTest::providerFrameEndOfSequenceReportsProtocolViolation()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);

    emit sessionFactory->lastSession()->endOfSequence(sessionFactory->lastSession()->lastFrameToken());
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("provider protocol violation")));

    const uint requestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
}

void ImageViewportTest::providerTimedPlaybackAdvancementUsesPlaybackEntryPoint()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
}

void ImageViewportTest::providerTimedPlaybackStopsOnFrameFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastFrameToken(),
        QStringLiteral("playback frame failed"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("playback frame failed")));
}

void ImageViewportTest::providerTimedPlaybackWaitsForMetadata()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedPlaybackAfterMetadataUsesPlaybackEntryPoint()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedPausedPlaybackAfterMetadataUsesPlaybackEntryPoint()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedStopAfterPausedMetadataWaitRestoresInitialRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Paused"));
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(*playbackRequestCount, 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedStopWhileWaitingForMetadataRestoresInitialRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    const uint playbackRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("requestRevision").toUInt() > playbackRequestRevision);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 0);
}

void ImageViewportTest::providerTimedStopWhileWaitingForMetadataRestoresExplicitSeek()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.seek(3), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 3);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    const uint playbackRequestRevision = item.property("requestRevision").toUInt();

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 3);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("requestRevision").toUInt() > playbackRequestRevision);
    QCOMPARE(*frameRequestCount, 0);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250, 300, 400}));
    drainQueuedProviderResults();

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 3);
    QCOMPARE(item.property("requestedPosition").toInt(), 650);
    QCOMPARE(*frameRequestCount, 1);
    QCOMPARE(*lastRequestedFrame, 3);
}

void ImageViewportTest::providerTimedStopAfterMetadataPlaybackCreatesNonPlaybackRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition,
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*frameRequestCount, 1);

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken nonPlaybackToken = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == playbackToken);
    QVERIFY(nonPlaybackToken != playbackToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), playbackToken, &frame, 0, 0);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));

    emitTimedProviderFrameReady(sessionFactory->lastSession(), nonPlaybackToken, &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
}

void ImageViewportTest::providerTimedStopAfterMetadataPlaybackRestoresSupersededExplicitSeek()
{
    ImageSequenceFactory factory;

    QImage previousImage(16, 8, QImage::Format_ARGB32_Premultiplied);
    previousImage.fill(Qt::transparent);
    ImageFrame previousFrame(previousImage);
    TimedImageFrameList previousList;
    QVERIFY(previousList.appendFrame(&previousFrame, 100));
    QVERIFY(previousList.appendFrame(&previousFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> previousResult(factory.fromTimedFrameList(&previousList));
    QVERIFY(previousResult->sequence());

    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition,
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> providerResult(factory.fromProvider(&adapter));
    QVERIFY(providerResult->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(previousResult->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);

    item.setSequence(providerResult->sequence());
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 0);
    QCOMPARE(*lastPlaybackPosition, 0);
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken nonPlaybackToken = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == playbackToken);
    QVERIFY(nonPlaybackToken != playbackToken);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerTimedStopCancelsPlaybackRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*cancelRequestCount, 1);
    QVERIFY(sessionFactory->lastSession()->lastCancelledToken() == playbackToken);
}

void ImageViewportTest::providerTimedStopSupersedesPlaybackRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    item.advancePlaybackForTest(100);
    const ImageSequenceProviderRequestToken playbackToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);

    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), playbackToken, &frame, 1, 100);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->endOfSequence(playbackToken);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerUnsupported(playbackToken,
        QStringLiteral("stopped playback unsupported late"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerCancelled(playbackToken,
        QStringLiteral("stopped playback cancelled late"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());

    emit sessionFactory->lastSession()->providerFailed(playbackToken,
        QStringLiteral("stopped playback failed late"));
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerTimedSeekWhilePlayingWaitsForFrame()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
}

void ImageViewportTest::providerMetadataFailureReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("metadata service unavailable")));
}

void ImageViewportTest::providerInvalidTerminalTokenAfterMetadataIsIgnored()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    emit sessionFactory->lastSession()->providerFailed(ImageSequenceProviderRequestToken(),
        QStringLiteral("invalid token failure"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerDiagnosticsUseUnicodeScalarLimit()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    const int limit = ImageSequenceLimits::maximumDiagnosticStringLength();
    const char32_t codePoint[] = {0x1F642};
    const QString scalar = QString::fromUcs4(codePoint, 1);
    QString diagnostic;
    QString expected;
    diagnostic.reserve((limit + 1) * scalar.size());
    expected.reserve(limit * scalar.size());
    for (int i = 0; i < limit; ++i) {
        diagnostic += scalar;
        expected += scalar;
    }
    diagnostic += scalar;
    diagnostic += QStringLiteral("tail");

    ImageViewport item;
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        diagnostic);
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(errorString.toUcs4().size(), limit);
    QCOMPARE(errorString, expected);
}

void ImageViewportTest::providerDiagnosticsRedactPrivateDetails()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("decoder failed for https://user:secret@example.test/image.png token=abc123 path /home/ops/private/image.png and C:\\Users\\ops\\secret.png"));
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.contains(QStringLiteral("https://")));
    QVERIFY(!errorString.contains(QStringLiteral("user:secret")));
    QVERIFY(!errorString.contains(QStringLiteral("token=abc123")));
    QVERIFY(!errorString.contains(QStringLiteral("/home/ops/private")));
    QVERIFY(!errorString.contains(QStringLiteral("C:\\Users\\ops")));
    QVERIFY(errorString.contains(QStringLiteral("[redacted")));
}

void ImageViewportTest::providerDiagnosticsArePlainText()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("decoder <b>failed</b>\n<script>alert(1)</script>\ttry again"));
    drainQueuedProviderResults();

    const QString errorString = item.property("errorString").toString();
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QVERIFY(!errorString.contains(QLatin1Char('<')));
    QVERIFY(!errorString.contains(QLatin1Char('>')));
    QVERIFY(!errorString.contains(QLatin1Char('\n')));
    QVERIFY(!errorString.contains(QLatin1Char('\t')));
    QCOMPARE(errorString, QStringLiteral("decoder failed alert(1) try again"));
}

void ImageViewportTest::providerMetadataFailureStopsPendingPlayback()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
}

void ImageViewportTest::providerGenerationTerminalFailureRejectsDisplayCommands()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const uint requestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);

    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), requestRevision);
}

void ImageViewportTest::providerGenerationTerminalFailureAcceptsControlCommands()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerFailed(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata service unavailable"));
    drainQueuedProviderResults();

    const uint failedRequestRevision = item.property("requestRevision").toUInt();
    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Unsupported);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "UnsupportedRequest"));
    const uint unsupportedCommandRevision = item.property("commandRevision").toUInt();

    QCOMPARE(item.pause(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), unsupportedCommandRevision + 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), failedRequestRevision);

    const uint clearedCommandRevision = item.property("commandRevision").toUInt();
    QCOMPARE(item.stop(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("commandReason").toInt(), enumValue(metaObject, "CommandReason", "NoCommand"));
    QCOMPARE(item.property("commandRevision").toUInt(), clearedCommandRevision);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Stopped"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QCOMPARE(item.property("requestRevision").toUInt(), failedRequestRevision);
    QCOMPARE(*sessionCount, 1);
    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*closeCount, 1);
}

void ImageViewportTest::providerFrameFailureKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerFailed(frameToken,
        QStringLiteral("frame decode failed"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("frame decode failed")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    QSignalSpy diagnosticsSpy(&item, &ImageViewport::diagnosticsChanged);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("errorString").toString(), QString());
    QCOMPARE(diagnosticsSpy.count(), 1);
}

void ImageViewportTest::providerMetadataUnsupportedReportsUnsupportedRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerUnsupported(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("unsupported codec"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "UnsupportedRequest"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported codec")));
}

void ImageViewportTest::providerFrameUnsupportedKeepsGenerationSeekable()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerUnsupported(frameToken,
        QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("frameCount").toInt(), 1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported frame shape")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
}

void ImageViewportTest::providerFrameUnsupportedRetainsDisplayAndClearsOnSeek()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken unsupportedToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerUnsupported(unsupportedToken,
        QStringLiteral("unsupported frame shape"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Unsupported"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "PayloadRejection"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("unsupported frame shape")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::providerMetadataCancellationReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto cancelRequestCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        std::shared_ptr<int>(),
        cancelRequestCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->providerCancelled(sessionFactory->lastSession()->lastMetadataToken(),
        QStringLiteral("metadata cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*frameRequestCount, 0);
    QCOMPARE(*cancelRequestCount, 0);
    QCOMPARE(*closeCount, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), -1);
    QCOMPARE(item.property("requestedPosition").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("metadata cancelled by provider")));
}

void ImageViewportTest::providerFrameCancellationReportsProviderFailure()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    drainQueuedProviderResults();
    QCOMPARE(*frameRequestCount, 1);

    const ImageSequenceProviderRequestToken frameToken = sessionFactory->lastSession()->lastFrameToken();
    emit sessionFactory->lastSession()->providerCancelled(frameToken,
        QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("cancelled by provider")));

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(frameToken, &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
}

void ImageViewportTest::providerFrameCancellationRetainsDisplayAndClearsOnSeek()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(16.0, 8.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    const ImageSequenceProviderRequestToken cancelledToken = sessionFactory->lastSession()->lastFrameToken();
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    emit sessionFactory->lastSession()->providerCancelled(cancelledToken,
        QStringLiteral("cancelled by provider"));
    drainQueuedProviderResults();

    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("cancelled by provider")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 0);
    QCOMPARE(item.property("requestedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::solidBackgroundCreatesPaintNode()
{
    PaintProbeViewport item;
    item.setSize(QSizeF(24.0, 12.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::SolidColor);
    item.setBackgroundColor(QColor(20, 40, 60, 255));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(root->childCount(), 1);

    auto *background = dynamic_cast<QSGSimpleRectNode *>(root->firstChild());
    QVERIFY(background);
    QCOMPARE(background->rect(), QRectF(0.0, 0.0, 24.0, 12.0));
    QCOMPARE(background->color(), QColor(20, 40, 60, 255));
}

void ImageViewportTest::checkerboardBackgroundCreatesPaintNode()
{
    PaintProbeViewport item;
    item.setSize(QSizeF(24.0, 12.0));
    item.setBackgroundMode(ImageViewport::BackgroundMode::Checkerboard);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QVERIFY(root->childCount() > 0);
}

void ImageViewportTest::stillImageCreatesTexturePaintNode()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto *imageNode = dynamic_cast<QSGImageNode *>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportTest::stillImagePaintFailureReportsRenderFailure()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    PaintProbeViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));

    QScopedPointer<QSGNode> root(item.takePaintNode());

    QVERIFY(!root);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));
    QCOMPARE(item.property("displayedFrame").toInt(), -1);
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("render commit failed")));

    QCOMPARE(item.seek(0), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::timedFrameListPaintFailureRetainsPreviousDisplay()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    PaintProbeViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setSize(QSizeF(40.0, 20.0));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 1);

    QScopedPointer<QSGNode> root(item.takePaintNode());

    QVERIFY(!root);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportTest::timedFrameListPlayAfterPaintFailureRestartsDisplayRequest()
{
    ImageSequenceFactory factory;
    QImage firstImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    firstImage.fill(QColor(255, 0, 0, 255));
    QImage secondImage(4, 2, QImage::Format_ARGB32_Premultiplied);
    secondImage.fill(QColor(0, 255, 0, 255));
    ImageFrame firstFrame(firstImage);
    ImageFrame secondFrame(secondImage);
    TimedImageFrameList list;
    QVERIFY(list.appendFrame(&firstFrame, 100));
    QVERIFY(list.appendFrame(&secondFrame, 250));
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromTimedFrameList(&list));
    QVERIFY(result->sequence());

    PaintProbeViewport item;
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    item.setSize(QSizeF(0.0, 20.0));
    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    item.setSize(QSizeF(40.0, 20.0));
    QScopedPointer<QSGNode> failedRoot(item.takePaintNode());
    QVERIFY(!failedRoot);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::successfulPaintClearsRenderFailureInterest()
{
    ImageSequenceFactory factory;
    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    item.setParentItem(nullptr);
    QScopedPointer<QSGNode> detachedRoot(item.takePaintNode());

    QVERIFY(!detachedRoot);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::coverImageTextureNodeUsesVisibleSourceRect()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(100, 100);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    item.setFillMode(ImageViewport::FillMode::Cover);

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto *imageNode = dynamic_cast<QSGImageNode *>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), QRectF(0.0, 0.0, 100.0, 100.0));
    QCOMPARE(imageNode->sourceRect(), item.property("visibleImageRect").toRectF());
}

void ImageViewportTest::providerStillFrameCreatesTexturePaintNode()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(4.0, 2.0)));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto *imageNode = dynamic_cast<QSGImageNode *>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportTest::providerStillFrameWaitingForGeometryCreatesTexturePaintNode()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(0.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::still(QSizeF(4.0, 2.0)));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emit sessionFactory->lastSession()->frameReady(sessionFactory->lastSession()->lastFrameToken(), &frame);
    drainQueuedProviderResults();

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Empty"));

    item.setSize(QSizeF(40.0, 20.0));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));

    QScopedPointer<QSGNode> root(item.takePaintNode());
    QVERIFY(root);

    auto *imageNode = dynamic_cast<QSGImageNode *>(root->lastChild());
    QVERIFY(imageNode);
    QVERIFY(imageNode->texture());
    QCOMPARE(imageNode->rect(), item.property("contentRect").toRectF());
}

void ImageViewportTest::providerTimedFramePaintFailureRetainsPreviousDisplay()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(*frameRequestCount, 2);
    QCOMPARE(*lastRequestedFrame, 1);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);

    item.setParentItem(nullptr);
    QScopedPointer<QSGNode> failedRoot(item.takePaintNode());

    QVERIFY(!failedRoot);
    QCOMPARE(*closeCount, 0);
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);
    QCOMPARE(item.property("displayedPosition").toInt(), 0);
    QCOMPARE(item.property("displayedImageSize").toSizeF(), QSizeF(4.0, 2.0));
    QVERIFY(item.property("errorString").toString().contains(QStringLiteral("render commit failed")));
}

void ImageViewportTest::providerTimedPlayAfterPaintFailureRestartsPlaybackRequest()
{
    ImageSequenceFactory factory;
    const auto sessionCount = std::make_shared<int>(0);
    const auto metadataRequestCount = std::make_shared<int>(0);
    const auto frameRequestCount = std::make_shared<int>(0);
    const auto lastRequestedFrame = std::make_shared<int>(-1);
    const auto closeCount = std::make_shared<int>(0);
    const auto playbackRequestCount = std::make_shared<int>(0);
    const auto lastPlaybackFrame = std::make_shared<int>(-1);
    const auto lastPlaybackPosition = std::make_shared<int>(-1);
    auto sessionFactory = std::make_shared<CountingProviderSessionFactory>(sessionCount,
        metadataRequestCount,
        frameRequestCount,
        lastRequestedFrame,
        closeCount,
        playbackRequestCount,
        lastPlaybackFrame,
        lastPlaybackPosition);
    CountingProviderAdapter adapter(sessionFactory);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromProvider(&adapter));
    QVERIFY(result->sequence());

    QQuickWindow window;
    window.resize(40, 20);
    PaintProbeViewport item;
    item.setParentItem(window.contentItem());
    item.setSize(QSizeF(40.0, 20.0));
    item.setSequence(result->sequence());
    const QMetaObject *metaObject = item.metaObject();

    QVERIFY(sessionFactory->lastSession());
    emit sessionFactory->lastSession()->metadataReady(sessionFactory->lastSession()->lastMetadataToken(),
        ImageSequenceProviderMetadata::timedFrameList(QSizeF(4.0, 2.0), {100, 250}));
    drainQueuedProviderResults();

    QImage image(4, 2, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(255, 0, 0, 255));
    ImageFrame frame(image);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 0, 0);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.seek(1), ImageViewport::CommandOutcome::Accepted);
    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);

    item.setParentItem(nullptr);
    QScopedPointer<QSGNode> failedRoot(item.takePaintNode());
    QVERIFY(!failedRoot);

    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Error"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "RenderFailure"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedFrame").toInt(), 0);

    item.setParentItem(window.contentItem());
    QCOMPARE(item.play(), ImageViewport::CommandOutcome::Accepted);

    QCOMPARE(*playbackRequestCount, 1);
    QCOMPARE(*lastPlaybackFrame, 1);
    QCOMPARE(*lastPlaybackPosition, 100);
    QCOMPARE(*frameRequestCount, 3);
    QCOMPARE(*lastRequestedFrame, 1);
    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Waiting"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Loading"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "ProviderWaiting"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Retained"));
    QCOMPARE(item.property("errorString").toString(), QString());

    emitTimedProviderFrameReady(sessionFactory->lastSession(), &frame, 1, 100);
    QVERIFY(commitPaintNode(item));

    QCOMPARE(item.property("playbackPhase").toInt(), enumValue(metaObject, "PlaybackPhase", "Playing"));
    QCOMPARE(item.property("requestStatus").toInt(), enumValue(metaObject, "RequestStatus", "Ready"));
    QCOMPARE(item.property("requestReason").toInt(), enumValue(metaObject, "RequestReason", "Ready"));
    QCOMPARE(item.property("displayStatus").toInt(), enumValue(metaObject, "DisplayStatus", "Ready"));
    QCOMPARE(item.property("requestedFrame").toInt(), 1);
    QCOMPARE(item.property("requestedPosition").toInt(), 100);
    QCOMPARE(item.property("displayedFrame").toInt(), 1);
    QCOMPARE(item.property("displayedPosition").toInt(), 100);
    QCOMPARE(item.property("errorString").toString(), QString());
}

void ImageViewportTest::invalidPresentationEnumValuesAreIgnored()
{
    ImageViewport item;
    const uint initialDisplayRevision = item.property("displayRevision").toUInt();

    item.setFillMode(static_cast<ImageViewport::FillMode>(999));
    item.setHorizontalAlignment(static_cast<ImageViewport::HorizontalAlignment>(999));
    item.setVerticalAlignment(static_cast<ImageViewport::VerticalAlignment>(999));
    item.setBackgroundMode(static_cast<ImageViewport::BackgroundMode>(999));

    QCOMPARE(item.fillMode(), ImageViewport::FillMode::Contain);
    QCOMPARE(item.horizontalAlignment(), ImageViewport::HorizontalAlignment::AlignHCenter);
    QCOMPARE(item.verticalAlignment(), ImageViewport::VerticalAlignment::AlignVCenter);
    QCOMPARE(item.backgroundMode(), ImageViewport::BackgroundMode::Transparent);
    QCOMPARE(item.property("displayRevision").toUInt(), initialDisplayRevision);
}

void ImageViewportTest::presentationZoomUsesExactValueChanges()
{
    ImageViewport item;
    const double changedZoom = 1.0 + 5.0e-13;
    QVERIFY(changedZoom != 1.0);
    QSignalSpy presentationSpy(&item, &ImageViewport::presentationChanged);

    item.setZoom(changedZoom);

    QCOMPARE(item.zoom(), changedZoom);
    QCOMPARE(item.property("displayRevision").toUInt(), 1U);
    QCOMPARE(presentationSpy.count(), 1);

    QCOMPARE(item.resetView(), ImageViewport::CommandOutcome::Accepted);
    QCOMPARE(item.zoom(), 1.0);
    QCOMPARE(item.property("displayRevision").toUInt(), 2U);
    QCOMPARE(presentationSpy.count(), 2);
}

void ImageViewportTest::presentationChangesWithoutDisplayDoNotNotifyGeometryState()
{
    ImageViewport item;
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(2.0);
    item.setPan(QPointF(4.0, 8.0));
    item.setFillMode(ImageViewport::FillMode::Stretch);
    item.setMirrorHorizontally(true);

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(geometrySpy.count(), 0);

    item.setSize(QSizeF(100.0, 100.0));

    QCOMPARE(item.property("contentRect").toRectF(), QRectF());
    QCOMPARE(item.property("visibleImageRect").toRectF(), QRectF());
    QCOMPARE(geometrySpy.count(), 0);
}

void ImageViewportTest::presentationChangesNotifyGeometryState()
{
    ImageSequenceFactory factory;
    QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    ImageFrame frame(image);
    QScopedPointer<ImageSequenceFactoryResult> result(factory.fromFrame(&frame));
    QVERIFY(result->sequence());

    ImageViewport item;
    item.setSize(QSizeF(100.0, 100.0));
    item.setSequence(result->sequence());
    QSignalSpy geometrySpy(&item, &ImageViewport::geometryStateChanged);

    item.setZoom(2.0);
    QCOMPARE(geometrySpy.count(), 1);

    item.setPan(QPointF(4.0, 8.0));
    QCOMPARE(geometrySpy.count(), 2);

    item.setFillMode(ImageViewport::FillMode::Stretch);
    QCOMPARE(geometrySpy.count(), 3);

    item.setMirrorHorizontally(true);
    QCOMPARE(geometrySpy.count(), 4);
}

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
