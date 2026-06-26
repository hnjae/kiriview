#include "imagesequenceownership_p.h"

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

std::shared_ptr<ImageSequence> factorySequenceOwner(ImageSequence* sequence)
{
    if (!sequence) {
        return {};
    }

    std::lock_guard lock(ownerMutex());
    auto iterator = owners().find(sequence);
    if (iterator == owners().end()) {
        return {};
    }

    std::shared_ptr<ImageSequence> owner = iterator->second.lock();
    if (!owner) {
        owners().erase(iterator);
    }
    return owner;
}

}
