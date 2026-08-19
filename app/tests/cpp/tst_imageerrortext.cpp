// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "localization/imageerrortext.h"

#include "decoding/decodedimagefailure.h"

#include <QObject>
#include <QString>
#include <QTest>
#include <array>

class TestImageErrorText : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void plainTextIdsExposeCanonicalMessages();
    void decodedFailuresMapTypedFactsToStableApplicationCopy();
};

void TestImageErrorText::plainTextIdsExposeCanonicalMessages()
{
    QCOMPARE(kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData),
        QStringLiteral("Could not read the selected image data."));
    QCOMPARE(kiriview::imageErrorText(kiriview::ImageErrorTextId::OpenVideo),
        QStringLiteral("Could not open the selected video."));
}

void TestImageErrorText::decodedFailuresMapTypedFactsToStableApplicationCopy()
{
    struct Case
    {
        kiriview::DecodedImageFailureRoute route;
        kiriview::DecodedImageFailureOperation operation;
        kiriview::DecodedImageFailureCause cause;
        kiriview::ImageErrorTextId expectedText;
    };
    using Route = kiriview::DecodedImageFailureRoute;
    using Operation = kiriview::DecodedImageFailureOperation;
    using Cause = kiriview::DecodedImageFailureCause;
    using Text = kiriview::ImageErrorTextId;
    const std::array cases {
        Case { Route::Raw, Operation::DecodeRawImage, Cause::ResourceLimitExceeded,
            Text::DecodeImageResourceLimitExceeded },
        Case { Route::Apng, Operation::DecodeAnimationOpen, Cause::Unknown,
            Text::DecodeApngAnimation },
        Case { Route::HeifFamily, Operation::DecodeHeifSequenceOpen, Cause::Unknown,
            Text::DecodeHeifSequence },
        Case { Route::HeifFamily, Operation::DecodeHeifSequenceFrame, Cause::Unknown,
            Text::DecodeHeifSequence },
        Case { Route::QtRaster, Operation::DecodeAnimationOpen, Cause::Unknown,
            Text::DecodeImageAnimation },
        Case { Route::Svg, Operation::OpenStaticImageSource, Cause::Unknown, Text::DecodeSvgImage },
        Case { Route::HeifFamily, Operation::OpenStaticImageSource, Cause::Unknown,
            Text::DecodeHeifImage },
        Case { Route::Raw, Operation::DecodeRawImage, Cause::Unknown, Text::DecodeRawImage },
        Case {
            Route::QtRaster, Operation::OpenStaticImageSource, Cause::Unknown, Text::DecodeImage },
        Case { Route::Unknown, Operation::Unknown, Cause::Unknown, Text::DecodeImage },
    };

    const QString diagnosticMarker = QStringLiteral("backend diagnostic marker");
    for (const Case& testCase : cases) {
        const kiriview::DecodedImageFailure failure {
            testCase.route,
            testCase.operation,
            diagnosticMarker,
            kiriview::DecodedImageFailureSeverity::Error,
            true,
            testCase.cause,
        };
        const QString userMessage = kiriview::decodedImageFailureText(failure);
        QCOMPARE(userMessage, kiriview::imageErrorText(testCase.expectedText));
        QVERIFY(userMessage != diagnosticMarker);
    }
}

QTEST_GUILESS_MAIN(TestImageErrorText)

#include "tst_imageerrortext.moc"
