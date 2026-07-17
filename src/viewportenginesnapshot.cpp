#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"

ImageViewportStateSnapshot ViewportEngine::snapshot() const
{
    return projectViewportStateSnapshot(
        { acceptedGeometry(), currentGeometry() }, snapshotAccess());
}
