// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DOCUMENTSESSIONMEDIAPREDECODERUNTIME_H
#define KIRIVIEW_DOCUMENTSESSIONMEDIAPREDECODERUNTIME_H

#include "predecode/mediapredecodedependencies.h"
#include "predecode/predecodedimage.h"
#include "session/directmedianavigationcandidatesnapshot.h"
#include "session/documentsessiontypes.h"

#include <QUrl>
#include <memory>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
class MediaPredecodeCoordinator;

struct DocumentSessionMediaPredecodeInput
{
    bool directMediaNavigationActive = false;
    DocumentSessionKind documentKind = DocumentSessionKind::Empty;
    bool activeImageUsesImageDocumentSourceScope = false;
    bool imageReady = false;
    QUrl currentUrl;
    std::optional<DisplayedPredecodeImage> primaryDisplayedPredecodeImage;
    ImageFirstDisplayDecodeContext firstDisplayDecodeContext;
};

class DocumentSessionMediaPredecodeRuntime final
{
public:
    DocumentSessionMediaPredecodeRuntime(
        QObject* owner, MediaPredecodeDependencyOverrides dependencies = {});
    ~DocumentSessionMediaPredecodeRuntime();

    void schedule(const DocumentSessionMediaPredecodeInput& input,
        DirectMediaNavigationCandidateSnapshot candidateSnapshot);
    void schedule(const DocumentSessionMediaPredecodeInput& input, const QUrl& selectedTargetUrl,
        DirectMediaNavigationCandidateSnapshot candidateSnapshot);
    void cacheDisplayedImages(const DocumentSessionMediaPredecodeInput& input);
    void cancel();
    void clear();
    std::optional<PredecodedImage> findPredecodedImage(const QUrl& url) const;

private:
    static std::vector<DisplayedPredecodeImage> displayedImages(
        const DocumentSessionMediaPredecodeInput& input);

    std::unique_ptr<MediaPredecodeCoordinator> m_coordinator;
};
}

#endif
