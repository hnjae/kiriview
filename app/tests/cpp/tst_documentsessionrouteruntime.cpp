// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionrouteruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>

#include <memory>
#include <vector>

class TestDocumentSessionRouteRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void routeSourceUrlPlansAndExecutesFromCurrentKind();
    void routeMediaUrlPlansAndExecutesFromCurrentKind();
    void directImageMediaRouteExecutesSameScopeImageNavigationEntry();
    void executionRunsMutationPublicationFollowUpAndCompletionInOrder();
    void executionPublishesBeforeTypedFollowUps();
    void clearedNavigationRepublishesBeforePredecodeScopeSync();
    void directMediaScopeChangeSyncsPredecodeScopeAfterFinalCursorMutation();
    void activeNavigationRefreshesWithoutScopeChange();
    void externalAuthorityLossStopsBeforeCommitAndFollowUps();
    void delegatedNavigationRefreshMayConsumeOriginatingAuthority();
    void staleReentrantExecutionDoesNotSupersedeCurrentRoute();
    void authorityPreflightReentryKeepsNewerRoute();
    void reentrantExecutionSupersedesRemainingRoute();
    void sourceResolutionReentrySupersedesOlderRoute();
    void callbackDestructionStopsRemainingRoute();
};

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::ResolvedNavigationSource resolveIdentitySource(const QUrl& url)
{
    return kiriview::ResolvedNavigationSource(url, kiriview::NavigationSourceEntryFacts {}, url);
}

bool executeRoute(kiriview::DocumentSessionRouteRuntime& runtime,
    const kiriview::DocumentSessionRoutePlan& plan,
    const kiriview::DocumentSessionRouteExecutionControl& control = {})
{
    return runtime.executeWithSourceResolver(plan, resolveIdentitySource, control);
}
}

