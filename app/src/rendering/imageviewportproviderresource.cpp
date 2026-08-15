// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewportproviderresource.h"

#include "cache/imagebytecost.h"

#include <QImageIOHandler>
#include <QPointer>
#include <QTransform>
#include <algorithm>
#include <limits>
#include <new>
#include <ranges>
#include <utility>

namespace {
ImageViewportPayloadQuality payloadQuality(kiriview::DisplayImageQuality quality)
{
    switch (quality) {
    case kiriview::DisplayImageQuality::ThumbnailPreview:
        return ImageViewportPayloadQuality::Preview;
    case kiriview::DisplayImageQuality::FirstDisplay:
        return ImageViewportPayloadQuality::FirstDisplay;
    case kiriview::DisplayImageQuality::BoundedDetail:
        return ImageViewportPayloadQuality::BoundedDetail;
    case kiriview::DisplayImageQuality::Exact:
        return ImageViewportPayloadQuality::Exact;
    }
    return ImageViewportPayloadQuality::Unknown;
}

ImageViewportPayloadExactness payloadExactness(
    kiriview::DisplayImageQuality quality, const QSize& originalSize, const QSize& rasterSize)
{
    return quality == kiriview::DisplayImageQuality::Exact && originalSize == rasterSize
        ? ImageViewportPayloadExactness::ExactForSource
        : ImageViewportPayloadExactness::NotExact;
}

ImageFrame::OrientationPolicy orientationPolicy(QImageIOHandler::Transformations transformations)
{
    switch (static_cast<QImageIOHandler::Transformation>(transformations.toInt())) {
    case QImageIOHandler::TransformationNone:
        return ImageFrame::OrientationPolicy::Identity;
    case QImageIOHandler::TransformationMirror:
        return ImageFrame::OrientationPolicy::MirrorHorizontally;
    case QImageIOHandler::TransformationFlip:
        return ImageFrame::OrientationPolicy::MirrorVertically;
    case QImageIOHandler::TransformationRotate180:
        return ImageFrame::OrientationPolicy::Rotate180;
    case QImageIOHandler::TransformationRotate90:
        return ImageFrame::OrientationPolicy::Rotate90;
    case QImageIOHandler::TransformationMirrorAndRotate90:
        return ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90;
    case QImageIOHandler::TransformationFlipAndRotate90:
        return ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90;
    case QImageIOHandler::TransformationRotate270:
        return ImageFrame::OrientationPolicy::Rotate270;
    }
    return ImageFrame::OrientationPolicy::Identity;
}

kiriview::DisplayedPageRole displayedPageRole(ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? kiriview::DisplayedPageRole::Secondary
                                                    : kiriview::DisplayedPageRole::Primary;
}

std::optional<kiriview::DisplayImageRasterIdentity> rasterIdentityFor(
    const kiriview::StaticDisplayImagePayload& displayImage,
    const ImageSequenceProviderFrameEnvelope& envelope,
    kiriview::ImageViewportProviderFrameStage stage)
{
    if (envelope.isTimedFrame()) {
        if (stage == kiriview::ImageViewportProviderFrameStage::Provisional
            || displayImage.rasterKind != kiriview::DisplayImageRasterKind::TimedFrame) {
            return std::nullopt;
        }
        kiriview::DisplayImageRasterIdentity identity
            = kiriview::DisplayImageRasterIdentity::timedFrame(envelope.frame());
        return identity.isValid() ? std::optional<kiriview::DisplayImageRasterIdentity>(identity)
                                  : std::nullopt;
    }
    if (!envelope.isStillFrame()) {
        return std::nullopt;
    }
    if (stage == kiriview::ImageViewportProviderFrameStage::Provisional) {
        return displayImage.rasterKind == kiriview::DisplayImageRasterKind::ProvisionalPreview
            ? std::optional<kiriview::DisplayImageRasterIdentity>(
                  kiriview::DisplayImageRasterIdentity::provisionalPreview())
            : std::nullopt;
    }
    switch (displayImage.rasterKind) {
    case kiriview::DisplayImageRasterKind::AuthoritativeStill:
        return kiriview::DisplayImageRasterIdentity::authoritativeStill();
    case kiriview::DisplayImageRasterKind::Refinement:
        return kiriview::DisplayImageRasterIdentity::refinement();
    case kiriview::DisplayImageRasterKind::ProvisionalPreview:
    case kiriview::DisplayImageRasterKind::TimedFrame:
        return std::nullopt;
    }
    return std::nullopt;
}

QImage sourcePayloadForOrientation(
    const QImage& normalizedImage, ImageFrame::OrientationPolicy orientation)
{
    switch (orientation) {
    case ImageFrame::OrientationPolicy::Identity:
        return normalizedImage;
    case ImageFrame::OrientationPolicy::MirrorHorizontally:
        return normalizedImage.flipped(Qt::Horizontal);
    case ImageFrame::OrientationPolicy::MirrorVertically:
        return normalizedImage.flipped(Qt::Vertical);
    case ImageFrame::OrientationPolicy::Rotate180:
        return normalizedImage.transformed(QTransform().rotate(180));
    case ImageFrame::OrientationPolicy::Rotate90:
        return normalizedImage.transformed(QTransform().rotate(270));
    case ImageFrame::OrientationPolicy::MirrorHorizontallyAndRotate90:
        return normalizedImage.transformed(QTransform().rotate(270)).flipped(Qt::Horizontal);
    case ImageFrame::OrientationPolicy::MirrorVerticallyAndRotate90:
        return normalizedImage.transformed(QTransform().rotate(270)).flipped(Qt::Vertical);
    case ImageFrame::OrientationPolicy::Rotate270:
        return normalizedImage.transformed(QTransform().rotate(90));
    }
    return normalizedImage;
}

qsizetype frameConstructionBufferCount(ImageFrame::OrientationPolicy orientation)
{
    return orientation == ImageFrame::OrientationPolicy::Identity ? 1 : 3;
}

std::optional<qsizetype> frameConstructionPeakByteCount(
    const QImage& image, ImageFrame::OrientationPolicy orientation)
{
    const qsizetype retainedByteCount = image.sizeInBytes();
    const qsizetype bufferCount = frameConstructionBufferCount(orientation);
    if (retainedByteCount <= 0 || bufferCount <= 0
        || retainedByteCount > std::numeric_limits<qsizetype>::max() / bufferCount) {
        return std::nullopt;
    }
    return retainedByteCount * bufferCount;
}

class DisplayImageFrameLease final
{
public:
    DisplayImageFrameLease(std::shared_ptr<kiriview::DisplayImageStore> store, QString storeEntryId)
        : m_store(std::move(store))
        , m_storeEntryId(std::move(storeEntryId))
    {
    }

