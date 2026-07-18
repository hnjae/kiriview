// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "facade/kiriimagedocument.h"
#include "facade/kirimediainformation.h"
#include "facade/kirivideodocument.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaProperty>
#include <QRegularExpression>
#include <QStringList>
#include <QTest>

class TestArchitectureBoundaries : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void leafDocumentSourceRoutesAreReadOnlyPublicObservations();
    void leafDocumentRouteSettersStayPrivateToTheSession();
    void qmlCannotSetLeafDocumentRoutes();
    void qmlCannotInvokeLeafImageNavigationOrDeletion();
    void qmlDoesNotWriteSharedActionState();
    void qmlActionProxiesDoNotOverrideRuntimeActionState();
    void qmlDoesNotOwnSharedActionPolicy();
    void qmlDoesNotComputePannabilityActionGate();
    void qmlDoesNotRecomputeSharedMediaReadiness();
    void qmlDoesNotWriteDurablePresentationState();
    void imageDocumentHasNoPublicPresentationBackdoorSetters();
    void qmlViewportReportsPositionsThroughCommandBridge();
    void qmlViewportCommandHandlersDoNotApplyProjectionDirectly();
    void qmlViewportUsesOpaqueRevisionTokens();
    void leafDocumentsAreNotProductionQmlCreatable();
    void actionUiGatesAreRevisionedSnapshots();
    void qmlDoesNotManufactureStaleSensitiveRevisions();
    void imageActionAvailabilityFacadeIsNotWritableQmlBackdoor();
    void fixedViewerShortcutsDoNotBypassRuntimeRouting();
    void qmlDoesNotExposeFixedViewerScanCommandRoutes();
    void videoSeekShortcutsRouteThroughApplicationRuntime();
    void qmlDoesNotOwnVideoPlaybackControlState();
    void qmlDoesNotOwnFullscreenChromeLifecycle();
    void applicationFacadeDoesNotOwnFixedViewerCommandRouting();
    void applicationFacadeDoesNotOwnActionStateSourceAttachment();
    void applicationCommandRouterPortsAreGroupedByOwner();
    void applicationFacadeDoesNotOwnActionCommandSwitch();
    void sessionPublicProjectionHasNoPartialUpdateBackdoor();
    void sessionPublicProjectionDoesNotSampleLeafFacadesWhileApplying();
    void qmlDoesNotWriteSharedVideoOutputAttachment();
    void videoOutputAttachmentIsNotWritablePublicVideoDocumentState();
    void qmlDoesNotDeriveSharedControlPolicyFromLeafDocuments();
    void qmlUsesCentralNavigationPresentationOrder();
    void qmlViewportUsesContextBridgeForRenderContextDiscovery();
    void providerRenderingRejectsTileSourceContracts();
    void productionImageDisplayUsesProviderPathOnly();
    void openedCollectionThumbnailEligibilityUsesSharedPolicy();
    void decodingUsesNeutralThumbnailContracts();
    void cppNamespaceIsLowercaseKiriview();
    void documentSessionUsesThumbnailStripDependencyPort();
    void documentSessionUsesOpenWithRuntime();
    void documentSessionOpenWithUsesNamedPlanPort();
    void activeNavigationThumbnailRuntimeUsesCanonicalThumbnailSourceKey();
    void liveDirectoryWatchUsesProviderBoundary();
    void mediaFormatRegistryDoesNotOwnLocalizedDialogLabels();
    void asyncImageIoJobsDoNotOwnDecodeDataLoading();
    void asyncImageIoJobsDoNotOwnDirectoryCandidateLoading();
    void asyncImageIoJobsDoNotOwnOpenedCollectionCandidateLoading();
    void mediaEntrySourceStoreDoesNotDependOnDocumentPlanning();
    void documentSessionDirectMediaScopeUsesNamedPort();
    void documentSessionDirectMediaActivityUsesNamedPort();
    void documentSessionMediaPredecodeInputUsesNamedPort();
    void documentSessionDirectMediaNavigationInputUsesNamedPort();
    void documentSessionPublicSnapshotInputUsesNamedPort();
    void documentSessionRouteRuntimePortsAreGrouped();
    void documentSessionDirectMediaNavigationUsesCoordinator();
    void documentSessionImageDocumentSyncUsesRuntime();
    void documentSessionVideoDocumentSyncUsesRuntime();
    void imageDocumentSourceLoadPlanDispatchHasNamedExecutor();
    void imageDocumentOpenPlanDispatchHasNamedExecutor();
    void imageDocumentPredecodePlanDispatchHasNamedExecutor();
    void imageDocumentNavigationPlanDispatchHasNamedExecutor();
    void imageDocumentLifecyclePlanDispatchHasNamedExecutor();
    void imageDocumentMediaEntrySourcePlanDispatchHasNamedExecutor();
    void imageDocumentSpreadPlanDispatchHasNamedExecutor();
    void imageDocumentPredecodedImageLookupUsesNamedPort();
    void imageDocumentPrimaryPageSlotUsesNamedPort();
    void imageDocumentNavigationSnapshotUsesNamedPort();
    void imageDocumentPageCandidateSnapshotUsesNamedPort();
    void imageDocumentAdjacentPredecodeSchedulingUsesNamedPort();
    void imageDocumentDeletionProgressUsesNamedPort();
    void imageDocumentCurrentPageNumberUsesNamedPort();
    void imageDocumentAnimationLoadErrorUsesNamedPort();
    void imagePageSurfaceOwnersExposeNoPresentationState();
    void imagePresentationPageSlotsUseDisplaySourceVariants();
    void activePresentationDoesNotWritePageSurfacePresentationState();
    void productionFacadesDoNotExposePresentationBackdoorSetters();
    void mediaInformationFacadeExposesSnapshotRevision();
};

namespace {
QString projectPath(const QString& relativePath)
{
    return QDir(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../..")).filePath(relativePath);
}

QString readProjectFile(const QString& relativePath)
{
    QFile file(projectPath(relativePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Cannot open %s", qPrintable(relativePath));
    }
    return QString::fromUtf8(file.readAll());
}

QStringList productionQmlFiles()
{
    QStringList files;
    QDirIterator iterator(projectPath(QStringLiteral("src/qml")),
        QStringList { QStringLiteral("*.qml") }, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        files.push_back(iterator.next());
    }
    files.sort();
    return files;
}

QStringList projectFilesUnder(const QStringList& relativeRoots, const QStringList& nameFilters)
{
    QStringList files;
    for (const QString& relativeRoot : relativeRoots) {
        QDirIterator iterator(
            projectPath(relativeRoot), nameFilters, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            files.push_back(iterator.next());
        }
    }
    files.sort();
    return files;
}

QString relativeProjectPath(const QString& absolutePath)
{
    return QDir(projectPath(QString())).relativeFilePath(absolutePath);
}

QString matchingLines(const QString& filePath, const QList<QRegularExpression>& patterns)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("%1: cannot open file").arg(relativeProjectPath(filePath));
    }

    QStringList matches;
    int lineNumber = 0;
    while (!file.atEnd()) {
        ++lineNumber;
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        for (const QRegularExpression& pattern : patterns) {
            if (pattern.match(line).hasMatch()) {
                matches.push_back(QStringLiteral("%1:%2: %3")
                        .arg(relativeProjectPath(filePath))
                        .arg(lineNumber)
                        .arg(line));
                break;
            }
        }
    }
    return matches.join(QLatin1Char('\n'));
}

void verifySourceUrlReadOnly(const QMetaObject& metaObject)
{
    const int sourceUrlIndex = metaObject.indexOfProperty("sourceUrl");
    QVERIFY(sourceUrlIndex >= 0);

    const QMetaProperty sourceUrlProperty = metaObject.property(sourceUrlIndex);
    QVERIFY(sourceUrlProperty.hasNotifySignal());
    QVERIFY(!sourceUrlProperty.isWritable());
}

void verifyPrivateRouteSetter(const QString& relativePath)
{
    const QString header = readProjectFile(relativePath);
    const QRegularExpression propertyPattern(QStringLiteral(
        R"(Q_PROPERTY\(\s*QUrl\s+sourceUrl\s+READ\s+sourceUrl\s+NOTIFY\s+sourceUrlChanged\s*\))"));
    QVERIFY2(propertyPattern.match(header).hasMatch(),
        qPrintable(
            QStringLiteral("%1 sourceUrl property must remain read-only").arg(relativePath)));
    QVERIFY2(!header.contains(QStringLiteral("Q_INVOKABLE void setSourceUrl")),
        qPrintable(QStringLiteral("%1 must not expose setSourceUrl to QML").arg(relativePath)));

    const qsizetype privateIndex = header.indexOf(QStringLiteral("\nprivate:"));
    const qsizetype friendIndex
        = header.indexOf(QStringLiteral("friend class KiriDocumentSession;"));
    const qsizetype setterIndex = header.indexOf(QStringLiteral("void setSourceUrl"));
    QVERIFY2(privateIndex >= 0,
        qPrintable(QStringLiteral("%1 must have a private section").arg(relativePath)));
    QVERIFY2(friendIndex > privateIndex,
        qPrintable(QStringLiteral("%1 route setter friend must stay private").arg(relativePath)));
    QVERIFY2(setterIndex > friendIndex,
        qPrintable(QStringLiteral("%1 setSourceUrl must stay behind KiriDocumentSession")
                .arg(relativePath)));
}

QStringList existingProjectFiles(const QList<QString>& relativePaths)
{
    QStringList existing;
    for (const QString& relativePath : relativePaths) {
        if (QFileInfo::exists(projectPath(relativePath))) {
            existing.push_back(relativePath);
        }
    }
    return existing;
}
}

