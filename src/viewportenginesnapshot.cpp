#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"

ImageViewportStateSnapshot ViewportEngine::snapshot(ViewportEngineViewportInput input) const
{
    return projectViewportStateSnapshot(
        { acceptedGeometry(input), currentGeometry(input) }, snapshotAccess());
}