    ~DisplayImageFrameLease()
    {
        if (m_store != nullptr && !m_storeEntryId.isEmpty()) {
            m_store->releaseFrameLease(m_storeEntryId);
        }
    }

    Q_DISABLE_COPY_MOVE(DisplayImageFrameLease)

private:
    std::shared_ptr<kiriview::DisplayImageStore> m_store;
    QString m_storeEntryId;
};

struct FrameConstructionResult
{
    std::unique_ptr<ImageFrame> frame;
    kiriview::ImageDecodeWorkspaceHold retainedWorkspace;
    std::shared_ptr<DisplayImageFrameLease> displayStoreLease;
};
}

namespace kiriview {
enum class FrameConstructionState : quint8 {
    WaitingForAdmission,
    StartingWorker,
    Running,
    Retiring,
};

struct ImageViewportProviderResource::ActiveFrameConstruction
{
    ActiveFrameConstruction() = default;
    ~ActiveFrameConstruction() { QObject::disconnect(receiverDestroyedConnection); }
    Q_DISABLE_COPY_MOVE(ActiveFrameConstruction)

    quint64 id = 0;
    ImageViewportProviderWorkIdentity identity;
    ImageDecodeWorkspaceAdmission admission;
    ImageWorkerTask task;
    QMetaObject::Connection receiverDestroyedConnection;
    FrameConstructionState state = FrameConstructionState::WaitingForAdmission;
    bool publishResult = true;
};

bool operator==(
    const ImageViewportProviderWorkIdentity& left, const ImageViewportProviderWorkIdentity& right)
{
    return left.sourceGeneration == right.sourceGeneration && left.role == right.role
        && left.requestToken == right.requestToken && left.demandRevision == right.demandRevision
        && left.locationIdentity == right.locationIdentity;
}

ImageViewportProviderFrameResult& ImageViewportProviderFrameResult::operator=(
    const ImageViewportProviderFrameResult& other)
{
    if (this == &other) {
        return *this;
    }
    ImageViewportProviderFrameResult copy(other);
    return *this = std::move(copy);
}

ImageViewportProviderFrameResult& ImageViewportProviderFrameResult::operator=(
    ImageViewportProviderFrameResult&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    ImageViewportProviderFrameResult retired;
    retired.outputAdmission = std::move(outputAdmission);
    retired.displayImage = std::move(displayImage);
    retired.envelope = envelope;
    retired.formatIdentifier = std::move(formatIdentifier);
    retired.failureCause = failureCause;
    retired.failure = std::move(failure);
    retired.unsupportedCause = unsupportedCause;
    retired.stage = stage;

    outputAdmission = std::move(other.outputAdmission);
    displayImage = std::move(other.displayImage);
    envelope = other.envelope;
    formatIdentifier = std::move(other.formatIdentifier);
    failureCause = other.failureCause;
    failure = std::move(other.failure);
    unsupportedCause = other.unsupportedCause;
    stage = other.stage;
    return *this;
}

ImageViewportProviderPreparedFrame& ImageViewportProviderPreparedFrame::operator=(
    const ImageViewportProviderPreparedFrame& other)
{
    if (this == &other) {
        return *this;
    }
    ImageViewportProviderPreparedFrame copy(other);
    return *this = std::move(copy);
}

ImageViewportProviderPreparedFrame& ImageViewportProviderPreparedFrame::operator=(
    ImageViewportProviderPreparedFrame&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    ImageViewportProviderPreparedFrame retired;
    retired.outputAdmission = std::move(outputAdmission);
    retired.authoritativeStillDisplayImage = std::move(authoritativeStillDisplayImage);
    retired.storeEntryId = std::move(storeEntryId);
    retired.reuseKey = std::move(reuseKey);
    retired.envelope = envelope;
    retired.formatIdentifier = std::move(formatIdentifier);
    retired.failureCause = failureCause;
    retired.failure = std::move(failure);
    retired.unsupportedCause = unsupportedCause;
    retired.stage = stage;

    outputAdmission = std::move(other.outputAdmission);
    authoritativeStillDisplayImage = std::move(other.authoritativeStillDisplayImage);
    storeEntryId = std::move(other.storeEntryId);
    reuseKey = std::move(other.reuseKey);
    envelope = other.envelope;
    formatIdentifier = std::move(other.formatIdentifier);
    failureCause = other.failureCause;
    failure = std::move(other.failure);
    unsupportedCause = other.unsupportedCause;
    stage = other.stage;
    return *this;
}

ImageViewportProviderMetadataResult ImageViewportProviderMetadataResult::ready(
    ImageSequenceProviderMetadata metadata)
{
    ImageViewportProviderMetadataResult result;
    result.metadata = std::move(metadata);
    return result;
}

ImageViewportProviderMetadataResult ImageViewportProviderMetadataResult::failed(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    ImageViewportProviderMetadataResult result;
    result.failureCause = cause;
    result.failure = std::move(failure);
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::ready(
    StaticDisplayImagePayload displayImage, ImageSequenceProviderFrameEnvelope envelope,
    QString formatIdentifier, std::shared_ptr<DisplayImageOutputAdmission> outputAdmission)
{
    ImageViewportProviderFrameResult result;
    result.displayImage = std::move(displayImage);
    result.envelope = envelope;
    result.formatIdentifier = std::move(formatIdentifier);
    result.outputAdmission = std::move(outputAdmission);
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::provisional(
    StaticDisplayImagePayload displayImage, ImageSequenceProviderFrameEnvelope envelope,
    QString formatIdentifier)
{
    ImageViewportProviderFrameResult result;
    result.displayImage = std::move(displayImage);
    result.envelope = envelope;
    result.formatIdentifier = std::move(formatIdentifier);
    result.stage = ImageViewportProviderFrameStage::Provisional;
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::unsupported(
    ImageSequenceProviderUnsupportedCause cause)
{
    ImageViewportProviderFrameResult result;
    result.unsupportedCause = cause;
    return result;
}

ImageViewportProviderFrameResult ImageViewportProviderFrameResult::failed(
    ImageSequenceProviderFailureCause cause, ImageLoadFailure failure)
{
    ImageViewportProviderFrameResult result;
    result.failureCause = cause;
    result.failure = std::move(failure);
    return result;
}

ImageViewportProviderResource::ImageViewportProviderResource(quint64 sourceGeneration,
    QString locationIdentity, std::shared_ptr<ImageViewportProviderSource> source,
    std::shared_ptr<DisplayImageStore> displayStore,
    std::shared_ptr<ImageViewportFailureRegistry> failureRegistry,
    std::shared_ptr<ImageDecodeWorkspaceBudget> workspaceBudget,
    ImageWorkerScheduler frameConstructionScheduler)
    : m_sourceGeneration(sourceGeneration)
    , m_locationIdentity(std::move(locationIdentity))
    , m_displayLocationIdentity(m_locationIdentity)
    , m_source(std::move(source))
    , m_displayStore(std::move(displayStore))
    , m_failureRegistry(failureRegistry == nullptr
              ? std::make_shared<ImageViewportFailureRegistry>()
              : std::move(failureRegistry))
    , m_workspaceBudget(workspaceBudget == nullptr ? defaultImageDecodeWorkspaceBudget()
                                                   : std::move(workspaceBudget))
    , m_frameConstructionScheduler(frameConstructionScheduler.isValid()
              ? std::move(frameConstructionScheduler)
              : defaultImageWorkerScheduler())
{
    Q_ASSERT(m_displayStore != nullptr);
}

ImageViewportProviderResource::~ImageViewportProviderResource() = default;

bool ImageViewportProviderResource::bindDisplayLocationIdentity(QString locationIdentity)
{
    if (locationIdentity.isEmpty()) {
        return false;
    }

    const QMutexLocker lock(&m_stateMutex);
    if (m_closed || m_displayLocationIdentityBound || m_payloadPreparationStarted) {
        return false;
    }
    m_displayLocationIdentity = std::move(locationIdentity);
    m_displayLocationIdentityBound = true;
    return true;
}

ImageSequenceProviderMetadata ImageViewportProviderResource::constructionMetadata() const
{
    return m_source == nullptr ? ImageSequenceProviderMetadata {}
                               : m_source->constructionMetadata();
}

void ImageViewportProviderResource::requestMetadata(
    const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion)
{
    if (!matchesResource(identity) || m_source == nullptr || !completion) {
        return;
    }
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        if (std::ranges::contains(m_activeMetadataWork, identity)) {
            return;
        }
        m_activeMetadataWork.push_back(identity);
    }
    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestMetadata(identity,
        [resource, completion = std::move(completion)](ImageViewportProviderWorkIdentity completed,
            ImageViewportProviderMetadataResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner != nullptr && owner->matchesResource(completed)
                && owner->finalizeMetadata(completed)) {
                completion(std::move(completed), std::move(result));
            }
        });
}

void ImageViewportProviderResource::requestFrame(const ImageViewportProviderWorkIdentity& identity,
    ImageViewportProviderFrameRequest request, FrameCompletion completion)
{
    if (!matchesResource(identity) || !completion) {
        return;
    }
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        std::erase_if(
            m_activeFrameWork, [&identity](const ImageViewportProviderWorkIdentity& active) {
                return active.role == identity.role && active != identity;
            });
        std::erase_if(m_authoritativeFrameCandidates,
            [&identity](const AuthoritativeFrameCandidate& candidate) {
                return candidate.identity.role == identity.role;
            });
        if (m_authoritativeStillDisplayImageCandidate.has_value()
            && m_authoritativeStillDisplayImageCandidate->identity.role == identity.role) {
            m_authoritativeStillDisplayImageCandidate.reset();
        }
        if (!std::ranges::contains(m_activeFrameWork, identity)) {
            m_activeFrameWork.push_back(identity);
        }
    }

    if (m_source == nullptr) {
        ImageViewportProviderPreparedFrame result;
        result.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        if (finalizePreparedFrame(identity, result)) {
            completion(identity, std::move(result));
        }
        return;
    }

    const qint64 displayStoreEntryBudget = m_displayStore->byteBudget();
    request.maximumStoreEntryBytes = request.maximumStoreEntryBytes < 0
        ? displayStoreEntryBudget
        : std::min(request.maximumStoreEntryBytes, displayStoreEntryBudget);
    request.outputStore = m_displayStore;
    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    m_source->requestFrame(identity, request,
        [resource, completion = std::move(completion)](
            const ImageViewportProviderWorkIdentity& completed,
            ImageViewportProviderFrameResult result) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner == nullptr || !owner->matchesResource(completed)) {
                return;
            }
            ImageViewportProviderPreparedFrame prepared
                = owner->prepareFrame(completed, std::move(result));
            if (owner->finalizePreparedFrame(completed, prepared)) {
                completion(completed, std::move(prepared));
            }
        });
}

