// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import org.hnjae.kiriview

QtObject {
    id: root

    required property var provider
    required property bool collectionControlsVisibleFallback
    required property int fitActionId
    required property bool fitEnabledFallback
    required property int rightToLeftActionId
    required property bool rightToLeftCheckedFallback
    required property bool rightToLeftEnabledFallback
    required property int twoPageActionId
    required property bool twoPageCheckedFallback
    required property bool twoPageEnabledFallback
    required property bool zoomEditableFallback
    required property int zoomMaximumManualPercentFallback
    required property int zoomMinimumManualPercentFallback
    required property bool zoomPercentAvailableFallback
    required property bool zoomPercentKnownFallback
    required property real zoomPercentFallback

    readonly property int projectionRevision: provider?.actionStateRevision ?? 0
    readonly property int phase: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarPresentationPhase === "function") {
            return provider.imageToolbarPresentationPhase();
        }
        return zoomEditableFallback ? KiriViewApplication.ImageToolbarPresentationCurrent : KiriViewApplication.ImageToolbarPresentationUnavailable;
    }
    readonly property bool collectionControlsVisible: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarCollectionControlsVisible === "function") {
            return provider.imageToolbarCollectionControlsVisible();
        }
        return collectionControlsVisibleFallback;
    }
    readonly property bool rightToLeftAppearanceEnabled: actionAppearanceEnabled(rightToLeftActionId, rightToLeftEnabledFallback)
    readonly property bool rightToLeftAppearanceChecked: actionAppearanceChecked(rightToLeftActionId, rightToLeftCheckedFallback)
    readonly property bool rightToLeftInteractionEnabled: actionInteractionEnabled(rightToLeftActionId, rightToLeftEnabledFallback)
    readonly property bool twoPageAppearanceEnabled: actionAppearanceEnabled(twoPageActionId, twoPageEnabledFallback)
    readonly property bool twoPageAppearanceChecked: actionAppearanceChecked(twoPageActionId, twoPageCheckedFallback)
    readonly property bool twoPageInteractionEnabled: actionInteractionEnabled(twoPageActionId, twoPageEnabledFallback)
    readonly property int presentedFitActionId: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarPresentedFitActionId === "function") {
            return provider.imageToolbarPresentedFitActionId();
        }
        return fitActionId;
    }
    readonly property bool fitAppearanceEnabled: actionAppearanceEnabled(presentedFitActionId, fitEnabledFallback)
    readonly property bool fitInteractionEnabled: actionInteractionEnabled(presentedFitActionId, fitEnabledFallback)
    readonly property bool presentedImageReady: phase !== KiriViewApplication.ImageToolbarPresentationUnavailable
    readonly property bool zoomAppearanceEnabled: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomAppearanceEnabled === "function") {
            return provider.imageToolbarZoomAppearanceEnabled();
        }
        return zoomEditableFallback;
    }
    readonly property bool zoomInteractionEnabled: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomInteractionEnabled === "function") {
            return provider.imageToolbarZoomInteractionEnabled();
        }
        return zoomEditableFallback;
    }
    readonly property bool presentedZoomEditable: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomPercentEditable === "function") {
            return provider.imageToolbarZoomPercentEditable();
        }
        return zoomEditableFallback;
    }
    readonly property bool presentedZoomPercentAvailable: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomPercentAvailable === "function") {
            return provider.imageToolbarZoomPercentAvailable();
        }
        return zoomPercentAvailableFallback;
    }
    readonly property bool presentedZoomPercentKnown: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomPercentKnown === "function") {
            return provider.imageToolbarZoomPercentKnown();
        }
        return zoomPercentKnownFallback;
    }
    readonly property real presentedZoomPercent: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomPercent === "function") {
            return provider.imageToolbarZoomPercent();
        }
        return zoomPercentFallback;
    }
    readonly property int presentedZoomMinimumManualPercent: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomMinimumManualPercent === "function") {
            return provider.imageToolbarZoomMinimumManualPercent();
        }
        return zoomMinimumManualPercentFallback;
    }
    readonly property int presentedZoomMaximumManualPercent: {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarZoomMaximumManualPercent === "function") {
            return provider.imageToolbarZoomMaximumManualPercent();
        }
        return zoomMaximumManualPercentFallback;
    }

    function actionAppearanceEnabled(actionId, fallback) {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarActionAppearanceEnabled === "function") {
            return provider.imageToolbarActionAppearanceEnabled(actionId);
        }
        return fallback;
    }

    function actionAppearanceChecked(actionId, fallback) {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarActionAppearanceChecked === "function") {
            return provider.imageToolbarActionAppearanceChecked(actionId);
        }
        return fallback;
    }

    function actionInteractionEnabled(actionId, fallback) {
        projectionRevision;
        if (provider !== null && provider !== undefined && typeof provider.imageToolbarActionInteractionEnabled === "function") {
            return provider.imageToolbarActionInteractionEnabled(actionId);
        }
        return fallback;
    }
}