void TestDocumentSessionRouteRuntime::executionRunsMutationPublicationFollowUpAndCompletionInOrder()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.session.clearSessionErrorString
        = [&events]() { events.push_back(QStringLiteral("clear-error")); };
    ports.directMedia.clearDirectMediaCursor = [&events]() {
        events.push_back(QStringLiteral("clear-cursor"));
        return true;
    };
    ports.session.executeWithRoutingSuppressed = [&events](const std::function<void()>& mutation) {
        events.push_back(QStringLiteral("suppress-begin"));
        mutation();
        events.push_back(QStringLiteral("suppress-end"));
    };
    ports.documents.enterImageDocument = [&events](
                                             const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("enter-image:%1").arg(source.requestedUrl().toString()));
    };
    ports.sourceIdentity.useOriginalSourceIdentity = [&events](const QUrl& url) {
        events.push_back(QStringLiteral("identity:%1").arg(url.toString()));
    };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.directMedia.directMediaNavigationActive = []() { return false; };
    ports.directMedia.refreshDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("refresh-navigation")); };
    ports.followUp.syncMediaPredecodeScope
        = [&events]() { events.push_back(QStringLiteral("sync-predecode-scope")); };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl imageUrl = localUrl(QStringLiteral("/tmp/page.png"));
    kiriview::DocumentSessionRoutePlan plan;
    plan.kind = kiriview::DocumentSessionRouteKind::DirectImage;
    plan.sourceUrl = imageUrl;
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearSessionErrorStringRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearDirectMediaCursorRouteOperation {} },
        kiriview::DocumentSessionRouteMutation { kiriview::EnterImageDocumentRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    plan.publishPublicProjection = true;
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::RefreshDirectMediaNavigationAfterRoutingRouteEffect {} },
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::ClearMediaPredecodeRouteEffect {} },
    };

    QVERIFY(executeRoute(runtime, plan));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("clear-error"),
        QStringLiteral("clear-cursor"),
        QStringLiteral("suppress-begin"),
        QStringLiteral("enter-image:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-end"),
        QStringLiteral("identity:%1").arg(imageUrl.toString()),
        QStringLiteral("publish"),
        QStringLiteral("sync-predecode-scope"),
        QStringLiteral("refresh-navigation"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::routeSourceUrlPlansAndExecutesFromCurrentKind()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.session.clearSessionErrorString
        = [&events]() { events.push_back(QStringLiteral("clear-error")); };
    ports.directMedia.cancelDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("cancel-navigation")); };
    ports.directMedia.cancelMediaDeletion
        = [&events]() { events.push_back(QStringLiteral("cancel-deletion")); };
    ports.directMedia.clearDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("clear-navigation")); };
    ports.directMedia.clearDirectMediaCursor = [&events]() {
        events.push_back(QStringLiteral("clear-cursor"));
        return true;
    };
    ports.directMedia.requestDirectImageCursor
        = [&events](const kiriview::ResolvedNavigationSource& source) {
              events.push_back(
                  QStringLiteral("request-image-cursor:%1").arg(source.requestedUrl().toString()));
              return true;
          };
    ports.session.executeWithRoutingSuppressed = [&events](const std::function<void()>& mutation) {
        events.push_back(QStringLiteral("suppress-begin"));
        mutation();
        events.push_back(QStringLiteral("suppress-end"));
    };
    ports.documents.leaveVideoMode
        = [&events]() { events.push_back(QStringLiteral("leave-video")); };
    ports.documents.enterImageDocument = [&events](
                                             const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("enter-image:%1").arg(source.requestedUrl().toString()));
    };
    ports.directMedia.syncDirectImageCursorFromDocument = [&events]() {
        events.push_back(QStringLiteral("sync-image-cursor"));
        return true;
    };
    ports.sourceIdentity.useImageDocumentSourceIdentity
        = [&events]() { events.push_back(QStringLiteral("image-document-identity")); };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.directMedia.directMediaNavigationActive = []() { return false; };
    ports.directMedia.refreshDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("refresh-navigation")); };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl imageUrl = localUrl(QStringLiteral("/tmp/page.png"));

    QVERIFY(executeRoute(runtime,
        kiriview::documentSessionRoutePlanForSourceUrl(
            imageUrl, kiriview::DocumentSessionKind::Empty)));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("clear-error"),
        QStringLiteral("cancel-navigation"),
        QStringLiteral("cancel-deletion"),
        QStringLiteral("clear-navigation"),
        QStringLiteral("clear-cursor"),
        QStringLiteral("request-image-cursor:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-begin"),
        QStringLiteral("leave-video"),
        QStringLiteral("suppress-end"),
        QStringLiteral("suppress-begin"),
        QStringLiteral("enter-image:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-end"),
        QStringLiteral("sync-image-cursor"),
        QStringLiteral("image-document-identity"),
        QStringLiteral("publish"),
        QStringLiteral("refresh-navigation"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::routeMediaUrlPlansAndExecutesFromCurrentKind()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.directMedia.cancelMediaDeletion
        = [&events]() { events.push_back(QStringLiteral("cancel-deletion")); };
    ports.directMedia.setDirectVideoCursor = [&events](
                                                 const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("video-cursor:%1").arg(source.requestedUrl().toString()));
        return true;
    };
    ports.session.executeWithRoutingSuppressed = [&events](const std::function<void()>& mutation) {
        events.push_back(QStringLiteral("suppress-begin"));
        mutation();
        events.push_back(QStringLiteral("suppress-end"));
    };
    ports.documents.enterVideoDocument = [&events](
                                             const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("enter-video:%1").arg(source.requestedUrl().toString()));
    };
    ports.documents.clearImageDocument
        = [&events]() { events.push_back(QStringLiteral("clear-image")); };
    ports.sourceIdentity.useOriginalSourceIdentity = [&events](const QUrl& url) {
        events.push_back(QStringLiteral("identity:%1").arg(url.toString()));
    };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.directMedia.directMediaNavigationActive = []() { return false; };
    ports.directMedia.refreshDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("refresh-navigation")); };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl videoUrl = localUrl(QStringLiteral("/tmp/movie.mp4"));

    QVERIFY(executeRoute(runtime,
        kiriview::documentSessionRoutePlanForMediaUrl(
            videoUrl, kiriview::DocumentSessionKind::Image)));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("cancel-deletion"),
        QStringLiteral("video-cursor:%1").arg(videoUrl.toString()),
        QStringLiteral("suppress-begin"),
        QStringLiteral("clear-image"),
        QStringLiteral("suppress-end"),
        QStringLiteral("suppress-begin"),
        QStringLiteral("enter-video:%1").arg(videoUrl.toString()),
        QStringLiteral("suppress-end"),
        QStringLiteral("identity:%1").arg(videoUrl.toString()),
        QStringLiteral("publish"),
        QStringLiteral("refresh-navigation"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::directImageMediaRouteExecutesSameScopeImageNavigationEntry()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.directMedia.cancelMediaDeletion
        = [&events]() { events.push_back(QStringLiteral("cancel-deletion")); };
    ports.directMedia.requestDirectImageCursor
        = [&events](const kiriview::ResolvedNavigationSource& source) {
              events.push_back(
                  QStringLiteral("request-image-cursor:%1").arg(source.requestedUrl().toString()));
              return false;
          };
    ports.session.executeWithRoutingSuppressed = [&events](const std::function<void()>& mutation) {
        events.push_back(QStringLiteral("suppress-begin"));
        mutation();
        events.push_back(QStringLiteral("suppress-end"));
    };
    ports.documents.leaveVideoMode
        = [&events]() { events.push_back(QStringLiteral("leave-video")); };
    ports.documents.enterImageDocument = [&events](
                                             const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("enter-image:%1").arg(source.requestedUrl().toString()));
    };
    ports.documents.enterImageDocumentSameScopeNavigation =
        [&events](const kiriview::ResolvedNavigationSource& source) {
            events.push_back(
                QStringLiteral("enter-same-scope-image:%1").arg(source.requestedUrl().toString()));
        };
    ports.directMedia.syncDirectImageCursorFromDocument = [&events]() {
        events.push_back(QStringLiteral("sync-image-cursor"));
        return false;
    };
    ports.sourceIdentity.useImageDocumentSourceIdentity
        = [&events]() { events.push_back(QStringLiteral("image-document-identity")); };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.directMedia.directMediaNavigationActive = []() { return false; };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl imageUrl = localUrl(QStringLiteral("/tmp/page.png"));

    QVERIFY(executeRoute(runtime,
        kiriview::documentSessionRoutePlanForMediaUrl(
            imageUrl, kiriview::DocumentSessionKind::Image)));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("cancel-deletion"),
        QStringLiteral("request-image-cursor:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-begin"),
        QStringLiteral("leave-video"),
        QStringLiteral("suppress-end"),
        QStringLiteral("suppress-begin"),
        QStringLiteral("enter-same-scope-image:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-end"),
        QStringLiteral("sync-image-cursor"),
        QStringLiteral("image-document-identity"),
        QStringLiteral("publish"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::executionPublishesBeforeTypedFollowUps()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.session.clearSessionErrorString
        = [&events]() { events.push_back(QStringLiteral("clear-error")); };
    ports.directMedia.clearDirectMediaCursor = [&events]() {
        events.push_back(QStringLiteral("clear-cursor"));
        return true;
    };
    ports.session.executeWithRoutingSuppressed = [&events](const std::function<void()>& mutation) {
        events.push_back(QStringLiteral("suppress-begin"));
        mutation();
        events.push_back(QStringLiteral("suppress-end"));
    };
    ports.documents.enterImageDocument = [&events](
                                             const kiriview::ResolvedNavigationSource& source) {
        events.push_back(QStringLiteral("enter-image:%1").arg(source.requestedUrl().toString()));
    };
    ports.sourceIdentity.useOriginalSourceIdentity = [&events](const QUrl& url) {
        events.push_back(QStringLiteral("identity:%1").arg(url.toString()));
    };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.directMedia.directMediaNavigationActive = []() { return false; };
    ports.directMedia.refreshDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("refresh-navigation")); };
    ports.followUp.syncMediaPredecodeScope
        = [&events]() { events.push_back(QStringLiteral("sync-predecode-scope")); };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl imageUrl = localUrl(QStringLiteral("/tmp/page.png"));
    kiriview::DocumentSessionRoutePlan plan;
    plan.kind = kiriview::DocumentSessionRouteKind::DirectImage;
    plan.sourceUrl = imageUrl;
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearSessionErrorStringRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearDirectMediaCursorRouteOperation {} },
        kiriview::DocumentSessionRouteMutation { kiriview::EnterImageDocumentRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    plan.publishPublicProjection = true;
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::RefreshDirectMediaNavigationAfterRoutingRouteEffect {} },
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::ClearMediaPredecodeRouteEffect {} },
    };

    QVERIFY(executeRoute(runtime, plan));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("clear-error"),
        QStringLiteral("clear-cursor"),
        QStringLiteral("suppress-begin"),
        QStringLiteral("enter-image:%1").arg(imageUrl.toString()),
        QStringLiteral("suppress-end"),
        QStringLiteral("identity:%1").arg(imageUrl.toString()),
        QStringLiteral("publish"),
        QStringLiteral("sync-predecode-scope"),
        QStringLiteral("refresh-navigation"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::clearedNavigationRepublishesBeforePredecodeScopeSync()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.clearDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("clear-navigation")); };
    ports.followUp.recomputePublicProjection
        = [&events]() { events.push_back(QStringLiteral("publish")); };
    ports.followUp.syncMediaPredecodeScope
        = [&events]() { events.push_back(QStringLiteral("sync-predecode-scope")); };
    ports.session.routeCompleted = []() { };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    kiriview::DocumentSessionRoutePlan plan;
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearDirectMediaNavigationRouteOperation {} },
    };
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::ClearMediaPredecodeRouteEffect {} },
    };

    QVERIFY(executeRoute(runtime, plan));

    const std::vector<QString> expected {
        QStringLiteral("clear-navigation"),
        QStringLiteral("clear-navigation"),
        QStringLiteral("publish"),
        QStringLiteral("sync-predecode-scope"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::
    directMediaScopeChangeSyncsPredecodeScopeAfterFinalCursorMutation()
{
    std::vector<QString> events;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith
        = [&events]() { events.push_back(QStringLiteral("cancel-open-with")); };
    ports.directMedia.clearDirectMediaCursor = [&events]() {
        events.push_back(QStringLiteral("clear-cursor"));
        return true;
    };
    ports.directMedia.requestDirectImageCursor
        = [&events](const kiriview::ResolvedNavigationSource& source) {
              events.push_back(
                  QStringLiteral("request-image-cursor:%1").arg(source.requestedUrl().toString()));
              return true;
          };
    ports.followUp.syncMediaPredecodeScope
        = [&events]() { events.push_back(QStringLiteral("sync-predecode-scope")); };
    ports.directMedia.refreshDirectMediaNavigation
        = [&events]() { events.push_back(QStringLiteral("refresh-navigation")); };
    ports.session.routeCompleted = [&events]() { events.push_back(QStringLiteral("complete")); };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    const QUrl imageUrl = localUrl(QStringLiteral("/tmp/page.png"));
    kiriview::DocumentSessionRoutePlan plan;
    plan.sourceUrl = imageUrl;
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearThenRequestDirectImageCursorRouteOperation {} },
    };
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::RefreshDirectMediaNavigationAfterRoutingRouteEffect {} },
    };

    QVERIFY(executeRoute(runtime, plan));

    const std::vector<QString> expected {
        QStringLiteral("cancel-open-with"),
        QStringLiteral("clear-cursor"),
        QStringLiteral("request-image-cursor:%1").arg(imageUrl.toString()),
        QStringLiteral("sync-predecode-scope"),
        QStringLiteral("refresh-navigation"),
        QStringLiteral("complete"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionRouteRuntime::activeNavigationRefreshesWithoutScopeChange()
{
    int refreshCount = 0;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.directMediaNavigationActive = []() { return true; };
    ports.directMedia.refreshDirectMediaNavigation = [&refreshCount]() { ++refreshCount; };
    ports.session.routeCompleted = []() { };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    kiriview::DocumentSessionRoutePlan plan;
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::RefreshDirectMediaNavigationAfterRoutingRouteEffect {} },
    };

    QVERIFY(executeRoute(runtime, plan));

    QCOMPARE(refreshCount, 1);
}