void TestArchitectureBoundaries::leafDocumentSourceRoutesAreReadOnlyPublicObservations()
{
    verifySourceUrlReadOnly(KiriImageDocument::staticMetaObject);
    verifySourceUrlReadOnly(KiriVideoDocument::staticMetaObject);
}

void TestArchitectureBoundaries::leafDocumentRouteSettersStayPrivateToTheSession()
{
    verifyPrivateRouteSetter(QStringLiteral("src/facade/kiriimagedocument.h"));
    verifyPrivateRouteSetter(QStringLiteral("src/facade/kirivideodocument.h"));
}

void TestArchitectureBoundaries::qmlCannotSetLeafDocumentRoutes()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(
            QStringLiteral(R"(\b(?:imageDocument|videoDocument)\s*\.\s*sourceUrl\s*=)")),
        QRegularExpression(
            QStringLiteral(R"(\b(?:imageDocument|videoDocument)\s*\.\s*setSourceUrl\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(\bdocumentSession\s*\.\s*(?:imageDocument|videoDocument)\s*\.\s*sourceUrl\s*=)")),
        QRegularExpression(QStringLiteral(
            R"(\bdocumentSession\s*\.\s*(?:imageDocument|videoDocument)\s*\.\s*setSourceUrl\s*\()")),
        QRegularExpression(QStringLiteral(R"(^\s*sourceUrl\s*:)")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlCannotInvokeLeafImageNavigationOrDeletion()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriimagedocument.h"));
    const QStringList forbiddenInvokables {
        QStringLiteral("Q_INVOKABLE void openPreviousPage"),
        QStringLiteral("Q_INVOKABLE void openNextPage"),
        QStringLiteral("Q_INVOKABLE void openPreviousSinglePage"),
        QStringLiteral("Q_INVOKABLE void openNextSinglePage"),
        QStringLiteral("Q_INVOKABLE void openImageAtPage"),
        QStringLiteral("Q_INVOKABLE void openPreviousContainer"),
        QStringLiteral("Q_INVOKABLE void openNextContainer"),
        QStringLiteral("Q_INVOKABLE void deleteDisplayedFile"),
    };
    QStringList exposedInvokables;
    for (const QString& invokable : forbiddenInvokables) {
        if (header.contains(invokable)) {
            exposedInvokables.push_back(invokable);
        }
    }
    QVERIFY2(exposedInvokables.isEmpty(), qPrintable(exposedInvokables.join(QLatin1Char('\n'))));

    const QList<QRegularExpression> forbiddenQmlCalls {
        QRegularExpression(QStringLiteral(
            R"(\bimageDocument\s*\.\s*(?:openPreviousPage|openNextPage|openPreviousSinglePage|openNextSinglePage|openImageAtPage|openPreviousContainer|openNextContainer|deleteDisplayedFile)\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(\bdocumentSession\s*\.\s*imageDocument\s*\.\s*(?:openPreviousPage|openNextPage|openPreviousSinglePage|openNextSinglePage|openImageAtPage|openPreviousContainer|openNextContainer|deleteDisplayedFile)\s*\()")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenQmlCalls);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotWriteSharedActionState()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(
            R"(\bsourceAction\s*\.\s*(?:enabled|checked|checkable|text|shortcut)\s*=)")),
        QRegularExpression(QStringLiteral(
            R"(\bsourceAction\s*\.\s*(?:setEnabled|setChecked|setText|setShortcut)\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(\bactionForId\s*\([^)]*\)\s*\.\s*(?:enabled|checked|checkable|text|shortcut)\s*=)")),
        QRegularExpression(QStringLiteral(
            R"(\bactionForId\s*\([^)]*\)\s*\.\s*(?:setEnabled|setChecked|setText|setShortcut)\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(\bapplication\s*\.\s*action\s*\([^)]*\)\s*\.\s*(?:enabled|checked|checkable|text|shortcut)\s*=)")),
        QRegularExpression(QStringLiteral(
            R"(\bapplication\s*\.\s*action\s*\([^)]*\)\s*\.\s*(?:setEnabled|setChecked|setText|setShortcut)\s*\()")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlActionProxiesDoNotOverrideRuntimeActionState()
{
    const QString managedAction = readProjectFile(QStringLiteral("src/qml/ManagedAction.qml"));
    const QString actionProxy = readProjectFile(QStringLiteral("src/qml/ActionProxy.qml"));
    const QString combined = managedAction + QLatin1Char('\n') + actionProxy;
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bproxy(?:Enabled|Checked|Checkable)\b)")),
        QRegularExpression(QStringLiteral(R"(\b(?:enabled|checked|checkable)Override\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(combined);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotOwnSharedActionPolicy()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bupdateActionState\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bfunction\s+dispatchAction\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bonActionTriggered\s*:)")),
        QRegularExpression(QStringLiteral(R"(\bonActionTriggered\s*\()")),
        QRegularExpression(QStringLiteral(R"(^\s*Shortcut\s*\{)")),
    };

    QStringList violations;
    for (const QString& relativePath : { QStringLiteral("src/qml/ImageActions.qml"),
             QStringLiteral("src/qml/ImageShortcuts.qml") }) {
        const QString matches = matchingLines(projectPath(relativePath), forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotComputePannabilityActionGate()
{
    const QString mainQml = readProjectFile(QStringLiteral("src/qml/Main.qml"));
    const QString applicationHeader
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString applicationImplementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bimageMode\s*&&\s*[\w.]+imagePannable\b)")),
        QRegularExpression(QStringLiteral(R"(\bimagePannable\s*&&\s*\w+\.imageMode\b)")),
        QRegularExpression(QStringLiteral(R"(\bimageInteractionSurface\s*\.\s*imagePannable\b)")),
    };
    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(mainQml);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
    QVERIFY(!applicationHeader.contains(QStringLiteral("bool imagePannable")));
    QVERIFY(!applicationHeader.contains(QStringLiteral("m_imagePannable")));
    QVERIFY(!applicationImplementation.contains(QStringLiteral("m_imagePannable")));
}

void TestArchitectureBoundaries::qmlDoesNotRecomputeSharedMediaReadiness()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(
            R"(\bimageDocument\s*\.\s*status\s*===\s*KiriImageDocument\s*\.\s*Ready)")),
        QRegularExpression(
            QStringLiteral(R"(\bimageDocument\s*\.\s*unsupportedOpenedCollectionVideo\b)")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotWriteDurablePresentationState()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*zoomPercent\s*=)")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*setZoomPercent\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*setFitMode\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*resetZoom\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*twoPageModeEnabled\s*=)")),
        QRegularExpression(
            QStringLiteral(R"(\bimageDocument\s*\.\s*rightToLeftReadingEnabled\s*=)")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*viewportContentPosition\s*=)")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*visibleItemRect\s*=)")),
        QRegularExpression(
            QStringLiteral(R"(\bimageDocument\s*\.\s*setViewportContentPosition\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*setVisibleItemRect\s*\()")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::imageDocumentHasNoPublicPresentationBackdoorSetters()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriimagedocument.h"));
    const qsizetype privateIndex = header.indexOf(QStringLiteral("\nprivate:"));
    QVERIFY(privateIndex > 0);
    const QString publicHeader = header.left(privateIndex);

    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bvoid\s+setViewportContentPosition\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bvoid\s+setVisibleItemRect\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bvoid\s+setZoomPercent\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bvoid\s+setTwoPageModeEnabled\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bvoid\s+setRightToLeftReadingEnabled\s*\()")),
    };
    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        const QRegularExpressionMatch match = pattern.match(publicHeader);
        if (match.hasMatch()) {
            violations.push_back(match.captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlViewportReportsPositionsThroughCommandBridge()
{
    const QString viewport = readProjectFile(QStringLiteral("src/qml/ImageViewport.qml"));
    QVERIFY(!viewport.contains(QStringLiteral("Qt.callLater")));
    QVERIFY(!viewport.contains(QStringLiteral("requestViewportContentPosition(")));
    QVERIFY(!viewport.contains(QStringLiteral("beginViewportCommandApplication(")));
    QVERIFY(!viewport.contains(QStringLiteral("completeViewportCommandApplication(")));
    QVERIFY(!viewport.contains(QStringLiteral("acknowledgeViewportCommand(")));
    QVERIFY(
        !QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*viewportContentPosition\s*=)"))
            .match(viewport)
            .hasMatch());
    QVERIFY(!QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*visibleItemRect\s*=)"))
            .match(viewport)
            .hasMatch());
    QVERIFY(!QRegularExpression(
        QStringLiteral(R"(\bimageDocument\s*\.\s*setViewportContentPosition\s*\()"))
            .match(viewport)
            .hasMatch());
    QVERIFY(!QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*setVisibleItemRect\s*\()"))
            .match(viewport)
            .hasMatch());
}

