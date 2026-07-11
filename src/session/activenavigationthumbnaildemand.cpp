// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnaildemand.h"

namespace kiriview {
ActiveNavigationThumbnailDemandBucket activeNavigationThumbnailDemandBucketForPhysicalMaxEdge(
    int physicalMaxEdge)
{
    if (physicalMaxEdge <= 0) {
        return ActiveNavigationThumbnailDemandBucket::None;
    }
    if (physicalMaxEdge <= 128) {
        return ActiveNavigationThumbnailDemandBucket::Normal;
    }
    if (physicalMaxEdge <= 256) {
        return ActiveNavigationThumbnailDemandBucket::Large;
    }
    if (physicalMaxEdge <= 512) {
        return ActiveNavigationThumbnailDemandBucket::XLarge;
    }

    return ActiveNavigationThumbnailDemandBucket::XXLarge;
}

}
