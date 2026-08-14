// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imageloadtypes.h"
#include "location/documentportalpathvalidation_p.h"
#include "location/imagelocation.h"
#include "location/imageurl.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <linux/magic.h>
#include <optional>
#include <sys/xattr.h>

namespace {
QUrl archiveUrl(const QString& scheme, const QString& path)
{
    QUrl url;
    url.setScheme(scheme);
    url.setPath(path);
    return url;
}

bool writeEmptyFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}
}

class TestImageUrl : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void userVisibleFileNameDecodesFilenameComponentExactlyOnce();
    void normalizedContainerUrlsStripQueryFragmentsAndCleanLocalPaths();
    void normalizedUrlIdentityHelpersRejectInvalidImageUrlsAndPreserveKeyFormatting();
    void directoryNavigationHelpersOwnParentAndIdentityRules();
    void normalizedUrlIdentityComparisonHandlesEmptyAndPathEquivalentUrls();
    void sameNormalizedUrlMatchesPathSegments();
    void parentUrlForContainerNavigationHandlesContainers();
    void documentPortalHostPathValuesRequireAbsoluteCleanPaths();
    void documentPortalCandidatesRequireFilesystemAbsoluteRoots();
    void documentPortalMountAuthenticationRequiresFuseMountBoundary();
    void trustedNavigationSourceFactsResolveDocumentPortalHostWithoutXattr();
    void navigationSourceFactsRequireConfirmedKioFuseArchiveMapping();
    void resolvedNavigationSourceCollectsFactsOncePerSnapshot();
    void resolvedNavigationSourceRetainsNegativeFactsUntilReplacement();
    void injectedEmptyProviderDoesNotUseDefaultAdapter();
    void ordinaryFileDocumentPortalHostPathAttributeIsIgnored();
    void fileWithoutDocumentPortalXattrProducesNegativeFacts();
    void kioFuseShapedLocalPathsRemainLocalWithoutServiceConfirmation();
    void imageLocationTypesExposeExplicitState();
};

void TestImageUrl::userVisibleFileNameDecodesFilenameComponentExactlyOnce()
{
    QCOMPARE(kiriview::userVisibleFileNameForUrl(
                 QUrl::fromLocalFile(QStringLiteral("/media/[clip].mp4"))),
        QStringLiteral("[clip].mp4"));
    QCOMPARE(kiriview::userVisibleFileNameForUrl(
                 QUrl::fromLocalFile(QStringLiteral("/media/%5Bclip%5D.mp4"))),
        QStringLiteral("%5Bclip%5D.mp4"));
    QCOMPARE(kiriview::userVisibleFileNameForUrl(QUrl()), QString());
}

void TestImageUrl::normalizedContainerUrlsStripQueryFragmentsAndCleanLocalPaths()
{
    QUrl fileUrl = QUrl::fromLocalFile(QStringLiteral("/images/./chapter/../page.png"));
    fileUrl.setQuery(QStringLiteral("cache=1"));
    fileUrl.setFragment(QStringLiteral("view"));
    QCOMPARE(kiriview::normalizedFileContainerUrl(fileUrl),
        QUrl::fromLocalFile(QStringLiteral("/images/page.png")));

    QUrl directoryUrl = QUrl::fromLocalFile(QStringLiteral("/images/chapter"));
    directoryUrl.setQuery(QStringLiteral("cache=1"));
    directoryUrl.setFragment(QStringLiteral("view"));
    QCOMPARE(kiriview::normalizedDirectoryContainerUrl(directoryUrl),
        QUrl::fromLocalFile(QStringLiteral("/images/chapter/")));
}

void TestImageUrl::normalizedUrlIdentityHelpersRejectInvalidImageUrlsAndPreserveKeyFormatting()
{
    const QUrl normalizedUrl = QUrl::fromLocalFile(QStringLiteral("/images/page 1.png"));
    const QUrl equivalentUrl = QUrl::fromLocalFile(QStringLiteral("/images/chapter/../page 1.png"));

    QCOMPARE(kiriview::normalizedUrlForIdentity(equivalentUrl), normalizedUrl);
    const std::optional<QUrl> validImageUrl = kiriview::normalizedValidImageUrl(equivalentUrl);
    QVERIFY(validImageUrl.has_value());
    QCOMPARE(*validImageUrl, normalizedUrl);
    QVERIFY(!kiriview::normalizedValidImageUrl(QUrl()).has_value());
    QCOMPARE(kiriview::normalizedUrlIdentityKey(equivalentUrl),
        normalizedUrl.toString(QUrl::PrettyDecoded));
    QCOMPARE(kiriview::normalizedUrlIdentityKey(equivalentUrl, QUrl::FullyEncoded),
        normalizedUrl.toString(QUrl::FullyEncoded));
}