void TestArchitectureBoundaries::qmlViewportCommandHandlersDoNotApplyProjectionDirectly()
{
    const QString viewport = readProjectFile(QStringLiteral("src/qml/ImageViewport.qml"));
    const QStringList commandHandlers {
        QStringLiteral("moveContentPosition"),
        QStringLiteral("applyDisplayedImageInitialContentPosition"),
        QStringLiteral("panBy"),
        QStringLiteral("panToBottomRight"),
        QStringLiteral("panToTopLeft"),
        QStringLiteral("zoomByStep"),
        QStringLiteral("toggleFitOrActualSize"),
    };

    QStringList violations;
    for (const QString& handler : commandHandlers) {
        const QRegularExpression functionPattern(
            QStringLiteral(R"(function\s+%1\s*\([^)]*\)\s*\{([\s\S]*?)\n    \})")
                .arg(QRegularExpression::escape(handler)));
        QRegularExpressionMatchIterator matches = functionPattern.globalMatch(viewport);
        QVERIFY2(matches.hasNext(),
            qPrintable(QStringLiteral("ImageViewport.qml must define %1()").arg(handler)));

        while (matches.hasNext()) {
            if (matches.next().captured(1).contains(QStringLiteral("applyViewportProjection("))) {
                violations.push_back(QStringLiteral("%1()").arg(handler));
            }
        }
    }

    QVERIFY2(violations.isEmpty(),
        qPrintable(QStringLiteral("Viewport command handlers must rely on projection changes: %1")
                .arg(violations.join(QStringLiteral(", ")))));
}

void TestArchitectureBoundaries::qmlViewportUsesOpaqueRevisionTokens()
{
    const QString viewport = readProjectFile(QStringLiteral("src/qml/ImageViewport.qml"));
    const QString imageDocumentHeader
        = readProjectFile(QStringLiteral("src/facade/kiriimagedocument.h"));
    const QList<QRegularExpression> qmlForbiddenPatterns {
        QRegularExpression(QStringLiteral(
            R"(\bproperty\s+(?:int|real|double|string|var)\s+appliedViewport(?:Command|Observation)Revision(?:Token)?\b)")),
        QRegularExpression(QStringLiteral(R"(\bviewport(?:Command|Observation)Revision\b)")),
    };
    const QList<QRegularExpression> headerForbiddenPatterns {
        QRegularExpression(QStringLiteral(
            R"(Q_PROPERTY\s*\(\s*quint64\s+viewport(?:Command|AppliedCommand|Observation)Revision\b)")),
        QRegularExpression(
            QStringLiteral(R"(Q_INVOKABLE\s+quint64\s+requestViewportContentPosition\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(Q_INVOKABLE\s+bool\s+(?:beginViewportCommandApplication|completeViewportCommandApplication|acknowledgeViewportCommand)\s*\(\s*quint64\b)")),
    };
    QStringList violations;
    for (const QRegularExpression& pattern : qmlForbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(viewport);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }
    for (const QRegularExpression& pattern : headerForbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(imageDocumentHeader);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
    QVERIFY(!viewport.contains(QStringLiteral("viewportCommandRevisionToken")));
    QVERIFY(!viewport.contains(QStringLiteral("viewportObservationRevisionToken")));
}

void TestArchitectureBoundaries::leafDocumentsAreNotProductionQmlCreatable()
{
    const QString imageHeader = readProjectFile(QStringLiteral("src/facade/kiriimagedocument.h"));
    const QString videoHeader = readProjectFile(QStringLiteral("src/facade/kirivideodocument.h"));
    QVERIFY(!imageHeader.contains(QStringLiteral("QML_ELEMENT")));
    QVERIFY(!videoHeader.contains(QStringLiteral("QML_ELEMENT")));
    QVERIFY(imageHeader.contains(QStringLiteral("QML_UNCREATABLE")));
    QVERIFY(videoHeader.contains(QStringLiteral("QML_UNCREATABLE")));
}

void TestArchitectureBoundaries::actionUiGatesAreRevisionedSnapshots()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString implementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));
    QVERIFY(!header.contains(QStringLiteral("updateActionUiState(bool")));
    QVERIFY(!implementation.contains(QStringLiteral("updateActionUiState(bool")));
}

