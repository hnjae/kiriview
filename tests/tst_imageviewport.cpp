#include "imageviewport.h"

#include <QtQuick/QQuickItem>
#include <QtTest/QTest>

class ImageViewportTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructsAsQuickItem();
    void doesNotExposeSourceProperty();
};

void ImageViewportTest::defaultConstructsAsQuickItem()
{
    ImageViewport item;

    QVERIFY(item.flags().testFlag(QQuickItem::ItemHasContents));
}

void ImageViewportTest::doesNotExposeSourceProperty()
{
    ImageViewport item;

    QCOMPARE(item.metaObject()->indexOfProperty("source"), -1);
}

QTEST_MAIN(ImageViewportTest)

#include "tst_imageviewport.moc"
