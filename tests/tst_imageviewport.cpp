#include "imageviewport.h"

#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

class ImageViewportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructsAsQuickItem();
};

void ImageViewportTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QCOMPARE(item.source(), QUrl());
    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
