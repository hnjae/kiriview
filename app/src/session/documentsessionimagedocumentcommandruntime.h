// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONIMAGEDOCUMENTCOMMANDRUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONIMAGEDOCUMENTCOMMANDRUNTIME_H

#include "async/imageasyncticket.h"
#include "session/documentsessiondocumentports.h"

#include <QUrl>
#include <QtGlobal>
#include <memory>
#include <optional>

namespace kiriview {
class DocumentSessionImageDocumentCommandRuntime final
{
public:
    explicit DocumentSessionImageDocumentCommandRuntime(
        DocumentSessionImageDocumentCommandPort commands = {});
    ~DocumentSessionImageDocumentCommandRuntime();
    Q_DISABLE_COPY_MOVE(DocumentSessionImageDocumentCommandRuntime)

    [[nodiscard]] bool setSource(const ResolvedNavigationSource& source);
    [[nodiscard]] bool clearSourceUrl();
    [[nodiscard]] std::optional<MediaEntrySourceVideoPlaybackDeviceResult>
    loadOpenedCollectionVideoPlaybackDevice(
        const OpenedCollectionScopeLocation& openedCollectionScope, const QUrl& videoUrl);
    void openPreviousPage();
    void openNextPage();
    void openImageAtPage(int number);
    void deleteDisplayedFile(FileDeletionMode mode);

private:
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    DocumentSessionImageDocumentCommandPort m_commands;
    ImageAsyncTicket m_sourceCommandAdmission;
};
}

#endif
