// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imagesequencesource_p.h"

#include "imagesequence_p.h"

namespace ImageViewportInternal {

ImageSequenceSource makeImageSequenceSource(
    ImageSequence* sequence, std::shared_ptr<ImageSequence> owner)
{
    ImageSequenceSource source;
    source.sequence = sequence;
    source.owner = owner ? std::move(owner) : ImageSequencePrivateAccess::owner(sequence);
    if (!ImageSequencePrivateAccess::isValid(sequence)) {
        return source;
    }

    source.facts.present = true;
    source.facts.provider = ImageSequencePrivateAccess::isProvider(sequence);
    source.facts.timed = ImageSequencePrivateAccess::isTimedList(sequence);
    source.facts.authoredAnimationFacts
        = ImageSequencePrivateAccess::authoredAnimationFacts(sequence);
    source.facts.authoredAnimationFactsAvailable
        = ImageSequencePrivateAccess::authoredAnimationFactsAvailable(sequence);
    source.facts.logicalSize = ImageSequencePrivateAccess::logicalSize(sequence);
    if (source.facts.provider) {
        source.facts.hasCompleteProviderKnownMetadata
            = ImageSequencePrivateAccess::hasCompleteProviderKnownMetadata(sequence);
        source.facts.providerKnownFacts = ImageSequencePrivateAccess::providerKnownFacts(sequence);
        source.facts.providerKnownLogicalSize
            = ImageSequencePrivateAccess::providerKnownLogicalSize(sequence);
        source.facts.providerKnownTimingIntervals
            = ImageSequencePrivateAccess::providerKnownTimingIntervals(sequence);
        source.facts.providerTimedPlaybackCapability
            = ImageSequencePrivateAccess::providerTimedPlaybackCapability(sequence);
        source.facts.providerFrameSeekCapability
            = ImageSequencePrivateAccess::providerFrameSeekCapability(sequence);
        source.facts.providerPositionSeekCapability
            = ImageSequencePrivateAccess::providerPositionSeekCapability(sequence);
        source.facts.providerThreadingContract
            = ImageSequencePrivateAccess::providerThreadingContract(sequence);
        source.providerSessionFactory
            = ImageSequencePrivateAccess::providerSessionFactory(sequence);
        return source;
    }

    source.facts.frameCount = ImageSequencePrivateAccess::frameCount(sequence);
    source.facts.totalDuration = ImageSequencePrivateAccess::totalDuration(sequence);
    source.facts.firstFramePosition
        = source.facts.timed ? ImageSequencePrivateAccess::frameStartPosition(sequence, 0) : -1;
    source.facts.timingIntervals = source.facts.timed
        ? ImageSequencePrivateAccess::timingIntervals(sequence)
        : TimingIntervals {};
    return source;
}

ImageSequenceSource factorySequenceSource(ImageSequence* sequence)
{
    return makeImageSequenceSource(sequence);
}

bool sourceIsStill(const ImageSequenceSource& source)
{
    return source.facts.present && !source.facts.provider && !source.facts.timed;
}

int sourceFrameStartPosition(const ImageSequenceSource& source, int frame)
{
    if (!source.facts.timed) {
        return -1;
    }
    if (source.facts.timingIntervals.isValid()) {
        return source.facts.timingIntervals.frameStartPosition(frame);
    }
    return ImageSequencePrivateAccess::frameStartPosition(source.sequence, frame);
}

int sourceFrameIndexForPosition(const ImageSequenceSource& source, int position)
{
    if (!source.facts.timed) {
        return -1;
    }
    if (source.facts.timingIntervals.isValid()) {
        return source.facts.timingIntervals.frameIndexForPosition(position);
    }
    return ImageSequencePrivateAccess::frameIndexForPosition(source.sequence, position);
}

QSizeF sourceLogicalSize(const ImageSequenceSource& source)
{
    if (source.facts.provider) {
        return source.facts.providerKnownLogicalSize;
    }
    return source.facts.logicalSize;
}

QImage sourceFrameImage(const ImageSequenceSource& source, int frame)
{
    return ImageSequencePrivateAccess::frameImage(source.sequence, frame);
}

FramePayload sourceFramePayload(const ImageSequenceSource& source, int frame)
{
    return ImageSequencePrivateAccess::framePayload(source.sequence, frame);
}

FramePayloadFacts sourceFramePayloadFacts(const ImageSequenceSource& source, int frame)
{
    return ImageSequencePrivateAccess::framePayloadFacts(source.sequence, frame);
}

}
