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
using ActiveNavigationThumbnailWorkRetirementCallback
    = std::function<void(ActiveNavigationThumbnailWorkId)>;

class ActiveNavigationThumbnailJobExecutor final
{
public:
    ActiveNavigationThumbnailJobExecutor(QObject* owner,
        ThumbnailCacheLookupProvider lookupProvider, ThumbnailGenerationProvider generationProvider,
        ActiveNavigationThumbnailWorkCallback completionCallback,
        ActiveNavigationThumbnailWorkRetirementCallback retirementCallback = {});
    ~ActiveNavigationThumbnailJobExecutor();
    Q_DISABLE_COPY_MOVE(ActiveNavigationThumbnailJobExecutor)

    bool start(ActiveNavigationThumbnailWorkRequest request);
    bool cancel(ActiveNavigationThumbnailWorkId workId);
    void cancelAll();

private:
    class State;
    std::shared_ptr<State> m_state;
};
}

#endif
