// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILJOBEXECUTOR_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILJOBEXECUTOR_H

#include "session/activenavigationthumbnailwork.h"
#include "thumbnail/thumbnailcachelookup.h"
#include "thumbnail/thumbnailgeneration.h"

#include <functional>
#include <memory>

class QObject;

namespace kiriview {
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