void ImageViewportProviderResource::cancel(const QVector<ImageSequenceProviderRequestToken>& tokens)
{
    std::vector<std::unique_ptr<ActiveFrameConstruction>> canceledConstructions;
    std::vector<ImageWorkerTask> canceledTasks;
    {
        const QMutexLocker lock(&m_stateMutex);
        std::erase_if(
            m_activeMetadataWork, [&tokens](const ImageViewportProviderWorkIdentity& identity) {
                return std::ranges::contains(tokens, identity.requestToken);
            });
        std::erase_if(
            m_activeFrameWork, [&tokens](const ImageViewportProviderWorkIdentity& identity) {
                return std::ranges::contains(tokens, identity.requestToken);
            });
        std::erase_if(m_authoritativeFrameCandidates,
            [&tokens](const AuthoritativeFrameCandidate& candidate) {
                return std::ranges::contains(tokens, candidate.identity.requestToken);
            });
        if (m_authoritativeStillDisplayImageCandidate.has_value()
            && std::ranges::contains(
                tokens, m_authoritativeStillDisplayImageCandidate->identity.requestToken)) {
            m_authoritativeStillDisplayImageCandidate.reset();
        }
        auto construction = m_activeFrameConstructions.begin();
        while (construction != m_activeFrameConstructions.end()) {
            if (*construction == nullptr
                || !std::ranges::contains(tokens, (*construction)->identity.requestToken)) {
                ++construction;
                continue;
            }
            (*construction)->publishResult = false;
            if ((*construction)->state == FrameConstructionState::WaitingForAdmission) {
                canceledConstructions.push_back(std::move(*construction));
                construction = m_activeFrameConstructions.erase(construction);
            } else {
                canceledTasks.push_back(std::move((*construction)->task));
                ++construction;
            }
        }
    }
    canceledConstructions.clear();
    for (ImageWorkerTask& task : canceledTasks) {
        task.cancel();
    }
    if (m_source != nullptr) {
        m_source->cancel(tokens);
    }
}