void TestDocumentSessionRouteRuntime::externalAuthorityLossStopsBeforeCommitAndFollowUps()
{
    bool current = true;
    int staleMutationCount = 0;
    int commitCount = 0;
    int publicationCount = 0;
    int followUpCount = 0;
    int completionCount = 0;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.clearDirectMediaCursor = [&]() {
        current = false;
        return true;
    };
    ports.directMedia.requestDirectImageCursor = [&](const kiriview::ResolvedNavigationSource&) {
        ++staleMutationCount;
        return true;
    };
    ports.followUp.recomputePublicProjection = [&]() { ++publicationCount; };
    ports.followUp.syncMediaPredecodeScope = [&]() { ++followUpCount; };
    ports.session.routeCompleted = [&]() { ++completionCount; };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    kiriview::DocumentSessionRoutePlan plan;
    plan.sourceUrl = localUrl(QStringLiteral("/media/page.png"));
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearThenRequestDirectImageCursorRouteOperation {} },
    };
    plan.publishPublicProjection = true;
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::ClearMediaPredecodeRouteEffect {} },
    };
    const kiriview::DocumentSessionRouteExecutionControl control {
        [&]() { return current; },
        [&]() { ++commitCount; },
    };

    QVERIFY(!executeRoute(runtime, plan, control));
    QCOMPARE(staleMutationCount, 0);
    QCOMPARE(commitCount, 0);
    QCOMPARE(publicationCount, 0);
    QCOMPARE(followUpCount, 0);
    QCOMPARE(completionCount, 0);
}

