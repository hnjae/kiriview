// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILJOBEXECUTOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILJOBEXECUTOR_H

#include "session/activenavigationthumbnailwork.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/thumbnailgeneration.h"

#include <QImage>
#include <QString>
#include <functional>
#include <memory>

class QObject;

namespace kiriview {
struct ActiveNavigationThumbnailWorkRequest
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ThumbnailSourceAdapterPlan sourcePlan;
};

enum class ActiveNavigationThumbnailWorkResultKind {
    Ready,
    Failed,
};

struct ActiveNavigationThumbnailWorkResult
{
    ActiveNavigationThumbnailWorkResultKind kind = ActiveNavigationThumbnailWorkResultKind::Failed;
    QImage image;
    ActiveNavigationThumbnailFailureKind failureKind
        = ActiveNavigationThumbnailFailureKind::GenerationFailed;
    QString errorString;
};

struct ActiveNavigationThumbnailWorkCompletion
{
    ActiveNavigationThumbnailWorkId workId;
    ThumbnailSourceKey sourceKey;
    ActiveNavigationThumbnailDemandBucket bucket = ActiveNavigationThumbnailDemandBucket::None;
    ActiveNavigationThumbnailWorkKind workKind = ActiveNavigationThumbnailWorkKind::Foreground;
    ActiveNavigationThumbnailWorkResult result;
};

using ActiveNavigationThumbnailWorkCallback
    = std::function<void(ActiveNavigationThumbnailWorkCompletion)>;

class ActiveNavigationThumbnailJobExecutor final
{
public:
    ActiveNavigationThumbnailJobExecutor(QObject* owner,
        ThumbnailCacheLookupProvider lookupProvider, ThumbnailGenerationProvider generationProvider,
        ActiveNavigationThumbnailWorkCallback completionCallback);
    ~ActiveNavigationThumbnailJobExecutor();

    ActiveNavigationThumbnailJobExecutor(const ActiveNavigationThumbnailJobExecutor&) = delete;
    ActiveNavigationThumbnailJobExecutor& operator=(const ActiveNavigationThumbnailJobExecutor&)
        = delete;

    bool start(ActiveNavigationThumbnailWorkRequest request);
    bool cancel(ActiveNavigationThumbnailWorkId workId);
    void cancelAll();

private:
    class State;
    std::shared_ptr<State> m_state;
};
}

#endif
