// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONVIDEODOCUMENTCOMMANDRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONVIDEODOCUMENTCOMMANDRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/documentsessiondocumentports.h"
#include "session/documentsessionvideooutputruntime.h"

#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <memory>

namespace kiriview {
using DocumentSessionVideoOutputClearPort = std::function<void()>;

class DocumentSessionVideoDocumentCommandRuntime final
{
public:
    explicit DocumentSessionVideoDocumentCommandRuntime(
        DocumentSessionVideoDocumentCommandPort commands = {},
        DocumentSessionVideoOutputClearPort clearVideoOutput = {});
    ~DocumentSessionVideoDocumentCommandRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionVideoDocumentCommandRuntime)

    [[nodiscard]] bool setSource(const ResolvedNavigationSource& source);
    [[nodiscard]] bool setSourceDevice(
        const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice);
    [[nodiscard]] bool leaveMode(const QUrl& currentSourceUrl);
    [[nodiscard]] DocumentSessionVideoOutputAttachmentPort outputAttachmentPort() const;

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionVideoDocumentCommandPort m_commands;
    DocumentSessionVideoOutputClearPort m_clearVideoOutput;
    ImageAsyncTicket m_commandAdmission;
};
}

#endif