void TestDocumentSessionRouteRuntime::delegatedNavigationRefreshMayConsumeOriginatingAuthority()
{
    bool originatingCurrent = true;
    int refreshCount = 0;
    int completionCount = 0;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.directMediaNavigationActive = []() { return true; };
    ports.directMedia.refreshDirectMediaNavigation = [&]() {
        ++refreshCount;
        originatingCurrent = false;
    };
    ports.session.routeCompleted = [&]() { ++completionCount; };

    kiriview::DocumentSessionRouteRuntime runtime(std::move(ports));
    kiriview::DocumentSessionRoutePlan plan;
    plan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::RefreshDirectMediaNavigationAfterRoutingRouteEffect {} },
    };
    const kiriview::DocumentSessionRouteExecutionControl control {
        [&]() { return originatingCurrent; },
        {},
    };

    QVERIFY(executeRoute(runtime, plan, control));
    QCOMPARE(refreshCount, 1);
    QCOMPARE(completionCount, 1);
}

void TestDocumentSessionRouteRuntime::staleReentrantExecutionDoesNotSupersedeCurrentRoute()
{
    const QUrl currentUrl = localUrl(QStringLiteral("/media/current.png"));
    const QUrl staleUrl = localUrl(QStringLiteral("/media/stale.png"));
    bool staleCompleted = true;
    int staleResolverCount = 0;
    int publicationCount = 0;
    int followUpCount = 0;
    int completionCount = 0;
    QUrl sourceIdentity;
    std::unique_ptr<kiriview::DocumentSessionRouteRuntime> runtime;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.clearDirectMediaCursor = [&]() {
        kiriview::DocumentSessionRoutePlan stalePlan;
        stalePlan.sourceUrl = staleUrl;
        stalePlan.mutations = {
            kiriview::DocumentSessionRouteMutation {
                kiriview::SetDirectVideoCursorRouteOperation {} },
        };
        const kiriview::DocumentSessionRouteExecutionControl staleControl {
            []() { return false; },
            {},
        };
        staleCompleted = runtime->executeWithSourceResolver(
            stalePlan,
            [&](const QUrl& url) {
                ++staleResolverCount;
                return resolveIdentitySource(url);
            },
            staleControl);
        return true;
    };
    ports.sourceIdentity.useOriginalSourceIdentity = [&](const QUrl& url) { sourceIdentity = url; };
    ports.followUp.recomputePublicProjection = [&]() { ++publicationCount; };
    ports.followUp.syncMediaPredecodeScope = [&]() { ++followUpCount; };
    ports.session.routeCompleted = [&]() { ++completionCount; };
    runtime = std::make_unique<kiriview::DocumentSessionRouteRuntime>(std::move(ports));

    kiriview::DocumentSessionRoutePlan currentPlan;
    currentPlan.sourceUrl = currentUrl;
    currentPlan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearDirectMediaCursorRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    currentPlan.publishPublicProjection = true;
    currentPlan.followUpEffects = {
        kiriview::DocumentSessionRouteFollowUpEffect {
            kiriview::ClearMediaPredecodeRouteEffect {} },
    };

    QVERIFY(executeRoute(*runtime, currentPlan));
    QVERIFY(!staleCompleted);
    QCOMPARE(staleResolverCount, 0);
    QCOMPARE(sourceIdentity, currentUrl);
    QCOMPARE(publicationCount, 1);
    QCOMPARE(followUpCount, 1);
    QCOMPARE(completionCount, 1);
}