void TestImageUrl::directoryNavigationHelpersOwnParentAndIdentityRules()
{
    const QUrl directoryUrl = QUrl::fromLocalFile(QStringLiteral("/images/chapter/../"));
    const QUrl normalizedDirectoryUrl = QUrl::fromLocalFile(QStringLiteral("/images/"));
    QCOMPARE(kiriview::normalizedDirectoryUrlForIdentity(directoryUrl), normalizedDirectoryUrl);
    QCOMPARE(kiriview::directoryUrlIdentityKey(directoryUrl),
        normalizedDirectoryUrl.toString(QUrl::FullyEncoded));

    QCOMPARE(kiriview::parentDirectoryUrlForFileNavigation(
                 QUrl::fromLocalFile(QStringLiteral("/images/a/../b/page.png"))),
        QUrl::fromLocalFile(QStringLiteral("/images/b/")));

    const QUrl archiveEntry(QStringLiteral("zip:///path/archive.zip!/chapter/page.png"));
    QCOMPARE(kiriview::parentDirectoryUrlForFileNavigation(archiveEntry),
        QUrl(QStringLiteral("zip:///path/archive.zip!/chapter/")));

    const QUrl requestedUrl = QUrl::fromLocalFile(QStringLiteral("/images/a/../b/page.png"));
    const kiriview::DirectoryNavigationLocation navigationLocation
        = kiriview::directoryNavigationLocationForSource(
            kiriview::ResolvedNavigationSource(requestedUrl, {}, requestedUrl));
    QVERIFY(navigationLocation.isValid());
    QVERIFY(kiriview::sameNormalizedUrl(
        navigationLocation.fileUrl, QUrl::fromLocalFile(QStringLiteral("/images/b/page.png"))));
    QCOMPARE(navigationLocation.directoryUrl, QUrl::fromLocalFile(QStringLiteral("/images/b/")));
}

void TestImageUrl::normalizedUrlIdentityComparisonHandlesEmptyAndPathEquivalentUrls()
{
    const QUrl rawUrl = QUrl::fromLocalFile(QStringLiteral("/images/chapter/../page.png"));
    const QUrl normalizedUrl = QUrl::fromLocalFile(QStringLiteral("/images/page.png"));

    const std::optional<QUrl> validUrl = kiriview::normalizedValidUrlForIdentity(rawUrl);
    QVERIFY(validUrl.has_value());
    QCOMPARE(*validUrl, normalizedUrl);
    QVERIFY(!kiriview::normalizedValidUrlForIdentity(QUrl()).has_value());
    QVERIFY(kiriview::sameNormalizedUrl(rawUrl, normalizedUrl));
}

void TestImageUrl::sameNormalizedUrlMatchesPathSegments()
{
    QVERIFY(kiriview::sameNormalizedUrl(
        QUrl::fromLocalFile(QStringLiteral("/images/chapter/../page.png")),
        QUrl::fromLocalFile(QStringLiteral("/images/page.png"))));
    QVERIFY(!kiriview::sameNormalizedUrl(QUrl::fromLocalFile(QStringLiteral("/images/page.png")),
        QUrl::fromLocalFile(QStringLiteral("/images/other.png"))));
}

void TestImageUrl::parentUrlForContainerNavigationHandlesContainers()
{
    QCOMPARE(
        kiriview::parentUrlForContainerNavigation(QUrl::fromLocalFile(QStringLiteral("/images/"))),
        QUrl::fromLocalFile(QStringLiteral("/")));
    const QUrl archiveUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz"));
    QCOMPARE(kiriview::parentUrlForContainerNavigation(archiveUrl),
        QUrl::fromLocalFile(QStringLiteral("/books/")));
}

