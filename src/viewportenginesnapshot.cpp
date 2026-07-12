#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"

namespace {
bool positiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}
}

ImageViewportStateSnapshot ViewportEngine::snapshot() const
{
    GeometryInput input;
    const auto access = snapshotAccess();
    input.primaryPresent = positiveSize(access.display().roles[0].displayedImageSize);
    input.primarySize = access.display().roles[0].displayedImageSize;
    input.secondarySize = access.display().roles[1].displayedImageSize;
    return snapshot(input);
}

ImageViewportStateSnapshot ViewportEngine::snapshot(const GeometryInput& input) const
{
    return snapshot({ input, input });
}

ImageViewportStateSnapshot ViewportEngine::snapshot(const SnapshotInput& input) const
{
    return projectViewportStateSnapshot(input, snapshotAccess());
}
