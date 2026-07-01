#include "imagesequenceownership_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

using namespace ImageViewportInternal;

namespace {
bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

void mergeControllerChanges(ViewportChangeSet& target, ViewportChangeSet source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.presentation = target.presentation || source.presentation;
    target.sequence = target.sequence || source.sequence;
    target.looping = target.looping || source.looping;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}

ImageSequence* sequenceFromPageSetValue(const QVariant& value, bool& ok)
{
    if (!value.isValid() || value.isNull()) {
        ok = true;
        return nullptr;
    }

    if (value.canConvert<ImageSequence*>()) {
        if (ImageSequence* sequence = value.value<ImageSequence*>()) {
            ok = true;
            return sequence;
        }
    }

    if (value.canConvert<QObject*>()) {
        QObject* object = value.value<QObject*>();
        if (!object && value.isNull()) {
            ok = true;
            return nullptr;
        }
        if (ImageSequence* sequence = qobject_cast<ImageSequence*>(object)) {
            ok = true;
            return sequence;
        }
    }

    ok = false;
    return nullptr;
}

struct NormalizedPageSetTransitionPolicy
{
    PageSetTransitionPolicy::DisplayTransition displayTransition
        = PageSetTransitionPolicy::DisplayTransition::RetainPrevious;
    PageSetTransitionPolicy::ZoomTransition zoomTransition
        = PageSetTransitionPolicy::ZoomTransition::Preserve;
    PageSetTransitionPolicy::ContentPositionTransition contentPositionTransition
        = PageSetTransitionPolicy::ContentPositionTransition::Clamp;
    PageSetTransitionPolicy::RotationTransition rotationTransition
        = PageSetTransitionPolicy::RotationTransition::Preserve;
    PageSetTransitionPolicy::MirrorTransition mirrorTransition
        = PageSetTransitionPolicy::MirrorTransition::Preserve;
    PageSetTransitionPolicy::ReplacementIntent replacementIntent
        = PageSetTransitionPolicy::ReplacementIntent::NewTarget;
    std::optional<ImageViewport::FitMode> explicitFitMode;
    std::optional<ImageViewport::SpreadDirection> explicitSpreadDirection;
    std::optional<double> explicitPageGap;
};

bool enumIntValue(const QVariant& value, int& result)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }

    bool ok = false;
    result = value.toInt(&ok);
    return ok;
}

bool updateDisplayTransition(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::DisplayTransition>(numericValue)) {
    case PageSetTransitionPolicy::DisplayTransition::RetainPrevious:
    case PageSetTransitionPolicy::DisplayTransition::ClearBeforeLoad:
        policy.displayTransition
            = static_cast<PageSetTransitionPolicy::DisplayTransition>(numericValue);
        return true;
    }

    return false;
}

bool updateZoomTransition(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::ZoomTransition>(numericValue)) {
    case PageSetTransitionPolicy::ZoomTransition::Preserve:
    case PageSetTransitionPolicy::ZoomTransition::ResetToContain:
    case PageSetTransitionPolicy::ZoomTransition::PreserveManualPercent:
        policy.zoomTransition = static_cast<PageSetTransitionPolicy::ZoomTransition>(numericValue);
        return true;
    }

    return false;
}

bool updateContentPositionTransition(
    const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::ContentPositionTransition>(numericValue)) {
    case PageSetTransitionPolicy::ContentPositionTransition::Preserve:
    case PageSetTransitionPolicy::ContentPositionTransition::Clamp:
    case PageSetTransitionPolicy::ContentPositionTransition::ScanStart:
    case PageSetTransitionPolicy::ContentPositionTransition::ScanEnd:
        policy.contentPositionTransition
            = static_cast<PageSetTransitionPolicy::ContentPositionTransition>(numericValue);
        return true;
    }

    return false;
}

bool updateRotationTransition(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::RotationTransition>(numericValue)) {
    case PageSetTransitionPolicy::RotationTransition::Preserve:
    case PageSetTransitionPolicy::RotationTransition::Reset:
        policy.rotationTransition
            = static_cast<PageSetTransitionPolicy::RotationTransition>(numericValue);
        return true;
    }

    return false;
}

bool updateMirrorTransition(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::MirrorTransition>(numericValue)) {
    case PageSetTransitionPolicy::MirrorTransition::Preserve:
    case PageSetTransitionPolicy::MirrorTransition::Reset:
        policy.mirrorTransition
            = static_cast<PageSetTransitionPolicy::MirrorTransition>(numericValue);
        return true;
    }

    return false;
}

