#include "viewportcontrollerhelpers_p.h"

ViewportMetadataProjection ViewportController::metadataProjection(
    ImageViewport::PageRole role) const
{
    if (role == ImageViewport::PageRole::Secondary) {
        if (!state.request.secondarySequence) {
            return {};
        }
        if (state.request.secondarySequenceIsProvider) {
            const ImageViewportInternal::ProviderGenerationState& provider
                = providerGenerationStateForRole(state, role);
            if (provider.metadataReady) {
                return projectProviderRuntimeMetadata(provider);
            }
            return projectProviderConstructionMetadata(viewport.secondaryProviderKnownFacts(),
                viewport.secondaryProviderTimedPlaybackCapability(),
                viewport.secondaryProviderFrameSeekCapability(),
                viewport.secondaryProviderPositionSeekCapability());
        }
        return projectBuiltInMetadata(true, viewport.hasSecondaryTimedSequence(),
            viewport.secondarySequenceFrameCount(), viewport.secondarySequenceTotalDuration());
    }

    if (!viewport.hasDisplayableSequence()) {
        return {};
    }
    if (viewport.hasProviderSequence()) {
        const ImageViewportInternal::ProviderGenerationState& provider
            = providerGenerationStateForRole(state, ImageViewport::PageRole::Primary);
        if (provider.metadataReady) {
            return projectProviderRuntimeMetadata(provider);
        }
        return projectProviderConstructionMetadata(viewport.providerKnownFacts(),
            viewport.providerTimedPlaybackCapability(), viewport.providerFrameSeekCapability(),
            viewport.providerPositionSeekCapability());
    }
    return projectBuiltInMetadata(true, viewport.hasTimedSequence(), viewport.sequenceFrameCount(),
        viewport.sequenceTotalDuration());
}