void ImageViewportProviderResource::close()
{
    std::vector<std::unique_ptr<ActiveFrameConstruction>> canceledConstructions;
    std::vector<ImageWorkerTask> canceledTasks;
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        m_closed = true;
        m_activeMetadataWork.clear();
        m_activeFrameWork.clear();
        m_authoritativeFrameCandidates.clear();
        m_authoritativeStillDisplayImageCandidate.reset();
        auto construction = m_activeFrameConstructions.begin();
        while (construction != m_activeFrameConstructions.end()) {
            if (*construction == nullptr
                || (*construction)->state == FrameConstructionState::WaitingForAdmission) {
                canceledConstructions.push_back(std::move(*construction));
                construction = m_activeFrameConstructions.erase(construction);
                continue;
            }
            (*construction)->publishResult = false;
            canceledTasks.push_back(std::move((*construction)->task));
            ++construction;
        }
    }
    canceledConstructions.clear();
    for (ImageWorkerTask& task : canceledTasks) {
        task.cancel();
    }
    if (m_source != nullptr) {
        m_source->close();
    }
}

std::optional<StaticDisplayImagePayload> ImageViewportProviderResource::currentStillDisplayImage(
    ImageViewportDemandRevisionToken demandRevision) const
{
    const QMutexLocker lock(&m_stateMutex);
    if (!m_currentStillDisplayImage.has_value()
        || m_currentStillDisplayImage->identity.demandRevision != demandRevision) {
        return std::nullopt;
    }
    const std::optional<DisplayImageStoreEntry> entry
        = m_displayStore->entry(m_currentStillDisplayImage->storeEntryId);
    if (!entry.has_value()) {
        return std::nullopt;
    }

    StaticDisplayImagePayload displayImage = m_currentStillDisplayImage->displayImage;
    displayImage.image = entry->image;
    return displayImage.isValid()
        ? std::optional<StaticDisplayImagePayload>(std::move(displayImage))
        : std::nullopt;
}