bool updateReplacementIntent(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    switch (static_cast<PageSetTransitionPolicy::ReplacementIntent>(numericValue)) {
    case PageSetTransitionPolicy::ReplacementIntent::NewTarget:
    case PageSetTransitionPolicy::ReplacementIntent::SameTargetRefinement:
        policy.replacementIntent
            = static_cast<PageSetTransitionPolicy::ReplacementIntent>(numericValue);
        return true;
    }

    return false;
}

bool updateExplicitFitMode(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    const ImageViewport::FitMode mode = static_cast<ImageViewport::FitMode>(numericValue);
    if (!isValidFitMode(mode)) {
        return false;
    }

    policy.explicitFitMode = mode;
    return true;
}

bool updateExplicitSpreadDirection(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    int numericValue = 0;
    if (!enumIntValue(value, numericValue)) {
        return false;
    }

    const ImageViewport::SpreadDirection direction
        = static_cast<ImageViewport::SpreadDirection>(numericValue);
    if (!isValidSpreadDirection(direction)) {
        return false;
    }

    policy.explicitSpreadDirection = direction;
    return true;
}

bool updateExplicitPageGap(const QVariant& value, NormalizedPageSetTransitionPolicy& policy)
{
    bool ok = false;
    const double pageGap = value.toDouble(&ok);
    if (!ok || !std::isfinite(pageGap) || pageGap < 0.0) {
        return false;
    }

    policy.explicitPageGap = pageGap;
    return true;
}

