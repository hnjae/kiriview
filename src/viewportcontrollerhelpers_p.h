#pragma once

#include "imageviewportstate_p.h"

inline void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    ImageViewportInternal::ViewportChangeSet source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    if (source.commandRevisionValue != 0) {
        target.commandRevisionValue = source.commandRevisionValue;
    }
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
    if (source.renderFailureDiagnostic.valid) {
        target.renderFailureDiagnostic = source.renderFailureDiagnostic;
    }
}
