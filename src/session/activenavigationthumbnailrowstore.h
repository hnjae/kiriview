// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILROWSTORE_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILROWSTORE_H

#include "location/sourcekey.h"
#include "session/activenavigationthumbnailmodel.h"
#include "session/activenavigationthumbnailprojection.h"
#include "session/thumbnailimagestore.h"

#include <QHash>
#include <QImage>
#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

class QAbstractListModel;
class QObject;

namespace kiriview {
struct ActiveNavigationThumbnailResult
{
    ActiveNavigationThumbnailResultStatus status = ActiveNavigationThumbnailResultStatus::NoResult;
    QUrl imageSource;
};

class ActiveNavigationThumbnailRowPort
{
public:
    virtual ~ActiveNavigationThumbnailRowPort() = default;

    ActiveNavigationThumbnailRowPort(const ActiveNavigationThumbnailRowPort&) = delete;
    ActiveNavigationThumbnailRowPort& operator=(const ActiveNavigationThumbnailRowPort&) = delete;

    virtual bool hasUsableReadyImage(const ThumbnailSourceKey& sourceKey) const = 0;
    virtual void applyPending(const ThumbnailSourceKey& sourceKey) = 0;
    virtual void applyUnsupported(const ThumbnailSourceKey& sourceKey) = 0;
    virtual void applyFailed(const ThumbnailSourceKey& sourceKey) = 0;
    virtual bool installReadyImage(const ThumbnailSourceKey& sourceKey, const QImage& image,
        ThumbnailImageRetentionPriority priority, bool preserveExistingReadyImage)
        = 0;
    virtual void updateRetentionPriority(
        const ThumbnailSourceKey& sourceKey, ThumbnailImageRetentionPriority priority)
        = 0;

protected:
    ActiveNavigationThumbnailRowPort() = default;
};

class ActiveNavigationThumbnailRowStore final : public ActiveNavigationThumbnailRowPort
{
public:
    explicit ActiveNavigationThumbnailRowStore(
        QObject* owner = nullptr, std::shared_ptr<ThumbnailImageStore> imageStore = {});
    ~ActiveNavigationThumbnailRowStore() override;

    ActiveNavigationThumbnailRowStore(const ActiveNavigationThumbnailRowStore&) = delete;
    ActiveNavigationThumbnailRowStore& operator=(const ActiveNavigationThumbnailRowStore&) = delete;

    QAbstractListModel* model() const;
    quint64 navigationGeneration() const;
    std::size_t rowCount() const;
    std::vector<ThumbnailSourceKey> sourceKeys() const;
    bool hasSameRowIdentities(const std::vector<ActiveNavigationThumbnailRow>& rows) const;
    void setRows(std::vector<ActiveNavigationThumbnailRow> rows);
    void setCurrentNumber(int currentNumber);
    ActiveNavigationThumbnailResult resultAt(std::size_t row) const;

    std::optional<std::size_t> rowIndexForIdentity(
        int number, const QUrl& url, quint64 navigationGeneration) const;
    std::optional<std::size_t> rowIndexForSourceKey(const ThumbnailSourceKey& sourceKey) const;
    ThumbnailSourceKey sourceKeyAt(std::size_t row) const;
    bool hasUsableReadyImage(const ThumbnailSourceKey& sourceKey) const override;
    void applyPending(const ThumbnailSourceKey& sourceKey) override;
    void applyUnsupported(const ThumbnailSourceKey& sourceKey) override;
    void applyFailed(const ThumbnailSourceKey& sourceKey) override;
    void applyResult(
        const ThumbnailSourceKey& sourceKey, const ActiveNavigationThumbnailResult& result);
    bool installReadyImage(const ThumbnailSourceKey& sourceKey, const QImage& image,
        ThumbnailImageRetentionPriority priority, bool preserveExistingReadyImage) override;
    void updateRetentionPriority(
        const ThumbnailSourceKey& sourceKey, ThumbnailImageRetentionPriority priority) override;

private:
    struct RowState
    {
        ActiveNavigationThumbnailRow row;
        ThumbnailSourceKey sourceKey;
        ActiveNavigationThumbnailResult result;
        QString imageStoreId;
    };

    static bool sameRowIdentity(
        const ActiveNavigationThumbnailRow& left, const ActiveNavigationThumbnailRow& right);
    static QString rowDemandIndexKey(int number, const QUrl& url, quint64 navigationGeneration);
    static QString sourceKeyIndexKey(const ThumbnailSourceKey& sourceKey);
    bool hasUsableReadyImage(const RowState& state) const;
    void releaseImage(RowState& state);
    void releaseAllImages();
    void rebuildRowIndexes();
    void publishRows();
    void publishResultAt(std::size_t row);

    std::unique_ptr<ActiveNavigationThumbnailModel> m_model;
    std::shared_ptr<ThumbnailImageStore> m_imageStore;
    std::vector<RowState> m_rows;
    quint64 m_navigationGeneration = 0;
    QHash<QString, std::size_t> m_rowIndexByDemandIdentity;
    QHash<QString, std::size_t> m_rowIndexBySourceKey;
};
}

#endif