std::optional<NormalizedPageSetTransitionPolicy> pageSetTransitionPolicyFromMap(
    const QVariantMap& map)
{
    NormalizedPageSetTransitionPolicy policy;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (it.key() == QStringLiteral("displayTransition")) {
            if (!updateDisplayTransition(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("zoomTransition")) {
            if (!updateZoomTransition(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("contentPositionTransition")) {
            if (!updateContentPositionTransition(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("rotationTransition")) {
            if (!updateRotationTransition(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("mirrorTransition")) {
            if (!updateMirrorTransition(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("replacementIntent")) {
            if (!updateReplacementIntent(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("fitMode")) {
            if (!updateExplicitFitMode(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("spreadDirection")) {
            if (!updateExplicitSpreadDirection(it.value(), policy)) {
                return std::nullopt;
            }
        } else if (it.key() == QStringLiteral("pageGap")) {
            if (!updateExplicitPageGap(it.value(), policy)) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }

    if (policy.zoomTransition == PageSetTransitionPolicy::ZoomTransition::ResetToContain
        && policy.explicitFitMode && *policy.explicitFitMode != ImageViewport::FitMode::Contain) {
        return std::nullopt;
    }

    return policy;
}

std::optional<NormalizedPageSetTransitionPolicy> pageSetTransitionPolicyFromVariant(
    const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return NormalizedPageSetTransitionPolicy {};
    }

    if (value.canConvert<PageSetTransitionPolicy>()) {
        const PageSetTransitionPolicy policy = value.value<PageSetTransitionPolicy>();
        return NormalizedPageSetTransitionPolicy { policy.displayTransition(),
            policy.zoomTransition(), policy.contentPositionTransition(),
            policy.rotationTransition(), policy.mirrorTransition(), policy.replacementIntent() };
    }

    if (value.canConvert<QVariantMap>()) {
        return pageSetTransitionPolicyFromMap(value.toMap());
    }

    return std::nullopt;
}
}

ImageSequence* ImageViewportPrivate::sequence() const { return controller.requestState().sequence; }

void ImageViewportPrivate::setSequence(ImageSequence* sequence)
{
    if (!controller.requestState().sequence && !controller.requestState().secondarySequence
        && !sequence) {
        return;
    }

    std::shared_ptr<ImageSequence> sequenceOwner = factorySequenceOwner(sequence);
    ViewportSequenceAssignmentResult result
        = controller.assignSequence({ sequence, std::move(sequenceOwner) });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
}

ImageSequence* ImageViewportPrivate::primarySequence() const { return sequence(); }

ImageSequence* ImageViewportPrivate::secondarySequence() const
{
    return controller.requestState().secondarySequence;
}

int ImageViewportPrivate::frameCountForSequence(ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return -1;
    }
    if (sequence->isStill() || sequence->isTimedList()) {
        return sequence->frameCount();
    }
    if (!sequence->isProvider()) {
        return -1;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return sequence->m_providerKnownFacts.isTimedFrameList()
            ? sequence->m_providerKnownFrameCount
            : 1;
    }
    if (sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(sequence->m_providerFrameSeekCapability)) {
        return sequence->m_providerKnownFacts.frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDurationForSequence(ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return -1;
    }
    if (sequence->isTimedList()) {
        return sequence->totalDuration();
    }
    if (sequence->isProvider() && sequence->m_hasCompleteProviderKnownMetadata
        && sequence->m_providerKnownFacts.isTimedFrameList()
        && sequence->m_providerKnownTimingIntervals) {
        return sequence->m_providerKnownTimingIntervals->totalDuration();
    }

    return -1;
}

QVariantMap ImageViewportPrivate::frameSeekBoundsForSequence(ImageSequence* sequence) const
{
    if (frameSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceFrameCount = frameCountForSequence(sequence);
    if (sequenceFrameCount <= 0) {
        return invalidRange();
    }

    return {
        { QStringLiteral("minimum"), 0 },
        { QStringLiteral("maximum"), sequenceFrameCount - 1 },
    };
}

QVariantMap ImageViewportPrivate::positionSeekBoundsForSequence(ImageSequence* sequence) const
{
    if (positionSeekSupportForSequence(sequence) != TriState::True) {
        return invalidRange();
    }

    const int sequenceTotalDuration = totalDurationForSequence(sequence);
    if (sequenceTotalDuration < 0) {
        return invalidRange();
    }

    return {
        { QStringLiteral("minimum"), 0 },
        { QStringLiteral("maximum"), sequenceTotalDuration },
    };
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isTimedList()) {
        return TriState::True;
    }
    if (sequence->isStill()) {
        return TriState::False;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerTimedPlaybackCapability,
                   sequence->m_providerKnownFacts.isTimedFrameList())
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerTimedPlaybackCapability);
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isStill() || sequence->isTimedList()) {
        return TriState::True;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerFrameSeekCapability, true)
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerFrameSeekCapability);
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupportForSequence(
    ImageSequence* sequence) const
{
    if (!sequence || !sequence->isValid()) {
        return TriState::Unavailable;
    }
    if (sequence->isTimedList()) {
        return TriState::True;
    }
    if (sequence->isStill()) {
        return TriState::False;
    }
    if (!sequence->isProvider()) {
        return TriState::Unavailable;
    }
    if (sequence->m_hasCompleteProviderKnownMetadata) {
        return providerResolvedCapability(sequence->m_providerPositionSeekCapability,
                   sequence->m_providerKnownFacts.isTimedFrameList())
            ? TriState::True
            : TriState::False;
    }

    return capabilitySupportToTriState(sequence->m_providerPositionSeekCapability);
}

ImageViewportPrivate::SpreadDirection ImageViewportPrivate::spreadDirection() const
{
    return presentation.spreadDirection;
}

void ImageViewportPrivate::setSpreadDirectionProperty(SpreadDirection direction)
{
    setSpreadDirection(direction);
}

double ImageViewportPrivate::pageGap() const { return presentation.pageGap; }

void ImageViewportPrivate::setPageGapProperty(double gap) { setPageGap(gap); }

ImageViewportPrivate::RequestStatus ImageViewportPrivate::requestStatus() const
{
    return controller.requestState().status;
}

ImageViewportPrivate::RequestReason ImageViewportPrivate::requestReason() const
{
    return controller.requestState().reason;
}

ImageViewportPrivate::CommandReason ImageViewportPrivate::commandReason() const
{
    return controller.requestState().commandReason;
}

ImageViewportPrivate::DisplayStatus ImageViewportPrivate::displayStatus() const
{
    return controller.displayState().status;
}

ImageViewportPrivate::PlaybackPhase ImageViewportPrivate::playbackPhase() const
{
    return controller.requestState().playbackPhase;
}

int ImageViewportPrivate::displayedFrame() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::requestedFrame() const
{
    if (hasDisplayableSequence()) {
        return controller.requestState().activeRequest.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedFrame() const { return displayedFrame(); }

int ImageViewportPrivate::primaryRequestedFrame() const { return requestedFrame(); }

int ImageViewportPrivate::secondaryDisplayedFrame() const
{
    if (hasReadyDisplay() && secondarySequence()) {
        return controller.displayState().secondaryDisplayedRequest.request.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedFrame() const
{
    if (secondarySequence()) {
        return controller.requestState().secondaryActiveRequest.target.frame;
    }

    return -1;
}

int ImageViewportPrivate::displayedPosition() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::requestedPosition() const
{
    if (hasProviderSequence()
        && (controller.providerTimedMetadata()
            || controller.requestState().activeRequest.target.position >= 0)) {
        return controller.requestState().activeRequest.target.position;
    }
    if (hasTimedSequence()) {
        return controller.requestState().activeRequest.target.position;
    }

    return -1;
}

int ImageViewportPrivate::primaryDisplayedPosition() const { return displayedPosition(); }

int ImageViewportPrivate::primaryRequestedPosition() const { return requestedPosition(); }

int ImageViewportPrivate::secondaryDisplayedPosition() const
{
    if (hasReadyDisplay() && secondarySequence()) {
        return controller.displayState().secondaryDisplayedRequest.request.target.position;
    }

    return -1;
}

int ImageViewportPrivate::secondaryRequestedPosition() const
{
    ImageSequence* sequence = secondarySequence();
    if (!sequence) {
        return -1;
    }
    if (sequence->isProvider()
        && (controller.secondaryProviderTimedMetadata()
            || controller.requestState().secondaryActiveRequest.target.position >= 0)) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }
    if (sequence->isTimedList()) {
        return controller.requestState().secondaryActiveRequest.target.position;
    }

    return -1;
}

int ImageViewportPrivate::frameCount() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerTimedMetadata() ? controller.providerFrameCount() : 1;
    }
    if (hasProviderSequence() && !controller.providerMetadataReady()
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability)) {
        return controller.requestState().sequence->m_providerKnownFacts.frameCount();
    }
    if (hasDisplayableSequence()) {
        return controller.requestState().sequence->frameCount();
    }

    return -1;
}

int ImageViewportPrivate::totalDuration() const
{
    if (hasProviderSequence() && controller.providerTimedMetadata()) {
        return controller.providerTotalDuration();
    }
    if (hasTimedSequence()) {
        return controller.requestState().sequence->totalDuration();
    }

    return -1;
}

bool ImageViewportPrivate::hasSecondaryTimedSequence() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList();
}

int ImageViewportPrivate::secondarySequenceFrameCount() const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && !sequence->isProvider() ? sequence->frameCount() : -1;
}

int ImageViewportPrivate::secondarySequenceFrameIndexForPosition(int position) const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList() ? sequence->frameIndexForPosition(position) : -1;
}

int ImageViewportPrivate::secondarySequenceFrameStartPosition(int frame) const
{
    ImageSequence* sequence = secondarySequence();
    return sequence && sequence->isTimedList() ? sequence->frameStartPosition(frame) : -1;
}

QVariantMap ImageViewportPrivate::frameSeekBounds() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        if (!controller.providerFrameSeekSupported()) {
            return invalidRange();
        }
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                controller.providerTimedMetadata() ? controller.providerFrameCount() - 1 : 0 },
        };
    }
    if (hasProviderSequence() && !controller.providerMetadataReady()
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount()
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability)) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                controller.requestState().sequence->m_providerKnownFacts.frameCount() - 1 },
        };
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.requestState().sequence->frameCount() - 1 },
        };
    }

    return invalidRange();
}