bool ImageViewportProviderResource::acceptAuthoritativeStillDisplayImage(
    const ImageViewportProviderWorkIdentity& identity,
    const ImageViewportProviderPreparedFrame& preparedFrame)
{
    if (!matchesResource(identity) || preparedFrame.isProvisional() || !preparedFrame.isReady()
        || !preparedFrame.envelope.isStillFrame()
        || preparedFrame.envelope.demandRevision() != identity.demandRevision
        || !preparedFrame.authoritativeStillDisplayImage.has_value()
        || !preparedFrame.authoritativeStillDisplayImage->isValid()) {
        return false;
    }

    const QMutexLocker lock(&m_stateMutex);
    const auto candidate = std::ranges::find_if(m_authoritativeFrameCandidates,
        [&identity, &preparedFrame](const AuthoritativeFrameCandidate& current) {
            return current.identity == identity
                && current.storeEntryId == preparedFrame.storeEntryId;
        });
    if (m_closed || candidate == m_authoritativeFrameCandidates.end()) {
        return false;
    }

    m_authoritativeFrameCandidates.erase(candidate);
    StaticDisplayImagePayload displayImage = *preparedFrame.authoritativeStillDisplayImage;
    displayImage.image = {};
    m_authoritativeStillDisplayImageCandidate = AuthoritativeStillDisplayImage { identity,
        preparedFrame.storeEntryId, std::move(displayImage) };
    return true;
}

bool ImageViewportProviderResource::acceptDisplayedStillDisplayImage(
    ImageViewportPageRole role, ImageViewportDemandRevisionToken demandRevision)
{
    const QMutexLocker lock(&m_stateMutex);
    if (m_currentStillDisplayImage.has_value() && m_currentStillDisplayImage->identity.role == role
        && m_currentStillDisplayImage->identity.demandRevision == demandRevision) {
        return true;
    }
    if (!m_authoritativeStillDisplayImageCandidate.has_value()
        || m_authoritativeStillDisplayImageCandidate->identity.role != role
        || m_authoritativeStillDisplayImageCandidate->identity.demandRevision != demandRevision) {
        return false;
    }

    m_currentStillDisplayImage = std::move(m_authoritativeStillDisplayImageCandidate);
    m_authoritativeStillDisplayImageCandidate.reset();
    return true;
}

