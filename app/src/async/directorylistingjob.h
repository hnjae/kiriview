// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DIRECTORYLISTINGJOB_H
#define KIRIVIEW_DIRECTORYLISTINGJOB_H

#include "async/imageiojob.h"
#include "system/kiooperationfailure.h"

#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <functional>
#include <optional>
#include <vector>

class QObject;

namespace kiriview {
struct DirectoryItem
{
    QUrl url;
    QString name;
    bool isFile = false;
    std::optional<qint64> byteSize;
    std::optional<qint64> modificationTimeSeconds;
};

using DirectoryItemList = std::vector<DirectoryItem>;

struct SiblingCandidateAdmissionLimits
{
    qsizetype maximumEntryCount = 0;
    qsizetype maximumIdentityCodeUnitCount = 0;
};

class DirectoryItemListAdmission final
{
public:
    explicit DirectoryItemListAdmission(SiblingCandidateAdmissionLimits limits);

    [[nodiscard]] bool admit(DirectoryItem item);
    [[nodiscard]] bool rejected() const;
    [[nodiscard]] qsizetype retainedEntryCount() const;
    [[nodiscard]] DirectoryItemList takeItems();

private:
    void reject();

    SiblingCandidateAdmissionLimits m_limits;
    DirectoryItemList m_items;
    qsizetype m_retainedIdentityCodeUnitCount = 0;
    bool m_rejected = false;
};

using DirectoryItemListCallback = std::function<void(DirectoryItemList)>;
using DirectoryItemListProvider = std::function<ImageIoJob(
    QObject*, QUrl, DirectoryItemListCallback, KioOperationFailureCallback)>;

[[nodiscard]] SiblingCandidateAdmissionLimits defaultSiblingCandidateAdmissionLimits();
ImageIoJob startDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
    DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback);
ImageIoJob startDirectoryItemList(QObject* receiver, const QUrl& directoryUrl,
    DirectoryItemListCallback callback, const KioOperationFailureCallback& errorCallback,
    SiblingCandidateAdmissionLimits limits);
ImageIoJob startDirectoryItemList(QObject* receiver, QUrl directoryUrl,
    DirectoryItemListCallback callback, KioOperationFailureCallback errorCallback,
    DirectoryItemListProvider provider);
DirectoryItemListProvider defaultDirectoryItemListProvider(
    SiblingCandidateAdmissionLimits limits = defaultSiblingCandidateAdmissionLimits());
}

#endif