QVariantMap ImageViewportPrivate::positionSeekBounds() const
{
    if (hasProviderSequence() && controller.providerTimedMetadata()
        && controller.providerPositionSeekSupported()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.providerTotalDuration() },
        };
    }
    if (hasTimedSequence()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.requestState().sequence->totalDuration() },
        };
    }

    return invalidRange();
}

int ImageViewportPrivate::primaryFrameCount() const { return frameCount(); }

int ImageViewportPrivate::secondaryFrameCount() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderTimedMetadata()
            ? controller.secondaryProviderFrameCount()
            : 1;
    }
    return frameCountForSequence(secondarySequence());
}

int ImageViewportPrivate::primaryTotalDuration() const { return totalDuration(); }

int ImageViewportPrivate::secondaryTotalDuration() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderTimedMetadata()) {
        return controller.secondaryProviderTotalDuration();
    }
    return totalDurationForSequence(secondarySequence());
}

QVariantMap ImageViewportPrivate::primaryFrameSeekBounds() const { return frameSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryFrameSeekBounds() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        if (!controller.secondaryProviderFrameSeekSupported()) {
            return invalidRange();
        }
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"),
                controller.secondaryProviderTimedMetadata()
                    ? controller.secondaryProviderFrameCount() - 1
                    : 0 },
        };
    }
    return frameSeekBoundsForSequence(secondarySequence());
}

