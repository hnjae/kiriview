// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "activenavigationthumbnaildemand.h"

#include <QString>

namespace {
bool sameDemandState(const kiriview::ActiveNavigationThumbnailDemand& left,
    const kiriview::ActiveNavigationThumbnailDemand& right)
{
    return left.number == right.number && left.url == right.url && left.bucket == right.bucket
        && left.priority == right.priority
        && left.navigationGeneration == right.navigationGeneration;
}

QString demandIdentity(const kiriview::ActiveNavigationThumbnailDemand& demand)
{
    return QStringLiteral("%1\x1f%2\x1f%3")
        .arg(demand.number)
        .arg(demand.url.toString(QUrl::FullyEncoded))
        .arg(demand.navigationGeneration);
}
}

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

bool ActiveNavigationThumbnailDemandTracker::record(const ActiveNavigationThumbnailDemand& demand)
{
    if (demand.number <= 0 || demand.url.isEmpty()
        || demand.bucket == ActiveNavigationThumbnailDemandBucket::None
        || demand.navigationGeneration == 0) {
        return false;
    }

    const QString identity = demandIdentity(demand);
    auto acceptedDemand = m_acceptedDemandsByIdentity.constFind(identity);
    if (acceptedDemand != m_acceptedDemandsByIdentity.cend()) {
        if (sameDemandState(acceptedDemand.value(), demand)) {
            return false;
        }

        m_acceptedDemandsByIdentity.insert(identity, demand);
        return true;
    }

    m_acceptedDemandsByIdentity.insert(identity, demand);
    return true;
}

void ActiveNavigationThumbnailDemandTracker::reset() { m_acceptedDemandsByIdentity.clear(); }
}