void TestArchitectureBoundaries::qmlDoesNotManufactureStaleSensitiveRevisions()
{
    const QString mainQml = readProjectFile(QStringLiteral("src/qml/Main.qml"));
    const QString videoViewport = readProjectFile(QStringLiteral("src/qml/VideoViewport.qml"));
    const QString applicationHeader
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString sessionHeader
        = readProjectFile(QStringLiteral("src/facade/kiridocumentsession.h"));
    const QString sessionRuntimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntime.h"));
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(
            QStringLiteral(R"(\bproperty\s+(?:int|real|double|var)\s+\w*Revision\b)")),
        QRegularExpression(QStringLiteral(R"(\b\w+Revision\s*\+=)")),
        QRegularExpression(QStringLiteral(R"(\bnext\w*Revision\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(Q_INVOKABLE\s+void\s+updateActionUiGateSnapshot\s*\(\s*quint64\s+revision\b)")),
        QRegularExpression(
            QStringLiteral(R"(reportVideoOutputSurfaceClaim\s*\(\s*quint64\s+claimRevision\b)")),
    };
    QStringList violations;
    for (const QString& source :
        { mainQml, videoViewport, applicationHeader, sessionHeader, sessionRuntimeHeader }) {
        for (const QRegularExpression& pattern : forbiddenPatterns) {
            QRegularExpressionMatchIterator iterator = pattern.globalMatch(source);
            while (iterator.hasNext()) {
                violations.push_back(iterator.next().captured(0));
            }
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::imageActionAvailabilityFacadeIsNotWritableQmlBackdoor()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/imageactionavailability.h"));
    QVERIFY(!header.contains(QStringLiteral("QML_ELEMENT")));
    QVERIFY(!header.contains(QStringLiteral("WRITE setImageReady")));
    QVERIFY(!header.contains(QStringLiteral("WRITE setTwoPageModeEnabled")));
    QVERIFY(!header.contains(QStringLiteral("WRITE setRightToLeftReadingEnabled")));
}

void TestArchitectureBoundaries::fixedViewerShortcutsDoNotBypassRuntimeRouting()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString implementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));
    QVERIFY(!header.contains(QStringLiteral("handleFixedShortcutEvent")));
    QVERIFY(!header.contains(QStringLiteral("handleHorizontalArrowShortcut")));
    QVERIFY(!header.contains(QStringLiteral("handleSinglePageArrowShortcut")));
    QVERIFY(!header.contains(QStringLiteral("handleVerticalPanShortcut")));
    QVERIFY(!implementation.contains(QStringLiteral("class FixedShortcutEventFilter")));
    QVERIFY(!implementation.contains(QStringLiteral("handleFixedShortcutEvent")));
}