QVariantMap ImageViewportPrivate::primaryPositionSeekBounds() const { return positionSeekBounds(); }

QVariantMap ImageViewportPrivate::secondaryPositionSeekBounds() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderTimedMetadata()
        && controller.secondaryProviderPositionSeekSupported()) {
        return {
            { QStringLiteral("minimum"), 0 },
            { QStringLiteral("maximum"), controller.secondaryProviderTotalDuration() },
        };
    }
    return positionSeekBoundsForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::timedPlaybackSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerTimedPlaybackSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerTimedPlaybackCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::frameSeekSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerFrameSeekSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerFrameSeekCapability);
    }
    if (hasStillSequence() || hasTimedSequence()) {
        return TriState::True;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::positionSeekSupport() const
{
    if (hasProviderSequence() && controller.providerMetadataReady()) {
        return controller.providerPositionSeekSupported() ? TriState::True : TriState::False;
    }
    if (hasProviderSequence()) {
        return capabilitySupportToTriState(
            controller.requestState().sequence->m_providerPositionSeekCapability);
    }
    if (hasTimedSequence()) {
        return TriState::True;
    }
    if (hasStillSequence()) {
        return TriState::False;
    }

    return TriState::Unavailable;
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryTimedPlaybackSupport() const
{
    return timedPlaybackSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryTimedPlaybackSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderTimedPlaybackSupported() ? TriState::True
                                                                    : TriState::False;
    }
    return timedPlaybackSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryFrameSeekSupport() const
{
    return frameSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryFrameSeekSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderFrameSeekSupported() ? TriState::True : TriState::False;
    }
    return frameSeekSupportForSequence(secondarySequence());
}

ImageViewportPrivate::TriState ImageViewportPrivate::primaryPositionSeekSupport() const
{
    return positionSeekSupport();
}

ImageViewportPrivate::TriState ImageViewportPrivate::secondaryPositionSeekSupport() const
{
    ImageSequence* sequence = secondarySequence();
    if (sequence && sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderPositionSeekSupported() ? TriState::True
                                                                   : TriState::False;
    }
    return positionSeekSupportForSequence(secondarySequence());
}

QSizeF ImageViewportPrivate::displayedImageSize() const
{
    if (hasReadyDisplay()) {
        return controller.displayState().displayedImageSize;
    }

    return QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::displayedSpreadSize() const
{
    const QSizeF primarySize = primaryDisplayedImageSize();
    if (!isPositiveSize(primarySize)) {
        return QSizeF(0.0, 0.0);
    }

    const QSizeF secondarySize = secondaryDisplayedImageSize();
    if (!isPositiveSize(secondarySize)) {
        return primarySize;
    }

    return QSizeF(primarySize.width() + presentation.pageGap + secondarySize.width(),
        std::max(primarySize.height(), secondarySize.height()));
}

QSizeF ImageViewportPrivate::primaryDisplayedImageSize() const { return displayedImageSize(); }

QSizeF ImageViewportPrivate::secondaryDisplayedImageSize() const
{
    if (!hasReadyDisplay()) {
        return QSizeF(0.0, 0.0);
    }

    const QSizeF size = secondaryLogicalSize();
    return isPositiveSize(size) ? size : QSizeF(0.0, 0.0);
}

QSizeF ImageViewportPrivate::secondaryLogicalSize() const
{
    ImageSequence* sequence = secondarySequence();
    if (!sequence || !sequence->isValid()) {
        return {};
    }
    if (sequence->isProvider() && controller.secondaryProviderMetadataReady()) {
        return controller.secondaryProviderLogicalSize();
    }
    if (!sequence->isProvider()) {
        return sequence->logicalSize();
    }

    return {};
}

uint ImageViewportPrivate::displayRevision() const { return controller.displayState().revision; }

uint ImageViewportPrivate::requestRevision() const
{
    return controller.requestState().requestRevision;
}

uint ImageViewportPrivate::commandRevision() const
{
    return controller.requestState().commandRevision;
}

QString ImageViewportPrivate::errorString() const { return controller.requestState().errorString; }

QString ImageViewportPrivate::warningString() const
{
    return controller.requestState().warningString;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageSet(
    const QVariant& primary, const QVariant& secondary, const QVariant& policy)
{
    bool primaryValid = false;
    bool secondaryValid = false;
    ImageSequence* primarySequence = sequenceFromPageSetValue(primary, primaryValid);
    ImageSequence* secondarySequence = sequenceFromPageSetValue(secondary, secondaryValid);

    if (!primaryValid || !secondaryValid) {
        return CommandOutcome::Invalid;
    }

    const std::optional<NormalizedPageSetTransitionPolicy> transitionPolicy
        = pageSetTransitionPolicyFromVariant(policy);
    if (!transitionPolicy) {
        const ViewportCommandResult result = controller.rejectInvalidCommand();
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    if (!primarySequence) {
        return clear();
    }

    ViewportChangeSet transitionChanges;
    auto markPresentationChanged = [&](bool affectsGeometry) {
        transitionChanges.presentation = true;
        transitionChanges.displayRevision = true;
        transitionChanges.geometryState = transitionChanges.geometryState
            || (affectsGeometry && hasReadyDisplay() && !itemBounds().isEmpty());
        transitionChanges.scheduleUpdate = true;
    };

    if (transitionPolicy->zoomTransition
        == PageSetTransitionPolicy::ZoomTransition::ResetToContain) {
        if (presentation.fitMode != FitMode::Contain || presentation.zoom != 1.0) {
            presentation.fitMode = FitMode::Contain;
            presentation.zoom = 1.0;
            markPresentationChanged(true);
        }
    }
    if (transitionPolicy->explicitFitMode
        && presentation.fitMode != *transitionPolicy->explicitFitMode) {
        presentation.fitMode = *transitionPolicy->explicitFitMode;
        markPresentationChanged(true);
    }
    if (transitionPolicy->rotationTransition == PageSetTransitionPolicy::RotationTransition::Reset
        && presentation.rotationDegrees != 0) {
        presentation.rotationDegrees = 0;
        markPresentationChanged(true);
    }
    if (transitionPolicy->mirrorTransition == PageSetTransitionPolicy::MirrorTransition::Reset
        && (presentation.mirrorHorizontally || presentation.mirrorVertically)) {
        presentation.mirrorHorizontally = false;
        presentation.mirrorVertically = false;
        markPresentationChanged(true);
    }
    if (transitionPolicy->explicitSpreadDirection
        && presentation.spreadDirection != *transitionPolicy->explicitSpreadDirection) {
        presentation.spreadDirection = *transitionPolicy->explicitSpreadDirection;
        markPresentationChanged(true);
    }
    if (transitionPolicy->explicitPageGap
        && presentation.pageGap != *transitionPolicy->explicitPageGap) {
        presentation.pageGap = *transitionPolicy->explicitPageGap;
        markPresentationChanged(true);
    }

    std::shared_ptr<ImageSequence> primaryOwner = factorySequenceOwner(primarySequence);
    std::shared_ptr<ImageSequence> secondaryOwner = factorySequenceOwner(secondarySequence);
    ImageViewportInternal::DisplayRequestTarget secondaryInitialTarget;
    ImageViewportInternal::ResolvedFrameIdentity secondaryInitialResolvedFrame;
    if (secondarySequence && secondarySequence->isValid() && !secondarySequence->isProvider()) {
        const int position = secondarySequence->isTimedList()
            ? secondarySequence->frameStartPosition(0)
            : -1;
        secondaryInitialTarget = {
            0, position, ImageViewportInternal::ProviderRequestTargetKind::Unknown
        };
        secondaryInitialResolvedFrame = { 0, position };
    }
    ViewportSequenceAssignmentResult result = controller.assignSequence(
        { primarySequence, std::move(primaryOwner), secondarySequence, std::move(secondaryOwner),
            secondaryInitialTarget, secondaryInitialResolvedFrame,
            transitionPolicy->displayTransition
                == PageSetTransitionPolicy::DisplayTransition::RetainPrevious,
            secondarySequence && secondarySequence->isProvider() });
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    if (result.openProviderSession && !openProviderSession()) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    if (result.openSecondaryProviderSession && !openProviderSession(PageRole::Secondary)) {
        mergeControllerChanges(result.changes,
            controller.handleProviderSessionOpenFailure(
                QStringLiteral("provider session creation failed")));
    }
    mergeControllerChanges(result.changes, transitionChanges);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return CommandOutcome::Accepted;
}