void TestDocumentSessionRouteRuntime::authorityPreflightReentryKeepsNewerRoute()
{
    const QUrl olderUrl = localUrl(QStringLiteral("/media/older.png"));
    const QUrl newerUrl = localUrl(QStringLiteral("/media/newer.png"));
    bool newerSubmitted = false;
    bool newerCompleted = false;
    int olderResolverCount = 0;
    std::vector<QUrl> appliedSources;
    std::unique_ptr<kiriview::DocumentSessionRouteRuntime> runtime;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.sourceIdentity.useOriginalSourceIdentity
        = [&](const QUrl& url) { appliedSources.push_back(url); };
    ports.session.routeCompleted = []() { };
    runtime = std::make_unique<kiriview::DocumentSessionRouteRuntime>(std::move(ports));

    kiriview::DocumentSessionRoutePlan newerPlan;
    newerPlan.sourceUrl = newerUrl;
    newerPlan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    kiriview::DocumentSessionRoutePlan olderPlan;
    olderPlan.sourceUrl = olderUrl;
    olderPlan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    const kiriview::DocumentSessionRouteExecutionControl olderControl {
        [&]() {
            if (!newerSubmitted) {
                newerSubmitted = true;
                newerCompleted = executeRoute(*runtime, newerPlan);
            }
            return true;
        },
        {},
    };

    const bool olderCompleted = runtime->executeWithSourceResolver(
        olderPlan,
        [&](const QUrl& url) {
            ++olderResolverCount;
            return resolveIdentitySource(url);
        },
        olderControl);

    QVERIFY(newerSubmitted);
    QVERIFY(newerCompleted);
    QVERIFY(!olderCompleted);
    QCOMPARE(olderResolverCount, 0);
    QCOMPARE(appliedSources, std::vector<QUrl> { newerUrl });
}