void TestImageUrl::documentPortalHostPathValuesRequireAbsoluteCleanPaths()
{
    const QString requestedPath = QStringLiteral("/run/flatpak/doc/item/02.mp4");
    const QString hostPath = QStringLiteral("/media/videos/02.mp4");
    const QByteArray encodedHostPath = QFile::encodeName(hostPath);

    const std::optional<QString> validatedHostPath
        = kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
            encodedHostPath, requestedPath);
    QVERIFY(validatedHostPath.has_value());
    QCOMPARE(*validatedHostPath, hostPath);

    QByteArray terminatedHostPath = encodedHostPath;
    terminatedHostPath.append('\0');
    const std::optional<QString> validatedTerminatedHostPath
        = kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
            terminatedHostPath, requestedPath);
    QVERIFY(validatedTerminatedHostPath.has_value());
    QCOMPARE(*validatedTerminatedHostPath, hostPath);

    QByteArray embeddedNullHostPath = encodedHostPath;
    embeddedNullHostPath.insert(7, '\0');
    QVERIFY(!kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
        embeddedNullHostPath, requestedPath)
            .has_value());
    QVERIFY(!kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
        QByteArray("media/videos/02.mp4"), requestedPath)
            .has_value());
    QVERIFY(!kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
        QByteArray(":/media/videos/02.mp4"), requestedPath)
            .has_value());
    QVERIFY(!kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
        QByteArray("/media/videos/../02.mp4"), requestedPath)
            .has_value());
    QVERIFY(!kiriview::NavigationSourceDetail::validatedDocumentPortalHostPath(
        QFile::encodeName(requestedPath), requestedPath)
            .has_value());
}

void TestImageUrl::documentPortalCandidatesRequireFilesystemAbsoluteRoots()
{
    QVERIFY(kiriview::NavigationSourceDetail::isDocumentPortalPathCandidate(
        QStringLiteral("/run/user/1000/doc/item/02.mp4"), QStringLiteral("/run/user/1000")));
    QVERIFY(kiriview::NavigationSourceDetail::isDocumentPortalPathCandidate(
        QStringLiteral("/run/flatpak/doc/item/02.mp4"), QString()));
    QVERIFY(!kiriview::NavigationSourceDetail::isDocumentPortalPathCandidate(
        QStringLiteral(":/run/user/1000/doc/item/02.mp4"), QStringLiteral("/run/user/1000")));
    QVERIFY(!kiriview::NavigationSourceDetail::isDocumentPortalPathCandidate(
        QStringLiteral(":/runtime/doc/item/02.mp4"), QStringLiteral(":/runtime")));
    QVERIFY(!kiriview::NavigationSourceDetail::isDocumentPortalPathCandidate(
        QStringLiteral("/runtime/doc/item/02.mp4"), QStringLiteral(":/runtime")));
}

void TestImageUrl::documentPortalMountAuthenticationRequiresFuseMountBoundary()
{
    using kiriview::NavigationSourceDetail::DocumentPortalMountFacts;
    using kiriview::NavigationSourceDetail::isAuthenticatedDocumentPortalMount;

    const DocumentPortalMountFacts authenticated {
        FUSE_SUPER_MAGIC,
        FUSE_SUPER_MAGIC,
        11,
        11,
        7,
    };
    QVERIFY(isAuthenticatedDocumentPortalMount(authenticated));

    DocumentPortalMountFacts changed = authenticated;
    changed.entryFileSystemType = 0;
    QVERIFY(!isAuthenticatedDocumentPortalMount(changed));
    changed = authenticated;
    changed.rootFileSystemType = 0;
    QVERIFY(!isAuthenticatedDocumentPortalMount(changed));
    changed = authenticated;
    changed.rootDevice = 12;
    QVERIFY(!isAuthenticatedDocumentPortalMount(changed));
    changed = authenticated;
    changed.parentDevice = changed.rootDevice;
    QVERIFY(!isAuthenticatedDocumentPortalMount(changed));
}

