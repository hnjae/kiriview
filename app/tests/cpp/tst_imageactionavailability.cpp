// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "application/imageactionavailabilitypolicy.h"

#include <QObject>
#include <QTest>

class TestImageActionAvailability : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectionDerivesReadyAndModeAvailabilityFromSnapshot();
    void collectionReadingCommandsDoNotRequireReadyImage();
    void projectionDerivesShortcutGatesFromSnapshot();
    void policyScopeLookupUsesApplicationScope();
    void activeImageDocumentSourceScopeLookupUsesSessionNavigationInput();
};

void TestImageActionAvailability::projectionDerivesReadyAndModeAvailabilityFromSnapshot()
{
    ImageActionAvailabilityInput input;
    input.imageReady = true;
    input.twoPageModeAvailable = true;
    input.rightToLeftReadingAvailable = true;

    ImageActionAvailabilityProjection projection = imageActionAvailabilityProjection(input);

    QVERIFY(projection.canUseReadyActions);
    QVERIFY(projection.canUseTransformActions);
    QVERIFY(projection.canUseTwoPageModeActions);
    QVERIFY(projection.canUseRightToLeftReadingActions);
    QVERIFY(!projection.twoPageModeActive);
    QVERIFY(!projection.rightToLeftReadingActive);

    input.twoPageModeEnabled = true;
    input.rightToLeftReadingEnabled = true;
    projection = imageActionAvailabilityProjection(input);

    QVERIFY(projection.twoPageModeActive);
    QVERIFY(projection.rightToLeftReadingActive);
    QVERIFY(!projection.canUseTransformActions);
}

void TestImageActionAvailability::collectionReadingCommandsDoNotRequireReadyImage()
{
    ImageActionAvailabilityInput input;
    input.twoPageModeEnabled = true;
    input.twoPageModeAvailable = true;
    input.rightToLeftReadingEnabled = true;
    input.rightToLeftReadingAvailable = true;

    const ImageActionAvailabilityProjection projection = imageActionAvailabilityProjection(input);

    QVERIFY(!projection.canUseReadyActions);
    QVERIFY(!projection.canUseTransformActions);
    QVERIFY(projection.canUseTwoPageModeActions);
    QVERIFY(projection.canUseRightToLeftReadingActions);
    QVERIFY(projection.twoPageModeActive);
    QVERIFY(projection.rightToLeftReadingActive);
    QVERIFY(projection.collectionReadingShortcutsEnabled);
    QVERIFY(projection.collectionReadingViewerShortcutsEnabled);
    QVERIFY(!projection.twoPageViewerShortcutsEnabled);
}

void TestImageActionAvailability::projectionDerivesShortcutGatesFromSnapshot()
{
    ImageActionAvailabilityInput input;
    input.imageReady = true;
    input.imagePannable = true;
    input.containerNavigationAvailable = true;
    input.twoPageModeEnabled = true;
    input.twoPageModeAvailable = true;
    input.rightToLeftReadingAvailable = true;

    ImageActionAvailabilityProjection projection = imageActionAvailabilityProjection(input);

    QVERIFY(projection.helpShortcutsEnabled);
    QVERIFY(projection.viewerShortcutsEnabled);
    QVERIFY(projection.readyShortcutsEnabled);
    QVERIFY(projection.readyViewerShortcutsEnabled);
    QVERIFY(projection.twoPageViewerShortcutsEnabled);
    QVERIFY(projection.collectionReadingShortcutsEnabled);
    QVERIFY(projection.collectionReadingViewerShortcutsEnabled);
    QVERIFY(projection.pannableShortcutsEnabled);
    QVERIFY(projection.pannableViewerShortcutsEnabled);
    QVERIFY(projection.containerShortcutsEnabled);
    QVERIFY(projection.containerViewerShortcutsEnabled);
    QVERIFY(!projection.transformShortcutsEnabled);
    QVERIFY(!projection.transformViewerShortcutsEnabled);

    input.textInputFocused = true;
    projection = imageActionAvailabilityProjection(input);

    QVERIFY(!projection.viewerShortcutsEnabled);
    QVERIFY(projection.readyShortcutsEnabled);
    QVERIFY(!projection.readyViewerShortcutsEnabled);
    QVERIFY(!projection.collectionReadingViewerShortcutsEnabled);
    QVERIFY(!projection.pannableViewerShortcutsEnabled);
    QVERIFY(!projection.containerViewerShortcutsEnabled);

    input.fileDeletionInProgress = true;
    projection = imageActionAvailabilityProjection(input);

    QVERIFY(!projection.readyShortcutsEnabled);
    QVERIFY(!projection.collectionReadingShortcutsEnabled);
    QVERIFY(!projection.pannableShortcutsEnabled);
    QVERIFY(!projection.containerShortcutsEnabled);
}

