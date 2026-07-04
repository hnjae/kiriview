#include "imagesequenceownership_p.h"

#include "imagesequence_p.h"

#include <mutex>
#include <unordered_map>

namespace {

std::mutex& ownerMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<ImageSequence*, std::weak_ptr<ImageSequence>>& owners()
{
    static std::unordered_map<ImageSequence*, std::weak_ptr<ImageSequence>> map;
    return map;
}

}

namespace ImageViewportInternal {

ImageSequenceSource makeImageSequenceSource(
    ImageSequence* sequence, std::shared_ptr<ImageSequence> owner)
{
    ImageSequenceSource source;
    source.sequence = sequence;
    source.owner = std::move(owner);
    if (!ImageSequencePrivateAccess::isValid(sequence)) {
        return source;
    }

    source.facts.present = true;
    source.facts.provider = ImageSequencePrivateAccess::isProvider(sequence);
    source.facts.timed = ImageSequencePrivateAccess::isTimedList(sequence);
    source.facts.authoredAnimationFacts
        = ImageSequencePrivateAccess::authoredAnimationFacts(sequence);
    if (source.facts.provider) {
        source.facts.hasCompleteProviderKnownMetadata
            = ImageSequencePrivateAccess::hasCompleteProviderKnownMetadata(sequence);
        source.facts.providerKnownFacts
            = ImageSequencePrivateAccess::providerKnownFacts(sequence);
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
    source.facts.firstFramePosition = source.facts.timed
        ? ImageSequencePrivateAccess::frameStartPosition(sequence, 0)
        : -1;
    source.facts.timingIntervals = source.facts.timed
        ? ImageSequencePrivateAccess::timingIntervals(sequence)
        : TimingIntervals {};
    return source;
}

void registerFactorySequenceOwner(const std::shared_ptr<ImageSequence>& sequence)
{
    if (!sequence) {
        return;
    }

    {
        std::lock_guard lock(ownerMutex());
        owners()[sequence.get()] = sequence;
    }

    QObject::connect(sequence.get(), &QObject::destroyed, sequence.get(), [](QObject* object) {
        std::lock_guard lock(ownerMutex());
        owners().erase(static_cast<ImageSequence*>(object));
    });
}

ImageSequenceSource factorySequenceSource(ImageSequence* sequence)
{
    if (!sequence) {
        return {};
    }

    std::lock_guard lock(ownerMutex());
    auto iterator = owners().find(sequence);
    if (iterator == owners().end()) {
        return makeImageSequenceSource(sequence);
    }

    std::shared_ptr<ImageSequence> owner = iterator->second.lock();
    if (!owner) {
        owners().erase(iterator);
        return {};
    }
    return makeImageSequenceSource(sequence, std::move(owner));
}

std::shared_ptr<ImageSequence> factorySequenceOwner(ImageSequence* sequence)
{
    return factorySequenceSource(sequence).owner;
}

}
