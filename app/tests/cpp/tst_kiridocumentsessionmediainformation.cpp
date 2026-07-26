// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "kiridocumentsession_test_support.h"

class TestKiriDocumentSessionMediaInformation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptySessionProjectsUnavailableMediaInformation();
    void imageMediaInformationUsesEmbeddedMetadataAndKnownDimensions();
    void imageMediaInformationClearsStaleEmbeddedMetadataOnReplacement();
    void videoMediaInformationUsesVideoSectionAndNoCameraRows();
    void mediaInformationDerivesFilenameAndPathFromTargetUrl();
    void mediaInformationRowModelsExposeLabelAndValueRoles();
};

#include "kiridocumentsession_media_information.inc"

QTEST_MAIN(TestKiriDocumentSessionMediaInformation)

#include "tst_kiridocumentsessionmediainformation.moc"