void TestDocumentSessionRouteRuntime::reentrantExecutionSupersedesRemainingRoute()
{
    const QUrl staleUrl = localUrl(QStringLiteral("/media/stale.png"));
    const QUrl latestUrl = localUrl(QStringLiteral("/media/latest.png"));
    int staleRequestCount = 0;
    int stalePublicationCount = 0;
    QUrl sourceIdentity;
    bool nestedRouteSubmitted = false;
    std::unique_ptr<kiriview::DocumentSessionRouteRuntime> runtime;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.clearDirectMediaCursor = [&]() {
        if (!nestedRouteSubmitted) {
            nestedRouteSubmitted = true;
            kiriview::DocumentSessionRoutePlan latestPlan;
            latestPlan.sourceUrl = latestUrl;
            latestPlan.mutations = {
                kiriview::DocumentSessionRouteMutation {
                    kiriview::UseOriginalSourceIdentityRouteOperation {} },
            };
            static_cast<void>(executeRoute(*runtime, latestPlan));
        }
        return true;
    };
    ports.directMedia.requestDirectImageCursor = [&](const kiriview::ResolvedNavigationSource&) {
        ++staleRequestCount;
        return true;
    };
    ports.sourceIdentity.useOriginalSourceIdentity = [&](const QUrl& url) { sourceIdentity = url; };
    ports.followUp.recomputePublicProjection = [&]() { ++stalePublicationCount; };
    ports.session.routeCompleted = []() { };
    runtime = std::make_unique<kiriview::DocumentSessionRouteRuntime>(std::move(ports));

    kiriview::DocumentSessionRoutePlan stalePlan;
    stalePlan.sourceUrl = staleUrl;
    stalePlan.mutations = {
        kiriview::DocumentSessionRouteMutation {
            kiriview::ClearThenRequestDirectImageCursorRouteOperation {} },
        kiriview::DocumentSessionRouteMutation {
            kiriview::UseOriginalSourceIdentityRouteOperation {} },
    };
    stalePlan.publishPublicProjection = true;

    static_cast<void>(executeRoute(*runtime, stalePlan));

    QVERIFY(nestedRouteSubmitted);
    QCOMPARE(staleRequestCount, 0);
    QCOMPARE(stalePublicationCount, 0);
    QCOMPARE(sourceIdentity, latestUrl);
}