void ImageViewportProviderResource::requestFrameHandle(QObject* receiver,
    const ImageViewportProviderWorkIdentity& identity,
    const ImageViewportProviderPreparedFrame& preparedFrame, FrameHandleCompletion completion)
{
    if (receiver == nullptr || !completion || !matchesResource(identity)
        || !preparedFrame.isReady()) {
        return;
    }

    const std::optional<DisplayImageStoreEntry> plannedEntry
        = m_displayStore->entry(preparedFrame.storeEntryId);
    const DisplayImageReuseKey& frameReuseKey = preparedFrame.reuseKey;
    if (!plannedEntry.has_value() || frameReuseKey.locationIdentity.isEmpty()
        || !frameReuseKey.rasterIdentity.isValid()
        || plannedEntry->image.size() != frameReuseKey.rasterSize) {
        completion(identity, {});
        return;
    }
    const ImageFrame::OrientationPolicy plannedOrientation
        = orientationPolicy(frameReuseKey.imageReaderTransformations);
    const std::optional<qsizetype> constructionPeak
        = frameConstructionPeakByteCount(plannedEntry->image, plannedOrientation);
    if (!constructionPeak.has_value() || *constructionPeak > m_workspaceBudget->aggregateByteLimit()
        || *constructionPeak > m_workspaceBudget->perOperationByteLimit()) {
        completion(identity, {});
        return;
    }

    quint64 constructionId = 0;
    {
        const QMutexLocker lock(&m_stateMutex);
        if (m_closed) {
            return;
        }
        do {
            constructionId = m_nextFrameConstructionId++;
        } while (constructionId == 0
            || std::ranges::any_of(m_activeFrameConstructions,
                [constructionId](const std::unique_ptr<ActiveFrameConstruction>& active) {
                    return active != nullptr && active->id == constructionId;
                }));
        auto active = std::make_unique<ActiveFrameConstruction>();
        active->id = constructionId;
        active->identity = identity;
        m_activeFrameConstructions.push_back(std::move(active));
    }

    const std::weak_ptr<ImageViewportProviderResource> resource = weak_from_this();
    const QMetaObject::Connection receiverDestroyed
        = QObject::connect(receiver, &QObject::destroyed, receiver, [resource, constructionId]() {
              if (const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock()) {
                  owner->cancelFrameConstruction(constructionId);
              }
          });
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (active != m_activeFrameConstructions.end()) {
            (*active)->receiverDestroyedConnection = receiverDestroyed;
        } else {
            QObject::disconnect(receiverDestroyed);
        }
    }
    auto admission = m_workspaceBudget->requestAdmission(receiver,
        ImageDecodeWorkspaceAdmissionRequest {
            *constructionPeak,
            0,
            ImageDecodeWorkspacePriority::Interactive,
        },
        [resource, receiver, constructionId, identity, storeEntryId = preparedFrame.storeEntryId,
            frameOriginalSize = frameReuseKey.originalSize,
            frameRasterSize = frameReuseKey.rasterSize, frameQuality = frameReuseKey.quality,
            frameTransformations = frameReuseKey.imageReaderTransformations,
            formatIdentifier = preparedFrame.formatIdentifier,
            completion](ImageDecodeWorkspaceLease workspaceLease) mutable {
            const std::shared_ptr<ImageViewportProviderResource> owner = resource.lock();
            if (owner == nullptr || !owner->frameConstructionIsActive(constructionId)) {
                return;
            }
            if (!owner->m_displayStore->acquireFrameLease(storeEntryId)) {
                workspaceLease = {};
                if (owner->claimFrameConstruction(constructionId)) {
                    completion(identity, {});
                }
                return;
            }

            std::shared_ptr<DisplayImageFrameLease> displayStoreLease;
            try {
                displayStoreLease
                    = std::make_shared<DisplayImageFrameLease>(owner->m_displayStore, storeEntryId);
            } catch (const std::bad_alloc&) {
                owner->m_displayStore->releaseFrameLease(storeEntryId);
                workspaceLease = {};
                if (owner->claimFrameConstruction(constructionId)) {
                    completion(identity, {});
                }
                return;
            }

            std::optional<DisplayImageStoreEntry> entry
                = owner->m_displayStore->entry(storeEntryId);
            if (!entry.has_value()) {
                displayStoreLease.reset();
                workspaceLease = {};
                if (owner->claimFrameConstruction(constructionId)) {
                    completion(identity, {});
                }
                return;
            }
            const ImageFrame::OrientationPolicy orientation
                = orientationPolicy(frameTransformations);
            const std::optional<qsizetype> actualPeak
                = frameConstructionPeakByteCount(entry->image, orientation);
            if (!actualPeak.has_value() || *actualPeak > workspaceLease.reservedByteCount()
                || entry->image.size() != frameRasterSize) {
                displayStoreLease.reset();
                workspaceLease = {};
                if (owner->claimFrameConstruction(constructionId)) {
                    completion(identity, {});
                }
                return;
            }
            const qsizetype frameRasterByteCount = entry->image.sizeInBytes();
            if (!owner->beginFrameConstructionWorker(constructionId)) {
                return;
            }

            ImageWorkerTask task = owner->m_frameConstructionScheduler.run(
                receiver,
                [entry = std::move(*entry), orientation, frameRasterByteCount, frameOriginalSize,
                    frameRasterSize, frameQuality, workspaceLease = std::move(workspaceLease),
                    displayStoreLease = std::move(displayStoreLease),
                    formatIdentifier = std::move(formatIdentifier)]() mutable {
                    FrameConstructionResult result;
                    result.displayStoreLease = std::move(displayStoreLease);
                    try {
                        QImage sourcePayload
                            = sourcePayloadForOrientation(entry.image, orientation);
                        if (sourcePayload.isNull()) {
                            return result;
                        }
                        result.frame = std::make_unique<ImageFrame>(sourcePayload,
                            frameOriginalSize, entry.byteCost, payloadQuality(frameQuality),
                            payloadExactness(frameQuality, frameOriginalSize, frameRasterSize),
                            orientation, std::move(formatIdentifier));
                    } catch (const std::bad_alloc&) {
                        return result;
                    }
                    result.retainedWorkspace = workspaceLease.retainOnly(frameRasterByteCount);
                    if (!result.retainedWorkspace.isManaged()) {
                        result.frame.reset();
                    }
                    return result;
                },
                [resource, constructionId, identity, completion = std::move(completion)](
                    FrameConstructionResult result) mutable {
                    const std::shared_ptr<ImageViewportProviderResource> completedOwner
                        = resource.lock();
                    if (completedOwner == nullptr
                        || !completedOwner->completeFrameConstruction(constructionId)) {
                        return;
                    }

                    std::unique_ptr<ImageSequenceProviderFrameHandle> handle;
                    if (result.frame != nullptr && result.retainedWorkspace.isManaged()
                        && result.displayStoreLease != nullptr) {
                        try {
                            std::function<void(ImageFrame*)> releaseFrame
                                = [displayStoreLease = std::move(result.displayStoreLease),
                                      retainedWorkspace = std::move(result.retainedWorkspace)](
                                      ImageFrame* releasedFrame) mutable {
                                      delete releasedFrame;
                                      displayStoreLease.reset();
                                      retainedWorkspace = {};
                                  };
                            auto* rawHandle = new (std::nothrow) ImageSequenceProviderFrameHandle(
                                result.frame.get(), std::move(releaseFrame));
                            if (rawHandle != nullptr) {
                                handle.reset(rawHandle);
                                [[maybe_unused]] ImageFrame* const transferredFrame
                                    = result.frame.release();
                                Q_ASSERT(transferredFrame == handle->frame());
                            }
                        } catch (const std::bad_alloc&) {
                        }
                    }
                    completion(identity, std::move(handle));
                });
            task.setRetirementCallback([resource, constructionId]() {
                if (const std::shared_ptr<ImageViewportProviderResource> active = resource.lock()) {
                    active->retireFrameConstruction(constructionId);
                }
            });
            owner->installFrameConstructionTask(constructionId, std::move(task));
        });

    if (!admission.has_value()) {
        if (claimFrameConstruction(constructionId)) {
            completion(identity, {});
        }
        return;
    }
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (active != m_activeFrameConstructions.end()) {
            (*active)->admission = std::move(*admission);
        }
    }
}

