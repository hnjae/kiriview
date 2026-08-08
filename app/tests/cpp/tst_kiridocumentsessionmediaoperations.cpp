// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

#include "facade/kiridocumentsessioncomposition.h"

class TestKiriDocumentSessionMediaOperations : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void openWithIsUnavailableInEmptySession();
    void openWithUsesCurrentDirectImageUrl();
    void openWithFailureEmitsToastSignal();
    void staleOpenWithFailureAfterReplacementIsIgnored();
    void staleOpenWithFailureAfterSessionDestructionIsIgnored();
    void videoNavigationValidatesAndReusesStillImageWarmCacheWhenReturning();
    void videoActiveNavigationExposesCurrentNumberAndCount();
    void initialDirectImagePredecodeUsesRequestedMediaCursor();
    void directImagePredecodeUsesSessionDependencyOverrides();
    void sessionOwnedSystemMemorySnapshotControlsComposedMemoryConsumers();
    void unknownSessionMemorySnapshotRemainsAcceptedAcrossComposition();
    void composedDependenciesConfigureSharedThumbnailStoreBudgetFromSnapshot();
    void directImagePredecodeDoesNotUseImageDocumentPageCandidates();
    void staleDirectMediaNavigationCandidateCompletionCannotPublishForNewSource();
    void nextMediaFromVideoCanRouteToImageWithoutUsingImageDocumentPageNavigation();
};

#include "kiridocumentsession_media_operations.inc"

QTEST_MAIN(TestKiriDocumentSessionMediaOperations)

#include "tst_kiridocumentsessionmediaoperations.moc"