void TestDocumentSessionRouteRuntime::sourceResolutionReentrySupersedesOlderRoute()
{
    const QUrl staleUrl = localUrl(QStringLiteral("/media/stale.mp4"));
    const QUrl latestUrl = localUrl(QStringLiteral("/media/latest.mp4"));
    std::vector<QUrl> appliedSources;
    std::unique_ptr<kiriview::DocumentSessionRouteRuntime> runtime;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.session.cancelMediaOpenWith = []() { };
    ports.directMedia.setDirectVideoCursor = [&](const kiriview::ResolvedNavigationSource& source) {
        appliedSources.push_back(source.requestedUrl());
        return true;
    };
    ports.session.routeCompleted = []() { };
    runtime = std::make_unique<kiriview::DocumentSessionRouteRuntime>(std::move(ports));

    kiriview::DocumentSessionRoutePlan latestPlan;
    latestPlan.sourceUrl = latestUrl;
    latestPlan.mutations = {
        kiriview::DocumentSessionRouteMutation { kiriview::SetDirectVideoCursorRouteOperation {} },
    };
    kiriview::DocumentSessionRoutePlan stalePlan;
    stalePlan.sourceUrl = staleUrl;
    stalePlan.mutations = {
        kiriview::DocumentSessionRouteMutation { kiriview::SetDirectVideoCursorRouteOperation {} },
    };

    const bool staleCompleted = runtime->executeWithSourceResolver(stalePlan, [&](const QUrl& url) {
        static_cast<void>(runtime->executeWithSourceResolver(latestPlan, [](const QUrl& latest) {
            return kiriview::ResolvedNavigationSource(
                latest, kiriview::NavigationSourceEntryFacts {}, latest);
        }));
        return kiriview::ResolvedNavigationSource(
            url, kiriview::NavigationSourceEntryFacts {}, url);
    });

    QVERIFY(!staleCompleted);
    QCOMPARE(appliedSources, std::vector<QUrl> { latestUrl });
}

void TestDocumentSessionRouteRuntime::callbackDestructionStopsRemainingRoute()
{
    int enterEmptyCount = 0;
    int publicationCount = 0;
    int completionCount = 0;
    std::unique_ptr<kiriview::DocumentSessionRouteRuntime> runtime;
    kiriview::DocumentSessionRouteRuntimePorts ports;
    ports.documents.leaveVideoMode = [&]() { runtime.reset(); };
    ports.documents.enterEmptyDocument = [&]() { ++enterEmptyCount; };
    ports.followUp.recomputePublicProjection = [&]() { ++publicationCount; };
    ports.session.routeCompleted = [&]() { ++completionCount; };
    runtime = std::make_unique<kiriview::DocumentSessionRouteRuntime>(std::move(ports));

    kiriview::DocumentSessionRoutePlan plan;
    plan.mutations = {
        kiriview::DocumentSessionRouteMutation { kiriview::LeaveVideoModeRouteOperation {} },
        kiriview::DocumentSessionRouteMutation { kiriview::EnterEmptyDocumentRouteOperation {} },
    };
    plan.publishPublicProjection = true;

    kiriview::DocumentSessionRouteRuntime* executingRuntime = runtime.get();
    const bool completed = executeRoute(*executingRuntime, plan);

    QVERIFY(!completed);
    QVERIFY(runtime == nullptr);
    QCOMPARE(enterEmptyCount, 0);
    QCOMPARE(publicationCount, 0);
    QCOMPARE(completionCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionRouteRuntime)

#include "tst_documentsessionrouteruntime.moc"