void TestArchitectureBoundaries::qmlDoesNotExposeFixedViewerScanCommandRoutes()
{
    const QString imageViewport = readProjectFile(QStringLiteral("src/qml/ImageViewport.qml"));
    const QString imageInteractionSurface
        = readProjectFile(QStringLiteral("src/qml/ImageViewportInteractionSurface.qml"));
    const QList<QRegularExpression> forbiddenQmlPatterns {
        QRegularExpression(QStringLiteral(R"(\brequestViewportScan(?:Forward|Backward)\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bfunction\s+scan(?:Forward|Backward)\s*\()")),
    };

    QStringList violations;
    for (const QString& source : { imageViewport, imageInteractionSurface }) {
        for (const QRegularExpression& pattern : forbiddenQmlPatterns) {
            QRegularExpressionMatchIterator iterator = pattern.globalMatch(source);
            while (iterator.hasNext()) {
                violations.push_back(iterator.next().captured(0));
            }
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::videoSeekShortcutsRouteThroughApplicationRuntime()
{
    const QString videoViewport = readProjectFile(QStringLiteral("src/qml/VideoViewport.qml"));
    const QList<QRegularExpression> forbiddenVideoViewportPatterns {
        QRegularExpression(QStringLiteral(R"(\bKeys\s*\.\s*onPressed\b)")),
        QRegularExpression(QStringLiteral(R"(\bhandleSeekShortcut\b)")),
        QRegularExpression(QStringLiteral(R"(\bseekByShortcut\b)")),
        QRegularExpression(QStringLiteral(R"(\bvideoDocument\s*\.\s*seekable\b)")),
        QRegularExpression(QStringLiteral(R"(\bvideoDocument\s*\.\s*seekBy\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bQt\s*\.\s*Key_(?:Left|Right|Up|Down)\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenVideoViewportPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(videoViewport);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotOwnVideoPlaybackControlState()
{
    const QString videoViewport = readProjectFile(QStringLiteral("src/qml/VideoViewport.qml"));
    const QString videoControls
        = readProjectFile(QStringLiteral("src/qml/VideoFloatingControls.qml"));
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bproperty\s+real\s+(?:durationMs|positionMs)\b)")),
        QRegularExpression(QStringLiteral(R"(\bproperty\s+bool\s+explicitlyRevealed\b)")),
        QRegularExpression(QStringLiteral(R"(\bid\s*:\s*hideTimer\b)")),
        QRegularExpression(QStringLiteral(
            R"(\bfunction\s+(?:syncDocumentTiming|syncDocumentPosition|syncTimelineToDocument|commitTimelineSeek|scheduleAutoHide|revealControls)\b)")),
        QRegularExpression(QStringLiteral(
            R"(\breadonly\s+property\s+bool\s+(?:fixedControlsMode|autoHideEligible|timelineInteractive)\b)")),
        QRegularExpression(QStringLiteral(R"(\bvideoDocument\s*\.\s*setPosition\s*\()")),
    };

    QStringList violations;
    for (const QString& source : { videoViewport, videoControls }) {
        for (const QRegularExpression& pattern : forbiddenPatterns) {
            QRegularExpressionMatchIterator iterator = pattern.globalMatch(source);
            while (iterator.hasNext()) {
                violations.push_back(iterator.next().captured(0));
            }
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotOwnFullscreenChromeLifecycle()
{
    const QString mainQml = readProjectFile(QStringLiteral("src/qml/Main.qml"));
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bproperty\s+int\s+visibilityBeforeFullscreen\b)")),
        QRegularExpression(
            QStringLiteral(R"(\bproperty\s+bool\s+fullscreen(?:PointerHidden|ToolBarRevealed)\b)")),
        QRegularExpression(QStringLiteral(
            R"(\bfunction\s+(?:toggleFullScreen|restoredVisibility|revealFullscreenToolBar|scheduleFullscreenToolBarHide)\b)")),
        QRegularExpression(
            QStringLiteral(R"(\bid\s*:\s*fullscreen(?:ToolBarHide|PointerIdle)Timer\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(mainQml);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::applicationFacadeDoesNotOwnFixedViewerCommandRouting()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString implementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));

    QVERIFY(!header.contains(QStringLiteral("navigation/imageshortcutnavigationpolicy.h")));
    QVERIFY(!header.contains(QStringLiteral("ImageShortcutNavigationPolicy m_navigationPolicy")));
    QVERIFY(!header.contains(QStringLiteral("application/applicationcommandportsource.h")));
    QVERIFY(!header.contains(QStringLiteral("private kiriview::ApplicationActions::"
                                            "ApplicationCommandPortSource")));
    QVERIFY(!header.contains(QStringLiteral("commandRouterShellPorts")));
    QVERIFY(!implementation.contains(QStringLiteral("m_navigationPolicy.")));
    QVERIFY(!implementation.contains(QStringLiteral("keyboardPanDistance")));
}

void TestArchitectureBoundaries::applicationFacadeDoesNotOwnActionStateSourceAttachment()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.h"));
    const QString implementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));

    QVERIFY(!header.contains(QStringLiteral("rebuildActionState")));
    QVERIFY(!header.contains(QStringLiteral("connectActionStateSources")));
    QVERIFY(!header.contains(QStringLiteral("disconnectActionStateSources")));
    QVERIFY(!header.contains(QStringLiteral("actionStateSnapshot")));
    QVERIFY(!header.contains(QStringLiteral("m_actionStateConnections")));
    QVERIFY(!header.contains(QStringLiteral("m_actionUiGateRevision")));
    QVERIFY(!header.contains(QStringLiteral("m_helpDialogOpen")));
    QVERIFY(!header.contains(QStringLiteral("m_textInputFocused")));
    QVERIFY(!implementation.contains(QStringLiteral("KiriViewApplication::rebuildActionState")));
    QVERIFY(
        !implementation.contains(QStringLiteral("KiriViewApplication::connectActionStateSources")));
    QVERIFY(!implementation.contains(
        QStringLiteral("KiriViewApplication::disconnectActionStateSources")));
    QVERIFY(!implementation.contains(QStringLiteral("KiriViewApplication::actionStateSnapshot")));
}

void TestArchitectureBoundaries::applicationCommandRouterPortsAreGroupedByOwner()
{
    const QString header
        = readProjectFile(QStringLiteral("src/application/applicationcommandrouter.h"));
    const qsizetype portsIndex
        = header.indexOf(QStringLiteral("struct ApplicationCommandRouterPorts"));
    QVERIFY(portsIndex >= 0);
    const qsizetype portsEnd = header.indexOf(QStringLiteral("};"), portsIndex);
    QVERIFY(portsEnd > portsIndex);
    const QString portsBlock = header.mid(portsIndex, portsEnd - portsIndex);
    QVERIFY(!portsBlock.contains(QStringLiteral("std::function<")));
}

void TestArchitectureBoundaries::applicationFacadeDoesNotOwnActionCommandSwitch()
{
    const QString implementation
        = readProjectFile(QStringLiteral("src/facade/kiriviewapplication.cpp"));

    QVERIFY(!implementation.contains(QStringLiteral("switch (actionId)")));
    QVERIFY(!implementation.contains(QStringLiteral("requestFitMode(KiriImageDocument::ZoomMode")));
    QVERIFY(!implementation.contains(QStringLiteral("deleteDisplayedFile(KiriDocumentSession::")));
    QVERIFY(!implementation.contains(QStringLiteral("KiriViewApplication::commandRouterInput")));
    QVERIFY(!implementation.contains(QStringLiteral("KiriViewApplication::commandRouterPorts")));
}

void TestArchitectureBoundaries::sessionPublicProjectionHasNoPartialUpdateBackdoor()
{
    const QString header = readProjectFile(QStringLiteral("src/session/documentsessionstate.h"));
    const QString implementation
        = readProjectFile(QStringLiteral("src/session/documentsessionstate.cpp"));
    QVERIFY(!header.contains(QStringLiteral("updatePublicProjection")));
    QVERIFY(!implementation.contains(QStringLiteral("updatePublicProjection")));
    QVERIFY(!implementation.contains(QStringLiteral("applyPublicProjection")));
}

void TestArchitectureBoundaries::sessionPublicProjectionDoesNotSampleLeafFacadesWhileApplying()
{
    const QString implementation = readProjectFile(
        QStringLiteral("src/session/documentsessionpublicsnapshotinputbuilder.cpp"));
    const qsizetype functionIndex
        = implementation.indexOf(QStringLiteral("buildDocumentSessionPublicSnapshotInput("));
    QVERIFY(functionIndex >= 0);
    const qsizetype nextFunctionIndex
        = implementation.indexOf(QStringLiteral("\n}"), functionIndex);
    QVERIFY(nextFunctionIndex > functionIndex);
    const QString body = implementation.mid(functionIndex, nextFunctionIndex - functionIndex);

    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bm_imageDocument\s*\.)")),
        QRegularExpression(QStringLiteral(R"(\bm_videoDocument\s*\.)")),
    };
    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(body);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }
    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlDoesNotWriteSharedVideoOutputAttachment()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\b\w+\s*\.\s*videoOutput\s*=)")),
        QRegularExpression(QStringLiteral(R"(\bvideoDocument\s*\.\s*videoOutput\s*=)")),
        QRegularExpression(
            QStringLiteral(R"(\bdocumentSession\s*\.\s*videoDocument\s*\.\s*videoOutput\s*=)")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::videoOutputAttachmentIsNotWritablePublicVideoDocumentState()
{
    const QString header = readProjectFile(QStringLiteral("src/facade/kirivideodocument.h"));
    QVERIFY(!header.contains(QStringLiteral("WRITE setVideoOutput")));
    QVERIFY(!header.contains(QStringLiteral("Q_INVOKABLE void setVideoOutput")));
}

void TestArchitectureBoundaries::qmlDoesNotDeriveSharedControlPolicyFromLeafDocuments()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(
            QStringLiteral(R"(\bimageDocument\s*\.\s*openedCollectionScopeActive\b)")),
        QRegularExpression(QStringLiteral(R"(\bimageDocument\s*\.\s*rightToLeftReadingEnabled\b)")),
        QRegularExpression(
            QStringLiteral(R"(\bimageDocument\s*\.\s*rightToLeftReadingAvailable\b)")),
        QRegularExpression(QStringLiteral(
            R"(\bvideoDocument\s*\.\s*status\s*===\s*KiriVideoDocument\s*\.\s*Ready)")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlUsesCentralNavigationPresentationOrder()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(
            R"(\brightToLeftReadingActive\b[^\n]*\?[^\n]*(?:previous|next|first|last)(?:Image|Container)(?:Managed|Menu)?Action)")),
        QRegularExpression(QStringLiteral(
            R"((?:previous|next|first|last)(?:Image|Container)(?:Managed|Menu)?Action[^\n]*\brightToLeftReadingActive\b[^\n]*\?)")),
    };

    QStringList violations;
    for (const QString& filePath : productionQmlFiles()) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::qmlViewportUsesContextBridgeForRenderContextDiscovery()
{
    const QString viewport = readProjectFile(QStringLiteral("src/qml/ImageViewport.qml"));
    QVERIFY(!viewport.contains(QStringLiteral("KiriImageView {")));
    QVERIFY(!viewport.contains(QStringLiteral("primaryImageView")));
    QVERIFY(!viewport.contains(QStringLiteral("secondaryImageView")));
    QVERIFY(!viewport.contains(QStringLiteral("renderContextProviderEnabled")));
    QVERIFY(!viewport.contains(QStringLiteral("imageView.viewportPointInsideImage(")));
    QVERIFY(!viewport.contains(QStringLiteral("imageView.nearestImageViewportPoint(")));
    QVERIFY(!viewport.contains(QStringLiteral("onDisplayedImageInitialContentPositionRequested")));
}

void TestArchitectureBoundaries::providerRenderingRejectsTileSourceContracts()
{
    const QList<QString> forbiddenFiles {
        QStringLiteral("src/rendering/imagetile.cpp"),
        QStringLiteral("src/rendering/imagetile.h"),
        QStringLiteral("src/rendering/imagetilegeometrypolicy.cpp"),
        QStringLiteral("src/rendering/imagetilegeometrypolicy.h"),
        QStringLiteral("src/rendering/imagetilevisibility.cpp"),
        QStringLiteral("src/rendering/imagetilevisibility.h"),
        QStringLiteral("src/rendering/qimagereadertilesource.cpp"),
        QStringLiteral("src/rendering/qimagereadertilesource.h"),
        QStringLiteral("src/rendering/svgtilesource.cpp"),
        QStringLiteral("src/rendering/svgtilesource.h"),
        QStringLiteral("src/rendering/heiftilesource.cpp"),
        QStringLiteral("src/rendering/heiftilesource.h"),
        QStringLiteral("src/rendering/qimagereaderscaledlevelcache.cpp"),
        QStringLiteral("src/rendering/qimagereaderscaledlevelcache.h"),
        QStringLiteral("src/policy/imagetilegeometry.rs"),
    };

    QStringList violations = existingProjectFiles(forbiddenFiles);

    const QStringList files = projectFilesUnder({ QStringLiteral("src") },
        { QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.rs"),
            QStringLiteral("*.txt") });
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bclass\s+ImageTileSource\b)")),
        QRegularExpression(QStringLiteral(R"(\bImageTileSource\b)")),
        QRegularExpression(QStringLiteral(R"(\bDecodedTile\b)")),
        QRegularExpression(QStringLiteral(R"(\bdecodeTile\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bImageTileGeometry\b)")),
        QRegularExpression(QStringLiteral(R"(\bImageTileVisibility\b)")),
        QRegularExpression(QStringLiteral(R"(\bQImageReaderTileSource\b)")),
        QRegularExpression(QStringLiteral(R"(\bSvgTileSource\b)")),
        QRegularExpression(QStringLiteral(R"(\bHeifTileSource\b)")),
        QRegularExpression(QStringLiteral(R"(\bQImageReaderScaledLevelCache\b)")),
    };
    for (const QString& filePath : files) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::productionImageDisplayUsesProviderPathOnly()
{
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(<rhi/qrhi\.h>)")),
        QRegularExpression(QStringLiteral(R"(\bQQuickWindow::rhi\b)")),
        QRegularExpression(QStringLiteral(R"((?:->|\.)rhi\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bQSGRenderNode\b)")),
        QRegularExpression(QStringLiteral(R"(\bQSGTexture\b)")),
        QRegularExpression(QStringLiteral(R"(\bQSGTextureProvider\b)")),
        QRegularExpression(QStringLiteral(R"(\bQQuickFramebufferObject\b)")),
        QRegularExpression(QStringLiteral(R"(\bupdatePaintNode\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bItemHasContents\b)")),
        QRegularExpression(QStringLiteral(R"(\bKiriImageView\b)")),
        QRegularExpression(QStringLiteral(R"(\bKiriImageRenderNode\b)")),
        QRegularExpression(QStringLiteral(R"(\bImageRenderFrame\b)")),
        QRegularExpression(QStringLiteral(R"(\bDisplayedImageRenderSnapshot\b)")),
        QRegularExpression(QStringLiteral(R"(\bDisplayedImageSurface\b)")),
        QRegularExpression(QStringLiteral(R"(\bStaticTileSurface\b)")),
        QRegularExpression(QStringLiteral(R"(\bStaticImagePayload\b)")),
        QRegularExpression(QStringLiteral(R"(\bcompatibilityStaticImage\b)")),
        QRegularExpression(QStringLiteral(R"(\bsetStaticImage\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bstaticTileCache\w*)")),
        QRegularExpression(QStringLiteral(R"(\bStaticTileCache\w*)")),
        QRegularExpression(QStringLiteral(R"(\bImageTileDecode(?:Scheduler|Runtime|State)\b)")),
        QRegularExpression(QStringLiteral(R"(\bDecodedTileCache\b)")),
        QRegularExpression(QStringLiteral(R"(\bscheduleVisibleTileDecode\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bImageDocumentChange::RenderFrame\b)")),
        QRegularExpression(QStringLiteral(R"(\bImageDocumentChange::Repaint\b)")),
        QRegularExpression(QStringLiteral(R"(\bImageDocumentPublicSignal::Repaint\b)")),
        QRegularExpression(QStringLiteral(R"(\brepaintRequested\b)")),
    };

    QStringList violations;
    const QStringList files = projectFilesUnder({ QStringLiteral("src") },
        { QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.qml"),
            QStringLiteral("*.txt") });
    for (const QString& filePath : files) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::openedCollectionThumbnailEligibilityUsesSharedPolicy()
{
    const QString policyHeader
        = readProjectFile(QStringLiteral("src/archive/openedcollectionthumbnailpolicy.h"));
    const QString karchiveBackend
        = readProjectFile(QStringLiteral("src/archive/mediaentrysourcebackend_karchive.cpp"));

    QVERIFY(!policyHeader.contains(
        QStringLiteral("openedCollectionRootSchemeSupportsThumbnailContentIdentity")));

    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(
            QStringLiteral(R"(\bopenedCollectionRootSchemeSupportsThumbnailContentIdentity\b)")),
        QRegularExpression(QStringLiteral(R"(\bisSupportedImageFileName\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bisComicBook\s*\()")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(karchiveBackend);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::decodingUsesNeutralThumbnailContracts()
{
    const QStringList files = projectFilesUnder(
        { QStringLiteral("src/decoding") }, { QStringLiteral("*.cpp"), QStringLiteral("*.h") });
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(#include\s+"session/thumbnail[^"]+")")),
        QRegularExpression(
            QStringLiteral(R"(#include\s+"session/activenavigationthumbnaildemand\.h")")),
    };

    QStringList violations;
    for (const QString& filePath : files) {
        const QString matches = matchingLines(filePath, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::cppNamespaceIsLowercaseKiriview()
{
    const QList<QRegularExpression> forbiddenCppPatterns {
        QRegularExpression(QStringLiteral(R"(\bnamespace\s+KiriView\b)")),
        QRegularExpression(QStringLiteral(R"(\bKiriView::)")),
        QRegularExpression(QStringLiteral(R"(\busing\s+namespace\s+KiriView\b)")),
    };
    const QList<QRegularExpression> forbiddenRustPatterns {
        QRegularExpression(QStringLiteral(R"(namespace\s*=\s*"KiriView")")),
    };

    QStringList violations;
    for (const QString& filePath : projectFilesUnder(QStringList { QStringLiteral("src") },
             QStringList {
                 QStringLiteral("*.cpp"),
                 QStringLiteral("*.h"),
             })) {
        const QString matches = matchingLines(filePath, forbiddenCppPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }
    for (const QString& filePath : projectFilesUnder(QStringList { QStringLiteral("src/policy") },
             QStringList { QStringLiteral("*.rs") })) {
        const QString matches = matchingLines(filePath, forbiddenRustPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::documentSessionUsesThumbnailStripDependencyPort()
{
    const QString documentSessionHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));

    const QList<QRegularExpression> rawThumbnailProviderFields {
        QRegularExpression(QStringLiteral(R"(\bactiveNavigationThumbnailLookupProvider\b)")),
        QRegularExpression(QStringLiteral(R"(\bactiveNavigationThumbnailGenerationProvider\b)")),
        QRegularExpression(QStringLiteral(R"(\bactiveNavigationThumbnailSourceAdapter\b)")),
        QRegularExpression(QStringLiteral(R"(\bactiveNavigationThumbnailWorkerScheduler\b)")),
        QRegularExpression(QStringLiteral(R"(\bactiveNavigationThumbnailImageStore\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : rawThumbnailProviderFields) {
        if (pattern.match(documentSessionHeader).hasMatch()) {
            violations.push_back(pattern.pattern());
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::documentSessionUsesOpenWithRuntime()
{
    const QString documentSessionHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));

    const QList<QRegularExpression> rawOpenWithFields {
        QRegularExpression(QStringLiteral(R"(\bm_mediaOpenWithProvider\b)")),
        QRegularExpression(QStringLiteral(R"(\bm_mediaOpenWithJob\b)")),
        QRegularExpression(QStringLiteral(R"(\bm_mediaOpenWithOperation\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : rawOpenWithFields) {
        if (pattern.match(documentSessionHeader).hasMatch()) {
            violations.push_back(pattern.pattern());
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::documentSessionOpenWithUsesNamedPlanPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("currentMediaOpenWithPlan")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("currentMediaOpenWithPlan()")));
}

void TestArchitectureBoundaries::activeNavigationThumbnailRuntimeUsesCanonicalThumbnailSourceKey()
{
    const QString thumbnailRuntimeHeader
        = readProjectFile(QStringLiteral("src/session/activenavigationthumbnailruntime.h"));
    const QString thumbnailRuntimeSource
        = readProjectFile(QStringLiteral("src/session/activenavigationthumbnailruntime.cpp"));

    QVERIFY(!thumbnailRuntimeHeader.contains(
        QStringLiteral("struct ActiveNavigationThumbnailSourceKey")));
    QVERIFY(!thumbnailRuntimeHeader.contains(QStringLiteral("static bool sameSourceKey")));
    QVERIFY(!thumbnailRuntimeSource.contains(
        QStringLiteral("ActiveNavigationThumbnailSourceKey sourceKeyForRow")));
}

void TestArchitectureBoundaries::liveDirectoryWatchUsesProviderBoundary()
{
    const QString entryHeader = readProjectFile(
        QStringLiteral("src/navigation/imagedocumentpagecandidatedirectoryentry.h"));
    const QString entryImplementation = readProjectFile(
        QStringLiteral("src/navigation/imagedocumentpagecandidatedirectoryentry.cpp"));
    const QString entryCombined = entryHeader + QLatin1Char('\n') + entryImplementation;
    QVERIFY2(!entryCombined.contains(QStringLiteral("KCoreDirLister")),
        "ImageDocumentPageCandidateDirectoryEntry must consume watch provider events instead of "
        "owning KDE listers");
}

void TestArchitectureBoundaries::mediaFormatRegistryDoesNotOwnLocalizedDialogLabels()
{
    const QString mediaRegistryHeader
        = readProjectFile(QStringLiteral("src/navigation/mediaformatregistry.h"));
    const QString mediaRegistrySource
        = readProjectFile(QStringLiteral("src/navigation/mediaformatregistry.cpp"));
    const QString combined = mediaRegistryHeader + QLatin1Char('\n') + mediaRegistrySource;

    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(KLocalizedString)")),
        QRegularExpression(QStringLiteral(R"(\bki?18nc?\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bordinaryMediaOpenDialogNameFilters\b)")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(combined);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::asyncImageIoJobsDoNotOwnDecodeDataLoading()
{
    const QStringList relativePaths = existingProjectFiles({
        QStringLiteral("src/async/imageiojobs.h"),
        QStringLiteral("src/async/imageiojobs.cpp"),
    });
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(#include\s+"decoding/imagedecoderequest\.h")")),
        QRegularExpression(QStringLiteral(R"(\bImageDecodeRequest\b)")),
        QRegularExpression(QStringLiteral(R"(\bstartStoredImageDataLoad\b)")),
        QRegularExpression(QStringLiteral(R"(\bKIO::storedGet\b)")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString matches = matchingLines(projectPath(relativePath), forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::asyncImageIoJobsDoNotOwnDirectoryCandidateLoading()
{
    const QStringList relativePaths = existingProjectFiles({
        QStringLiteral("src/async/imageiojobs.h"),
        QStringLiteral("src/async/imageiojobs.cpp"),
    });
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(#include\s+"async/directorylistingjob\.h")")),
        QRegularExpression(QStringLiteral(R"(\bDirectoryItemListProvider\b)")),
        QRegularExpression(QStringLiteral(R"(\bstartDirectoryImageDocumentPageCandidateList\b)")),
        QRegularExpression(QStringLiteral(R"(\bstartDirectoryContainerCandidateList\b)")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString matches = matchingLines(projectPath(relativePath), forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::asyncImageIoJobsDoNotOwnOpenedCollectionCandidateLoading()
{
    const QStringList relativePaths = existingProjectFiles({
        QStringLiteral("src/async/imageiojobs.h"),
        QStringLiteral("src/async/imageiojobs.cpp"),
    });
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(#include\s+"archive/mediaentrysourcebackend\.h")")),
        QRegularExpression(QStringLiteral(R"(\bMediaEntrySourceCandidatesResult\b)")),
        QRegularExpression(QStringLiteral(R"(\bloadMediaEntrySourceCandidates\b)")),
        QRegularExpression(QStringLiteral(R"(\bstartOpenedCollectionCandidateList\b)")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString matches = matchingLines(projectPath(relativePath), forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::mediaEntrySourceStoreDoesNotDependOnDocumentPlanning()
{
    const QList<QString> relativePaths {
        QStringLiteral("src/archive/mediaentrysourcestore.h"),
        QStringLiteral("src/archive/mediaentrysourcestore.cpp"),
    };
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(#include\s+"document/)")),
        QRegularExpression(QStringLiteral(R"(\bImageDocumentSourceLoadRequest\b)")),
        QRegularExpression(QStringLiteral(R"(\bopenedCollectionScopeLoadPlan\s*\()")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString matches = matchingLines(projectPath(relativePath), forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::documentSessionDirectMediaScopeUsesNamedPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("directMediaNavigationLoadScope")));
    QVERIFY(!runtimeHeader.contains(QStringLiteral("activeDirectMediaCursorUrl")));
    QVERIFY(!runtimeHeader.contains(QStringLiteral("directMediaCursorMatches")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("directMediaNavigationLoadScope()")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("activeDirectMediaCursorUrl()")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("directMediaCursorMatches")));
}

void TestArchitectureBoundaries::documentSessionDirectMediaActivityUsesNamedPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("directMediaNavigationActive")));
    QVERIFY(
        !runtimeHeader.contains(QStringLiteral("directImageLoadMayUseImageDocumentSourceScope")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::directMediaNavigationActive")));
    QVERIFY(!runtimeSource.contains(QStringLiteral(
        "DocumentSessionRuntimeGraph::directImageLoadMayUseImageDocumentSourceScope")));
}

void TestArchitectureBoundaries::documentSessionMediaPredecodeInputUsesNamedPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("mediaPredecodeInput()")));
    QVERIFY(!runtimeHeader.contains(QStringLiteral("activeImageUsesImageDocumentSourceScope")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::mediaPredecodeInput")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::activeImageUsesImageDocumentSourceScope")));
}

void TestArchitectureBoundaries::documentSessionDirectMediaNavigationInputUsesNamedPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("directMediaActiveNavigationInput")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::directMediaActiveNavigationInput")));
}

void TestArchitectureBoundaries::documentSessionPublicSnapshotInputUsesNamedPort()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("publicSnapshotInput(")));
    QVERIFY(!runtimeHeader.contains(QStringLiteral("m_publicSnapshotInputRevision")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::publicSnapshotInput")));
}

void TestArchitectureBoundaries::documentSessionRouteRuntimePortsAreGrouped()
{
    const QString routeRuntimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionrouteruntime.h"));

    const qsizetype runtimePortsIndex
        = routeRuntimeHeader.indexOf(QStringLiteral("struct DocumentSessionRouteRuntimePorts"));
    QVERIFY(runtimePortsIndex >= 0);
    const qsizetype runtimePortsEnd
        = routeRuntimeHeader.indexOf(QStringLiteral("};"), runtimePortsIndex);
    QVERIFY(runtimePortsEnd > runtimePortsIndex);
    const QString runtimePortsBlock
        = routeRuntimeHeader.mid(runtimePortsIndex, runtimePortsEnd - runtimePortsIndex);
    QVERIFY(!runtimePortsBlock.contains(QStringLiteral("std::function<")));
}

void TestArchitectureBoundaries::documentSessionDirectMediaNavigationUsesCoordinator()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("DocumentSessionDirectMediaNavigationRuntime")));
    QVERIFY(!runtimeHeader.contains(
        QStringLiteral("DocumentSessionDirectMediaNavigationApplicationRuntime")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("DocumentSessionRuntimeGraph::openMedia(")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::refreshDirectMediaNavigation")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::finishDirectMediaNavigation")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::updateDirectMediaNavigationBoundaryState")));
}

void TestArchitectureBoundaries::documentSessionVideoDocumentSyncUsesRuntime()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("documentsessionvideodocumentsync.h")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::syncFromVideoDocument")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("documentSessionVideoDocumentSyncPlan")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("DocumentSessionVideoDocumentSyncOperation")));
}

void TestArchitectureBoundaries::documentSessionImageDocumentSyncUsesRuntime()
{
    const QString runtimeHeader
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.h"));
    const QString runtimeSource
        = readProjectFile(QStringLiteral("src/session/documentsessionruntimegraph.cpp"));

    QVERIFY(!runtimeHeader.contains(QStringLiteral("documentsessionimagedocumentsync.h")));
    QVERIFY(!runtimeHeader.contains(QStringLiteral("documentsessiondirectimagecursorsync.h")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionRuntimeGraph::syncDirectImageCursorFromDocument")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("documentSessionImageDocumentSyncPlan")));
    QVERIFY(!runtimeSource.contains(QStringLiteral("documentSessionDirectImageCursorSyncPlan")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionImageDocumentSyncDirectMediaOperation")));
    QVERIFY(!runtimeSource.contains(
        QStringLiteral("DocumentSessionImageDocumentSyncProjectionOperation")));
}

void TestArchitectureBoundaries::imageDocumentSourceLoadPlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.sourceLoad.")));
}

void TestArchitectureBoundaries::imageDocumentOpenPlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.open.")));
}

void TestArchitectureBoundaries::imageDocumentPredecodePlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.predecode.")));
}

void TestArchitectureBoundaries::imageDocumentNavigationPlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.navigation.")));
}

void TestArchitectureBoundaries::imageDocumentLifecyclePlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.lifecycle.")));
}

void TestArchitectureBoundaries::imageDocumentMediaEntrySourcePlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.mediaEntrySource.")));
}

void TestArchitectureBoundaries::imageDocumentSpreadPlanDispatchHasNamedExecutor()
{
    const QString runtimeExecutorSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimeplanexecutor.cpp"));

    QVERIFY(!runtimeExecutorSource.contains(QStringLiteral("m_operations.spread.")));
}

void TestArchitectureBoundaries::imageDocumentPredecodedImageLookupUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(
        !controllersSource.contains(QStringLiteral("m_predecodeController->findPredecodedImage")));
}

void TestArchitectureBoundaries::imageDocumentPrimaryPageSlotUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(
        !controllersSource.contains(QStringLiteral("m_spreadController->commitPrimaryPageSlot")));
    QVERIFY(
        !controllersSource.contains(QStringLiteral("m_spreadController->clearPrimaryPageSlot")));
}

void TestArchitectureBoundaries::imageDocumentNavigationSnapshotUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(!controllersSource.contains(
        QStringLiteral("m_navigationController->pageNavigationSnapshot")));
}

void TestArchitectureBoundaries::imageDocumentPageCandidateSnapshotUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(
        !controllersSource.contains(QStringLiteral("m_navigationService->pageCandidateSnapshot")));
}

void TestArchitectureBoundaries::imageDocumentAdjacentPredecodeSchedulingUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(!controllersSource.contains(QStringLiteral("ScheduleAdjacentImagePredecodeOperation")));
}

void TestArchitectureBoundaries::imageDocumentDeletionProgressUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(!controllersSource.contains(QStringLiteral("m_deletionController->inProgress")));
}

void TestArchitectureBoundaries::imageDocumentCurrentPageNumberUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(!controllersSource.contains(QStringLiteral("m_navigationService->currentPageNumber")));
}

void TestArchitectureBoundaries::imageDocumentAnimationLoadErrorUsesNamedPort()
{
    const QString controllersSource
        = readProjectFile(QStringLiteral("src/document/imagedocumentruntimegraph.cpp"));

    QVERIFY(!controllersSource.contains(
        QStringLiteral("m_openController->finishAnimationLoadWithError")));
}

void TestArchitectureBoundaries::imagePageSurfaceOwnersExposeNoPresentationState()
{
    const QString relativePath = QStringLiteral("src/presentation/imagepagesurfacecontroller.h");
    const QString header = readProjectFile(relativePath);
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bsetViewportSize\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bviewportSize\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bsetVisibleItemRect\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bvisibleItemRect\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bsetZoomPercent\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bzoomPercent\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bzoomMode\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bsetFitMode\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bresetZoom\s*\()")),
        QRegularExpression(QStringLiteral(R"(\brotateClockwise\s*\()")),
        QRegularExpression(QStringLiteral(R"(\brotateCounterclockwise\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bresetRotation\s*\()")),
        QRegularExpression(QStringLiteral(R"(\brotationDegrees\s*\()")),
        QRegularExpression(QStringLiteral(R"(\brenderSnapshot\s*\()")),
    };

    QStringList violations;
    for (const QRegularExpression& pattern : forbiddenPatterns) {
        QRegularExpressionMatchIterator iterator = pattern.globalMatch(header);
        while (iterator.hasNext()) {
            violations.push_back(iterator.next().captured(0));
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::imagePresentationPageSlotsUseDisplaySourceVariants()
{
    const QString header
        = readProjectFile(QStringLiteral("src/presentation/imagepresentationruntime.h"));

    QVERIFY(!header.contains(QStringLiteral("bool hasImage = false")));
    QVERIFY(!header.contains(QStringLiteral("ImageDisplaySourceSlot displaySource;")));
}

void TestArchitectureBoundaries::activePresentationDoesNotWritePageSurfacePresentationState()
{
    const QList<QString> relativePaths {
        QStringLiteral("src/presentation/imagepresentationruntime.cpp"),
        QStringLiteral("src/presentation/imagespreadpresentationcontroller.cpp"),
    };
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bsetVisibleItemRect\s*\()")),
        QRegularExpression(QStringLiteral(
            R"(\b(?:m_primaryPageSurface|primaryPageSurface|m_secondaryPageController|pageSurfaceController)\b[^\n]*(?:\.|->)\s*(?:setViewportSize|setZoomPercent|setFitMode|resetZoom|rotateClockwise|rotateCounterclockwise|resetRotation)\s*\()")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString path = projectPath(relativePath);
        if (!QFileInfo::exists(path)) {
            continue;
        }

        const QString matches = matchingLines(path, forbiddenPatterns);
        if (!matches.isEmpty()) {
            violations.push_back(matches);
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::productionFacadesDoNotExposePresentationBackdoorSetters()
{
    const QList<QString> relativePaths {
        QStringLiteral("src/facade/kiriimagedocument.h"),
        QStringLiteral("src/document/imagedocumentruntime.h"),
        QStringLiteral("src/presentation/imagespreadpresentationcontroller.h"),
    };
    const QList<QRegularExpression> forbiddenPatterns {
        QRegularExpression(QStringLiteral(R"(\bsetViewportContentPosition\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bsetVisibleItemRect\s*\()")),
        QRegularExpression(QStringLiteral(R"(\bsetZoomPercent\s*\()")),
    };

    QStringList violations;
    for (const QString& relativePath : relativePaths) {
        const QString contents = readProjectFile(relativePath);
        for (const QRegularExpression& pattern : forbiddenPatterns) {
            QRegularExpressionMatchIterator iterator = pattern.globalMatch(contents);
            while (iterator.hasNext()) {
                violations.push_back(
                    QStringLiteral("%1: %2").arg(relativePath, iterator.next().captured(0)));
            }
        }
    }

    QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
}

void TestArchitectureBoundaries::mediaInformationFacadeExposesSnapshotRevision()
{
    const int revisionIndex = KiriMediaInformation::staticMetaObject.indexOfProperty("revision");
    QVERIFY(revisionIndex >= 0);

    const QMetaProperty revisionProperty
        = KiriMediaInformation::staticMetaObject.property(revisionIndex);
    QVERIFY(revisionProperty.hasNotifySignal());
    QVERIFY(!revisionProperty.isWritable());
}

QTEST_GUILESS_MAIN(TestArchitectureBoundaries)

#include "test_architectureboundaries.moc"