void TestImageUrl::trustedNavigationSourceFactsResolveDocumentPortalHostWithoutXattr()
{
    const QUrl portalUrl = QUrl::fromLocalFile(QStringLiteral("/run/user/1000/doc/02.mp4"));
    const QUrl hostUrl = QUrl::fromLocalFile(QStringLiteral("/media/videos/02.mp4"));

    kiriview::NavigationSourceEntryFacts facts;
    facts.documentPortalHostPath = hostUrl.toLocalFile();
    QCOMPARE(kiriview::resolvedNavigationSource(portalUrl, facts).navigationUrl(), hostUrl);

    facts.documentPortalHostPath = portalUrl.toLocalFile();
    QCOMPARE(kiriview::resolvedNavigationSource(portalUrl, facts).navigationUrl(), portalUrl);
}

void TestImageUrl::navigationSourceFactsRequireConfirmedKioFuseArchiveMapping()
{
    kiriview::NavigationSourceEntryFacts facts;
    facts.runtimeDir = QStringLiteral("/run/user/1000");

    const QString cbzFusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/zip/books/book.cbz/page.png");
    const QString cbtFusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/tar/books/book.cbt/page.png");
    const QString cb7FusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/sevenz/books/book.cb7/page.png");

    const QUrl cbzLocalUrl = QUrl::fromLocalFile(cbzFusePath);
    const QUrl cbtLocalUrl = QUrl::fromLocalFile(cbtFusePath);
    const QUrl cb7LocalUrl = QUrl::fromLocalFile(cb7FusePath);
    const kiriview::ResolvedNavigationSource unconfirmed
        = kiriview::resolvedNavigationSource(cbzLocalUrl, facts);
    QCOMPARE(unconfirmed.requestedUrl(), cbzLocalUrl);
    QCOMPARE(unconfirmed.navigationUrl(), cbzLocalUrl);
    QCOMPARE(kiriview::directoryNavigationLocationForSource(unconfirmed).directoryUrl,
        QUrl::fromLocalFile(QStringLiteral("/run/user/1000/kio-fuse-test/zip/books/book.cbz/")));
    QCOMPARE(kiriview::resolvedNavigationSource(cbtLocalUrl, facts).navigationUrl(), cbtLocalUrl);
    QCOMPARE(kiriview::resolvedNavigationSource(cb7LocalUrl, facts).navigationUrl(), cb7LocalUrl);

    const QUrl confirmedArchiveUrl
        = archiveUrl(QStringLiteral("zip"), QStringLiteral("/books/book.cbz/page.png"));
    facts.kioFuseArchiveUrl = confirmedArchiveUrl;
    const kiriview::ResolvedNavigationSource confirmed
        = kiriview::resolvedNavigationSource(cbzLocalUrl, facts);
    QCOMPARE(confirmed.requestedUrl(), cbzLocalUrl);
    QCOMPARE(confirmed.navigationUrl(), confirmedArchiveUrl);
    QCOMPARE(kiriview::directoryNavigationLocationForSource(confirmed).directoryUrl,
        archiveUrl(QStringLiteral("zip"), QStringLiteral("/books/book.cbz/")));

    facts.kioFuseArchiveUrl = QUrl(QStringLiteral("https://example.test/books/book.cbz/page.png"));
    QCOMPARE(kiriview::resolvedNavigationSource(cbzLocalUrl, facts).navigationUrl(), cbzLocalUrl);
}

void TestImageUrl::resolvedNavigationSourceCollectsFactsOncePerSnapshot()
{
    const QUrl portalUrl = QUrl::fromLocalFile(QStringLiteral("/run/user/1000/doc/album"));
    const QUrl hostUrl = QUrl::fromLocalFile(QStringLiteral("/books/album"));
    int probeCount = 0;
    bool isDirectory = true;
    const kiriview::NavigationSourceEntryFactProvider provider
        = [&probeCount, &hostUrl, &isDirectory](const QUrl&) {
              ++probeCount;
              return kiriview::NavigationSourceEntryFacts {
                  hostUrl.toLocalFile(),
                  QStringLiteral("/run/user/1000"),
                  isDirectory,
              };
          };

    const kiriview::ResolvedNavigationSource source
        = kiriview::NavigationSourceResolver(provider).resolveExternalSource(portalUrl);
    QCOMPARE(probeCount, 1);
    QCOMPARE(source.requestedUrl(), portalUrl);
    QCOMPARE(source.navigationUrl(), hostUrl);
    QCOMPARE(source.entryKind(), kiriview::NavigationSourceEntryKind::Directory);
    QCOMPARE(kiriview::directoryNavigationLocationForSource(source).fileUrl, hostUrl);
    QCOMPARE(kiriview::directoryNavigationLocationForSource(source).fileUrl, hostUrl);
    QCOMPARE(probeCount, 1);

    isDirectory = false;
    QCOMPARE(source.entryKind(), kiriview::NavigationSourceEntryKind::Directory);
    QCOMPARE(probeCount, 1);

    const kiriview::ResolvedNavigationSource replacement
        = kiriview::NavigationSourceResolver(provider).resolveExternalSource(portalUrl);
    QCOMPARE(replacement.entryKind(), kiriview::NavigationSourceEntryKind::Direct);
    QCOMPARE(probeCount, 2);
}