void TestImageActionAvailability::policyScopeLookupUsesApplicationScope()
{
    using Scope = kiriview::ApplicationActions::ImageShortcutScope;

    ImageActionAvailabilityProjection projection;
    projection.helpShortcutsEnabled = true;
    projection.readyViewerShortcutsEnabled = true;
    projection.collectionReadingViewerShortcutsEnabled = true;
    projection.transformViewerShortcutsEnabled = true;
    projection.pannableViewerShortcutsEnabled = true;
    projection.containerShortcutsEnabled = true;

    QVERIFY(imageActionAvailabilityShortcutsEnabledForScope(projection, Scope::HelpShortcutScope));
    QVERIFY(imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::ReadyViewerShortcutScope));
    QVERIFY(imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::CollectionReadingViewerShortcutScope));
    QVERIFY(imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::TransformViewerShortcutScope));
    QVERIFY(
        imageActionAvailabilityShortcutsEnabledForScope(projection, Scope::ContainerShortcutScope));
    QVERIFY(imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::MediaStartEndViewerShortcutScope));
    QVERIFY(!imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::ImageSelectionShortcutScope));
    QVERIFY(!imageActionAvailabilityShortcutsEnabledForScope(
        projection, Scope::PageViewerShortcutScope));
    QVERIFY(
        !imageActionAvailabilityShortcutsEnabledForScope(projection, Scope::ViewerShortcutScope));
}

void TestImageActionAvailability::activeImageDocumentSourceScopeLookupUsesSessionNavigationInput()
{
    using Scope = kiriview::ApplicationActions::ImageShortcutScope;

    ImageActionAvailabilityProjection projection;
    projection.helpShortcutsEnabled = true;
    projection.viewerShortcutsEnabled = true;
    projection.readyViewerShortcutsEnabled = true;

    ActiveMediaShortcutAvailabilityInput input;
    input.imageProjection = projection;
    input.activeNavigationActionsAvailable = true;

    QVERIFY(activeMediaShortcutsEnabledForScope(input, Scope::ImageSelectionShortcutScope));
    QVERIFY(activeMediaShortcutsEnabledForScope(input, Scope::PageViewerShortcutScope));

    input.activeNavigationActionsAvailable = false;
    QVERIFY(!activeMediaShortcutsEnabledForScope(input, Scope::ImageSelectionShortcutScope));
    QVERIFY(!activeMediaShortcutsEnabledForScope(input, Scope::PageViewerShortcutScope));

    input.videoMode = true;
    input.videoFileDeletionInProgress = false;
    QVERIFY(activeMediaShortcutsEnabledForScope(input, Scope::MediaStartEndViewerShortcutScope));

    input.activeNavigationActionsAvailable = true;
    input.videoFileDeletionInProgress = true;
    QVERIFY(activeMediaShortcutsEnabledForScope(input, Scope::PageShortcutScope));
    QVERIFY(!activeMediaShortcutsEnabledForScope(input, Scope::MediaStartEndViewerShortcutScope));
    QVERIFY(!activeMediaShortcutsEnabledForScope(input, Scope::ReadyViewerShortcutScope));

    QVERIFY(!activeMediaShortcutsEnabledForScope(input, static_cast<Scope>(999)));
}

QTEST_GUILESS_MAIN(TestImageActionAvailability)

#include "tst_imageactionavailability.moc"