ImageSequenceProviderFailure ImageViewportProviderResource::failure(
    ImageSequenceProviderFailureCause cause, std::optional<ImageLoadFailure> failure)
{
    if (!failure.has_value()) {
        return ImageSequenceProviderFailure(cause);
    }
    return ImageSequenceProviderFailure(
        cause, m_failureRegistry->registerFailure(std::move(*failure)));
}

bool ImageViewportProviderResource::matchesResource(
    const ImageViewportProviderWorkIdentity& identity) const
{
    return identity.sourceGeneration == m_sourceGeneration
        && identity.locationIdentity == m_locationIdentity;
}

bool ImageViewportProviderResource::finalizeMetadata(
    const ImageViewportProviderWorkIdentity& identity)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find(m_activeMetadataWork, identity);
    if (m_closed || active == m_activeMetadataWork.end()) {
        return false;
    }
    m_activeMetadataWork.erase(active);
    return true;
}

bool ImageViewportProviderResource::finalizePreparedFrame(
    const ImageViewportProviderWorkIdentity& identity,
    const ImageViewportProviderPreparedFrame& preparedFrame)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find(m_activeFrameWork, identity);
    if (m_closed || active == m_activeFrameWork.end()) {
        return false;
    }
    if (preparedFrame.isProvisional()) {
        return true;
    }

    m_activeFrameWork.erase(active);
    std::erase_if(
        m_authoritativeFrameCandidates, [&identity](const AuthoritativeFrameCandidate& candidate) {
            return candidate.identity.role == identity.role;
        });
    if (preparedFrame.isReady() && preparedFrame.authoritativeStillDisplayImage.has_value()
        && preparedFrame.envelope.isStillFrame()) {
        m_authoritativeFrameCandidates.push_back(
            AuthoritativeFrameCandidate { identity, preparedFrame.storeEntryId });
    }
    return true;
}

bool ImageViewportProviderResource::frameConstructionIsActive(quint64 constructionId) const
{
    const QMutexLocker lock(&m_stateMutex);
    return !m_closed
        && std::ranges::any_of(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& active) {
                return active != nullptr && active->id == constructionId;
            });
}

bool ImageViewportProviderResource::claimFrameConstruction(quint64 constructionId)
{
    std::unique_ptr<ActiveFrameConstruction> completed;
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (m_closed || active == m_activeFrameConstructions.end()
            || (*active)->state != FrameConstructionState::WaitingForAdmission) {
            return false;
        }
        completed = std::move(*active);
        m_activeFrameConstructions.erase(active);
    }
    completed.reset();
    return true;
}

bool ImageViewportProviderResource::beginFrameConstructionWorker(quint64 constructionId)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find_if(m_activeFrameConstructions,
        [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
            return current != nullptr && current->id == constructionId;
        });
    if (m_closed || active == m_activeFrameConstructions.end() || !(*active)->publishResult
        || (*active)->state != FrameConstructionState::WaitingForAdmission) {
        return false;
    }
    (*active)->state = FrameConstructionState::StartingWorker;
    return true;
}

bool ImageViewportProviderResource::completeFrameConstruction(quint64 constructionId)
{
    const QMutexLocker lock(&m_stateMutex);
    const auto active = std::ranges::find_if(m_activeFrameConstructions,
        [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
            return current != nullptr && current->id == constructionId;
        });
    if (active == m_activeFrameConstructions.end()
        || ((*active)->state != FrameConstructionState::StartingWorker
            && (*active)->state != FrameConstructionState::Running)) {
        return false;
    }
    (*active)->state = FrameConstructionState::Retiring;
    return !m_closed && (*active)->publishResult;
}

void ImageViewportProviderResource::cancelFrameConstruction(quint64 constructionId)
{
    std::unique_ptr<ActiveFrameConstruction> canceledPending;
    ImageWorkerTask canceledTask;
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (active == m_activeFrameConstructions.end()) {
            return;
        }
        (*active)->publishResult = false;
        if ((*active)->state == FrameConstructionState::WaitingForAdmission) {
            canceledPending = std::move(*active);
            m_activeFrameConstructions.erase(active);
        } else {
            canceledTask = std::move((*active)->task);
        }
    }
    canceledPending.reset();
    canceledTask.cancel();
}