void TestImageUrl::resolvedNavigationSourceRetainsNegativeFactsUntilReplacement()
{
    const QUrl sourceUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz"));
    int probeCount = 0;
    const kiriview::NavigationSourceEntryFactProvider provider = [&probeCount](const QUrl&) {
        ++probeCount;
        return kiriview::NavigationSourceEntryFacts {};
    };

    const kiriview::ResolvedNavigationSource first
        = kiriview::NavigationSourceResolver(provider).resolveExternalSource(sourceUrl);
    QVERIFY(!first.facts().documentPortalHostPath.has_value());
    QVERIFY(!first.facts().kioFuseArchiveUrl.has_value());
    QCOMPARE(kiriview::directoryNavigationLocationForSource(first).fileUrl, sourceUrl);
    QCOMPARE(kiriview::directoryNavigationLocationForSource(first).fileUrl, sourceUrl);
    QCOMPARE(probeCount, 1);

    const kiriview::ResolvedNavigationSource replacement
        = kiriview::NavigationSourceResolver(provider).resolveExternalSource(sourceUrl);
    QCOMPARE(replacement.navigationUrl(), sourceUrl);
    QCOMPARE(probeCount, 2);
}

void TestImageUrl::injectedEmptyProviderDoesNotUseDefaultAdapter()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString portalPath = directory.filePath(QStringLiteral("portal.png"));
    const QString hostPath = directory.filePath(QStringLiteral("host.png"));
    QVERIFY(writeEmptyFile(portalPath));
    QVERIFY(writeEmptyFile(hostPath));

    const QByteArray encodedPortalPath = QFile::encodeName(portalPath);
    const QByteArray encodedHostPath = QFile::encodeName(hostPath);
    if (setxattr(encodedPortalPath.constData(), "user.document-portal.host-path",
            encodedHostPath.constData(), static_cast<std::size_t>(encodedHostPath.size()), 0)
        != 0) {
        QSKIP("Filesystem does not support user xattrs");
    }

    const QUrl portalUrl = QUrl::fromLocalFile(portalPath);
    const kiriview::NavigationSourceResolver resolver(
        kiriview::NavigationSourceEntryFactProvider {});

    QCOMPARE(resolver.resolveExternalSource(portalUrl).navigationUrl(), portalUrl);
}

void TestImageUrl::ordinaryFileDocumentPortalHostPathAttributeIsIgnored()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("portal"))));
    QVERIFY(QDir().mkpath(directory.filePath(QStringLiteral("host"))));

    const QString portalPath = directory.filePath(QStringLiteral("portal/02.mp4"));
    const QString hostPath = directory.filePath(QStringLiteral("host/02.mp4"));
    QVERIFY(writeEmptyFile(portalPath));
    QVERIFY(writeEmptyFile(hostPath));

    const QByteArray encodedPortalPath = QFile::encodeName(portalPath);
    const QByteArray encodedHostPath = QFile::encodeName(hostPath);
    const int result = setxattr(encodedPortalPath.constData(), "user.document-portal.host-path",
        encodedHostPath.constData(), static_cast<std::size_t>(encodedHostPath.size()), 0);
    if (result != 0) {
        QSKIP("extended attributes unavailable");
    }

    const QUrl portalUrl = QUrl::fromLocalFile(portalPath);
    const kiriview::ResolvedNavigationSource source
        = kiriview::NavigationSourceResolver {}.resolveExternalSource(portalUrl);
    QVERIFY(!source.facts().documentPortalHostPath.has_value());
    QCOMPARE(source.navigationUrl(), portalUrl);

    const kiriview::DirectoryNavigationLocation navigationLocation
        = kiriview::directoryNavigationLocationForSource(source);
    QCOMPARE(navigationLocation.fileUrl, portalUrl);
    QCOMPARE(navigationLocation.directoryUrl,
        QUrl::fromLocalFile(directory.filePath(QStringLiteral("portal/"))));
}

