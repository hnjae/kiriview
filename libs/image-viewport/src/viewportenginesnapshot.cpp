// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"

ImageViewportStateSnapshot ViewportEngine::snapshot() const
{
    return projectViewportStateSnapshot(
        { acceptedGeometry(), currentGeometry() }, snapshotAccess());
}
