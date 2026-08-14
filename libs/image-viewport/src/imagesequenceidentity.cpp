// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <ImageViewport/imageviewporttypes.h>

ImageSequenceAuthoredAnimationFacts ImageSequenceAuthoredAnimationFacts::finiteLoop(int loopCount)
{
    ImageSequenceAuthoredAnimationFacts facts;
    facts.setFiniteLoopCount(loopCount);
    return facts;
}

ImageSequenceAuthoredAnimationFacts ImageSequenceAuthoredAnimationFacts::infiniteLoop()
{
    ImageSequenceAuthoredAnimationFacts facts;
    facts.m_loopMode = ImageSequenceAuthoredAnimationLoopMode::Infinite;
    facts.m_loopCount = -1;
    return facts;
}

bool ImageSequenceAuthoredAnimationFacts::autoplay() const { return m_autoplay; }

void ImageSequenceAuthoredAnimationFacts::setAutoplay(bool autoplay) { m_autoplay = autoplay; }

ImageSequenceAuthoredAnimationLoopMode ImageSequenceAuthoredAnimationFacts::loopMode() const
{
    return m_loopMode;
}

int ImageSequenceAuthoredAnimationFacts::loopCount() const { return m_loopCount; }

bool ImageSequenceAuthoredAnimationFacts::setFiniteLoopCount(int loopCount)
{
    if (loopCount < 2) {
        return false;
    }

    m_loopMode = ImageSequenceAuthoredAnimationLoopMode::Finite;
    m_loopCount = loopCount;
    return true;
}

bool ImageSequenceAuthoredAnimationFacts::isValid() const
{
    switch (m_loopMode) {
    case ImageSequenceAuthoredAnimationLoopMode::Unavailable:
        return m_loopCount == -1;
    case ImageSequenceAuthoredAnimationLoopMode::PlayOnce:
        return m_loopCount == 1;
    case ImageSequenceAuthoredAnimationLoopMode::Finite:
        return m_loopCount >= 2;
    case ImageSequenceAuthoredAnimationLoopMode::Infinite:
        return m_loopCount == -1;
    }
    return false;
}