void TestImageUrl::fileWithoutDocumentPortalXattrProducesNegativeFacts()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("plain-image.png"));
    QVERIFY(writeEmptyFile(path));

    const kiriview::ResolvedNavigationSource source
        = kiriview::NavigationSourceResolver {}.resolveExternalSource(QUrl::fromLocalFile(path));

    QVERIFY(!source.facts().documentPortalHostPath.has_value());
    QCOMPARE(source.navigationUrl(), QUrl::fromLocalFile(path));
}

void TestImageUrl::kioFuseShapedLocalPathsRemainLocalWithoutServiceConfirmation()
{
    const bool hadRuntimeDir = qEnvironmentVariableIsSet("XDG_RUNTIME_DIR");
    const QByteArray originalRuntimeDir = qgetenv("XDG_RUNTIME_DIR");
    qputenv("XDG_RUNTIME_DIR", "/run/user/1000");

    const QString cbzFusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/zip/books/book.cbz/page.png");
    const QString cbtFusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/tar/books/book.cbt/page.png");
    const QString cb7FusePath
        = QStringLiteral("/run/user/1000/kio-fuse-test/sevenz/books/book.cb7/page.png");

    for (const QString& localPath : { cbzFusePath, cbtFusePath, cb7FusePath }) {
        const QUrl localUrl = QUrl::fromLocalFile(localPath);
        QCOMPARE(
            kiriview::NavigationSourceResolver {}.resolveExternalSource(localUrl).navigationUrl(),
            localUrl);
    }

    qunsetenv("XDG_RUNTIME_DIR");
    const QUrl unscopedLookalike
        = QUrl::fromLocalFile(QStringLiteral("/tmp/kio-fuse-test/zip/books/book.cbz/page.png"));
    QCOMPARE(kiriview::NavigationSourceResolver {}
                 .resolveExternalSource(unscopedLookalike)
                 .navigationUrl(),
        unscopedLookalike);

    if (hadRuntimeDir) {
        qputenv("XDG_RUNTIME_DIR", originalRuntimeDir);
    } else {
        qunsetenv("XDG_RUNTIME_DIR");
    }
}

void TestImageUrl::imageLocationTypesExposeExplicitState()
{
    const kiriview::DisplayedImageLocation emptyLocation;
    QVERIFY(emptyLocation.isEmpty());

    const kiriview::OpenedCollectionScopeLocation archiveCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            QUrl::fromLocalFile(QStringLiteral("/books/book.cbz")),
            QUrl(QStringLiteral("zip:///books/book.cbz/")),
            kiriview::OpenedCollectionScopeKind::ComicBookArchive);
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            QUrl(QStringLiteral("zip:///books/book.cbz/page.png")), archiveCollection);
    QVERIFY(!location.isEmpty());
    QCOMPARE(location.openedCollectionScopeSourceUrl(), archiveCollection.fileUrl());
    QCOMPARE(location.openedCollectionScope().rootUrl(), archiveCollection.rootUrl());

    const kiriview::ImageLoadRequest plainOpen = kiriview::ImageLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(location.imageUrl(), {}));
    QVERIFY(!plainOpen.isEmpty());
    QCOMPARE(plainOpen.sourceUrl(), location.imageUrl());
    QVERIFY(plainOpen.openedCollectionScope().rootUrl().isEmpty());

    const QUrl containerUrl = archiveCollection.fileUrl();
    const kiriview::ImageLoadRequest containerOpen
        = kiriview::ImageLoadRequest::fromContainerTarget(
            kiriview::ImageDocumentPageTarget(
                location.imageUrl(), kiriview::ImageDocumentPageKind::Image),
            location.openedCollectionScope());
    QCOMPARE(containerOpen.openedCollectionScope().rootUrl(),
        location.openedCollectionScope().rootUrl());
    QCOMPARE(containerOpen.containerNavigationUrl(), containerUrl);
}

QTEST_GUILESS_MAIN(TestImageUrl)

#include "tst_imageurl.moc"
