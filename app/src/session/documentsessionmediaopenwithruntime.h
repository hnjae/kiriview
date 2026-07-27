// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONMEDIAOPENWITHRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONMEDIAOPENWITHRUNTIME_H

#include "async/imageasyncoperationstate.h"
#include "async/imageiojob.h"
#include "session/mediaopenwith.h"

#include <memory>

class QObject;

namespace kiriview {
class DocumentSessionMediaOpenWithRuntime final
{
public:
    explicit DocumentSessionMediaOpenWithRuntime(MediaOpenWithProvider provider = {});
    ~DocumentSessionMediaOpenWithRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionMediaOpenWithRuntime)

    void open(QObject* receiver, const MediaOpenWithPlan& plan, MediaOpenWithCallback callback);
    void cancel();
    [[nodiscard]] bool active() const;

private:
    std::shared_ptr<void> m_lifetime = std::make_shared<char>();
    MediaOpenWithProvider m_provider;
    ImageIoJob m_job;
    std::shared_ptr<ImageAsyncOperationState> m_operation {
        std::make_shared<ImageAsyncOperationState>()
    };
};
}

#endif