void ImageViewportProviderResource::installFrameConstructionTask(
    quint64 constructionId, ImageWorkerTask task)
{
    ImageWorkerTask canceledTask;
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (active == m_activeFrameConstructions.end()) {
            canceledTask = std::move(task);
        } else {
            (*active)->task = std::move(task);
            if ((*active)->state == FrameConstructionState::StartingWorker) {
                (*active)->state = FrameConstructionState::Running;
            }
            if (m_closed || !(*active)->publishResult) {
                canceledTask = std::move((*active)->task);
            }
        }
    }
    canceledTask.cancel();
}

void ImageViewportProviderResource::retireFrameConstruction(quint64 constructionId)
{
    std::unique_ptr<ActiveFrameConstruction> retired;
    {
        const QMutexLocker lock(&m_stateMutex);
        const auto active = std::ranges::find_if(m_activeFrameConstructions,
            [constructionId](const std::unique_ptr<ActiveFrameConstruction>& current) {
                return current != nullptr && current->id == constructionId;
            });
        if (active == m_activeFrameConstructions.end()) {
            return;
        }
        retired = std::move(*active);
        m_activeFrameConstructions.erase(active);
    }
    retired.reset();
}

ImageViewportProviderPreparedFrame ImageViewportProviderResource::prepareFrame(
    const ImageViewportProviderWorkIdentity& identity, ImageViewportProviderFrameResult result)
{
    const QString displayLocationIdentity = displayLocationIdentityForPayloadPreparation();
    ImageViewportProviderPreparedFrame prepared;
    prepared.stage = result.stage;
    prepared.envelope = result.envelope;
    prepared.envelope.setDemandRevision(identity.demandRevision);
    prepared.formatIdentifier = std::move(result.formatIdentifier);
    prepared.failureCause = result.failureCause;
    prepared.failure = std::move(result.failure);
    prepared.unsupportedCause = result.unsupportedCause;
    if (prepared.unsupportedCause.has_value()) {
        return prepared;
    }
    if (!result.displayImage.has_value() || !result.displayImage->isValid()) {
        if (!prepared.unsupportedCause.has_value()
            && prepared.failureCause == ImageSequenceProviderFailureCause::Unavailable) {
            prepared.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        }
        return prepared;
    }

    const StaticDisplayImagePayload& displayImage = *result.displayImage;
    const std::optional<DisplayImageRasterIdentity> rasterIdentity
        = rasterIdentityFor(displayImage, prepared.envelope, prepared.stage);
    if (!rasterIdentity.has_value()) {
        prepared.failureCause = ImageSequenceProviderFailureCause::ProviderInternal;
        return prepared;
    }
    const DisplayImageReuseKey reuseKey {
        displayLocationIdentity,
        displayImage.sourceIdentity,
        displayImage.sourceRevision,
        *rasterIdentity,
        displayImage.imageReaderTransform.transformations,
        displayImage.originalSize,
        displayImage.image.size(),
        displayImage.quality,
        displayImage.previewOrigin,
        displayedPageRole(identity.role),
    };
    if (result.outputAdmission == nullptr) {
        result.outputAdmission = m_displayStore->outputAdmissionForImage(displayImage.image);
        if (result.outputAdmission == nullptr) {
            result.outputAdmission
                = m_displayStore->reserveOutput(imageByteCost(displayImage.image),
                    DisplayImageOutputReservationOrigin::ProviderPreparationOutput);
        }
        if (result.outputAdmission == nullptr) {
            result.displayImage.reset();
            prepared.failureCause = ImageSequenceProviderFailureCause::ResourceExhausted;
            return prepared;
        }
    }
    prepared.outputAdmission = result.outputAdmission;
    prepared.storeEntryId = m_displayStore->acquireReusable(
        DisplayImageEntry {
            displayImage.image,
            displayImage.originalSize,
            displayImage.image.size(),
            displayImage.quality,
            DisplayImageRetentionPriority::Visible,
        },
        reuseKey, std::move(result.outputAdmission));
    if (prepared.storeEntryId.isEmpty()) {
        result.displayImage.reset();
        prepared.outputAdmission.reset();
        prepared.failureCause = ImageSequenceProviderFailureCause::ResourceExhausted;
        return prepared;
    }
    prepared.reuseKey = reuseKey;
    if (!prepared.isProvisional() && prepared.envelope.isStillFrame()) {
        const std::optional<DisplayImageStoreEntry> stored
            = m_displayStore->entry(prepared.storeEntryId);
        if (!stored.has_value()) {
            result.displayImage.reset();
            prepared.outputAdmission.reset();
            prepared.storeEntryId.clear();
            prepared.failureCause = ImageSequenceProviderFailureCause::ResourceExhausted;
            return prepared;
        }
        StaticDisplayImagePayload storedDisplayImage = displayImage;
        storedDisplayImage.image = stored->image;
        prepared.authoritativeStillDisplayImage = std::move(storedDisplayImage);
    }
    return prepared;
}

QString ImageViewportProviderResource::displayLocationIdentityForPayloadPreparation()
{
    const QMutexLocker lock(&m_stateMutex);
    m_payloadPreparationStarted = true;
    return m_displayLocationIdentity;
}
}
