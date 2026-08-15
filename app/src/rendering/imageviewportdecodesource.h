// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEVIEWPORTDECODESOURCE_H
#define KIRIVIEW_IMAGEVIEWPORTDECODESOURCE_H

#include "async/imageasyncworker.h"
#include "async/timerscheduler.h"
#include "decoding/animationsourceruntime.h"
#include "decoding/decodedimageresult.h"
#include "decoding/imageanimationrequest.h"
#include "decoding/imageanimationsourcecatalog.h"
#include "decoding/imagedecodedependencies.h"
#include "decoding/imagedecodejob.h"
#include "document/imageloadtypes.h"
#include "imageviewportproviderresource.h"
#include "metadata/embeddedmetadata.h"

#include <QObject>
#include <QString>
#include <list>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
class ImageViewportDecodeProviderSourceTestAccess;

enum class ImageViewportProvisionalPreviewPolicy {
    Allow,
    Suppress,
};

class ImageViewportDecodeProviderSource final
    : public QObject,
      public ImageViewportProviderSource,
      public std::enable_shared_from_this<ImageViewportDecodeProviderSource>
{
public:
    explicit ImageViewportDecodeProviderSource(ImageDecodeDependencies dependencies,
        ImageViewportProvisionalPreviewPolicy provisionalPreviewPolicy
        = ImageViewportProvisionalPreviewPolicy::Allow,
        TimerScheduler initialDetailTimerScheduler = {});
    ImageViewportDecodeProviderSource(ImageLoadSession session,
        ImageDecodeDependencies dependencies,
        std::optional<StaticDisplayImagePayload> authoritativeSeed = std::nullopt,
        TimerScheduler initialDetailTimerScheduler = {});
    ~ImageViewportDecodeProviderSource() override;
    Q_DISABLE_COPY_MOVE(ImageViewportDecodeProviderSource)

    [[nodiscard]] const EmbeddedMetadata& embeddedMetadata() const;
    [[nodiscard]] ImageSequenceProviderMetadata constructionMetadata() const override;
    void requestMetadata(
        const ImageViewportProviderWorkIdentity& identity, MetadataCompletion completion) override;
    void requestFrame(const ImageViewportProviderWorkIdentity& identity,
        ImageViewportProviderFrameRequest request, FrameCompletion completion) override;
    void cancel(const QVector<ImageSequenceProviderRequestToken>& tokens) override;
    void close() override;
    bool resolveSession(ImageLoadSession session,
        std::optional<StaticDisplayImagePayload> authoritativeSeed = std::nullopt);

private:
    friend class ImageViewportDecodeProviderSourceTestAccess;

    struct PendingMetadata
    {
        ImageViewportProviderWorkIdentity identity;
        MetadataCompletion completion;
    };

    struct PendingFrame
    {
        ImageViewportProviderWorkIdentity identity;
        ImageViewportProviderFrameRequest request;
        FrameCompletion completion;
        bool provisionalPublished = false;
    };

    struct AnimationState
    {
        std::shared_ptr<AnimationSourceRuntime> runtime;
        ImageSequenceProviderMetadata metadata;
        QString sourceIdentity;
        ImageSourceRevision sourceRevision;
        QString formatIdentifier;
        qsizetype frameDisplayByteCount = 0;
    };

    struct WorkerUnit
    {
        quint64 id = 0;
        std::optional<ImageViewportProviderWorkIdentity> identity;
        ImageWorkerTask task;
    };

    struct ActiveAnimationFrameWork
    {
        quint64 workerUnitId = 0;
        ImageViewportProviderWorkIdentity identity;
        ImageDecodeWorkspaceAdmission workspaceAdmission;
        bool publishResult = true;
    };

    struct StaticFramePlan
    {
        QSize targetRasterSize;
        bool requireExact = false;
    };

    enum class StaticFrameResolution {
        CandidateSelection,
        RefinementDecodeFailure,
        RefinementContractViolation,
        RefinementResourceExhausted,
        RefinementUnsupported,
    };

    struct StaticFrameAttempt
    {
        quint64 id = 0;
        PendingFrame pending;
        StaticFramePlan plan;
        std::shared_ptr<DisplayImageOutputAdmission> fallbackBasisOutputAdmission;
        StaticDisplayImagePayload fallbackBasis;
        std::shared_ptr<DisplayImageOutputAdmission> deadlineCandidateOutputAdmission;
        std::optional<StaticDisplayImagePayload> deadlineCandidate;
        quint64 refinementWorkerUnitId = 0;
        bool initialDemand = false;
        bool deadlineExpired = false;
        std::unique_ptr<RuntimeTimerHandle> initialDetailTimer;
    };

    struct StaticRefinementWork
    {
        quint64 workerUnitId = 0;
        QSize targetRasterSize;
        qsizetype producerPeakByteCost = 0;
        std::shared_ptr<DisplayImageOutputAdmission> basisOutputAdmission;
        StaticDisplayImagePayload basis;
        std::vector<quint64> attemptIds;
        qint64 maximumReusableBytes = -1;
        std::shared_ptr<DisplayImageStore> outputStore;
        std::shared_ptr<DisplayImageOutputAdmission> outputAdmission;
        ImageDecodeWorkspaceAdmission workspaceAdmission;
        bool retainWithoutSubscribers = false;
        bool productionStarted = false;
        bool retiring = false;
    };

    void ensureDecoded();
    void finishDecode(const ImageDecodeRequest& request, DecodedImageResult result);
    void finishDataLoadError(const ImageDecodeRequest& request, const ImageDataLoadError& error);
    void finishThumbnail(const ImageDecodeRequest& request, StaticDisplayImagePayload displayImage);
    void finishDecodedImage(DecodedImage image);
    void finishStaticImage(StaticDecodedImage image);
    void finishAnimationImage(ImageDecodeWorkspaceHold firstFrameWorkspaceHold, QImage firstFrame,
        ImageAnimationSourceCatalog catalog, ImageAnimationPlaybackRequest playbackRequest,
        QString sourceIdentity, ImageSourceRevision sourceRevision, QString formatIdentifier);
    void finishFailure(ImageSequenceProviderFailureCause cause, ImageLoadFailure failure);
    void publishMetadata();
    void publishFrames();
    void publishProvisionalFrames();
    void publishStaticFrame(PendingFrame pending);
    void startStaticRefinement(PendingFrame pending, StaticFramePlan plan, bool initialDemand);
    void startGrantedStaticRefinement(
        quint64 workerUnitId, ImageDecodeWorkspaceLease rasterWorkspaceLease);
    void finishStaticRefinement(quint64 workerUnitId, const StaticImageDisplayDecodeResult& result);
    void scheduleInitialDetailDeadline(quint64 attemptId);
    void markInitialDetailDeadlineExpired(quint64 attemptId);
    void finishInitialDetailDeadline(quint64 attemptId);
    void finishStaticFrameAttempt(StaticFrameAttempt attempt,
        const StaticImageDisplayDecodeDiagnostics& diagnostics = {},
        std::optional<StaticDisplayImagePayload> preferredCandidate = std::nullopt,
        bool considerCurrentCandidate = true,
        StaticFrameResolution resolution = StaticFrameResolution::CandidateSelection,
        std::shared_ptr<DisplayImageOutputAdmission> preferredOutputAdmission = {});
    [[nodiscard]] quint64 reserveStaticFrameAttemptId();
    [[nodiscard]] std::optional<StaticFrameAttempt> takeStaticFrameAttempt(
        quint64 attemptId, bool retainRefinementWork);
    void discardRetainedStaticRefinementsExcept(quint64 workerUnitId);
    void publishAnimationFrame(PendingFrame pending);
    void startGrantedAnimationFrame(PendingFrame pending, AnimationState animation,
        int requestedFrame, qsizetype perOperationBaselineByteCount, qsizetype outputByteCount,
        ImageDecodeWorkspaceLease workspaceLease);
    void retireAnimationFrameWork();
    void finishAnimationFrame(const PendingFrame& pending, const AnimationState& animation,
        int requestedFrame, AnimationSourceFrameResult result,
        std::shared_ptr<DisplayImageOutputAdmission> outputAdmission = {});
    quint64 reserveWorkerUnit(
        std::optional<ImageViewportProviderWorkIdentity> identity = std::nullopt);
    void attachWorkerTask(quint64 workerUnitId, ImageWorkerTask task);
    void cancelWorkerUnit(quint64 workerUnitId);
    void physicallyRetireWorkerUnit(quint64 workerUnitId);
    [[nodiscard]] bool hasWorkerUnit(quint64 workerUnitId) const;
    [[nodiscard]] bool detachWorkerUnit(quint64 workerUnitId);
    [[nodiscard]] const ImageLoadSession& resolvedSession() const;

    std::optional<ImageLoadSession> m_session;
    ImageDecodeDependencies m_dependencies;
    ImageViewportProvisionalPreviewPolicy m_provisionalPreviewPolicy
        = ImageViewportProvisionalPreviewPolicy::Allow;
    TimerScheduler m_initialDetailTimerScheduler;
    ImageDecodeJob m_decodeJob;
    EmbeddedMetadata m_embeddedMetadata;
    std::optional<ImageSequenceProviderMetadata> m_metadata;
    std::optional<StaticDisplayImagePayload> m_provisionalPreview;
    std::shared_ptr<DisplayImageOutputAdmission> m_authoritativeStaticImageOutputAdmission;
    std::optional<StaticDisplayImagePayload> m_authoritativeStaticImage;
    std::optional<StaticDisplayImagePayload> m_authoritativeSeed;
    std::optional<AnimationState> m_animation;
    std::optional<ImageLoadFailure> m_failure;
    ImageSequenceProviderFailureCause m_failureCause
        = ImageSequenceProviderFailureCause::Unavailable;
    std::vector<PendingMetadata> m_pendingMetadata;
    std::vector<PendingFrame> m_pendingFrames;
    std::vector<PendingFrame> m_deferredStaticFrames;
    std::vector<WorkerUnit> m_workerUnits;
    std::list<StaticFrameAttempt> m_staticFrameAttempts;
    std::list<StaticRefinementWork> m_staticRefinementWorks;
    std::optional<ActiveAnimationFrameWork> m_activeAnimationFrameWork;
    quint64 m_nextWorkerUnitId = 1;
    quint64 m_nextStaticFrameAttemptId = 1;
    bool m_decodeStarted = false;
    bool m_decodeComplete = false;
    bool m_publishingFrames = false;
    bool m_closed = false;
};
}

#endif
