#pragma once

#include "imageviewport.h"

class FramePreparation
{
public:
    struct ProviderFrameState
    {
        bool metadataReady = false;
        bool timedMetadata = false;
        QSizeF logicalSize;
        QVector<int> frameDurations;
        int currentFrame = -1;
    };

    static bool validateProviderStillMetadata(const ImageSequenceProviderMetadata& metadata);
    static bool validateProviderTimedMetadata(const ImageSequenceProviderMetadata& metadata);
    static bool exceedsPayloadLimit(const ImageFrame* frame);
    static bool validateProviderFrame(ImageFrame* frame,
        const ImageSequenceProviderFrameMetadata& metadata, const ProviderFrameState& state);
    static int providerFrameStartPosition(const QVector<int>& frameDurations, int frame);
    static int providerFrameIndexForPosition(const QVector<int>& frameDurations, int position);
    static int totalDuration(const QVector<int>& frameDurations);
    static QString boundedDiagnostic(const QString& diagnostic, const QString& fallback);
};
