// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILROWSTORE_H
#define KIRIVIEW_ACTIVENAVIGATIONTHUMBNAILROWSTORE_H

#include "location/sourcekey.h"
#include "session/activenavigationthumbnailmodel.h"
#include "session/activenavigationthumbnailprojection.h"
#include "session/activenavigationthumbnailwork.h"
#include "session/thumbnailimagestore.h"

#include <QHash>
#include <QImage>
#include <QUrl>
#include <QtGlobal>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QAbstractListModel;
class QObject;

namespace kiriview {
struct ActiveNavigationThumbnailResult
{
    ActiveNavigationThumbnailResultStatus status = ActiveNavigationThumbnailResultStatus::NoResult;
    QUrl imageSource;
};

enum class ActiveNavigationThumbnailRowUpdateKind {
    ProjectionOnly,
    SourceRefresh,
    IdentityReplacement,
};

class ActiveNavigationThumbnailRowUpdatePlan final
{
public:
    ActiveNavigationThumbnailRowUpdatePlan(ActiveNavigationThumbnailRowUpdatePlan&&) noexcept
        = default;
    ActiveNavigationThumbnailRowUpdatePlan& operator=(
        ActiveNavigationThumbnailRowUpdatePlan&&) noexcept
        = default;
    ~ActiveNavigationThumbnailRowUpdatePlan() = default;

    ActiveNavigationThumbnailRowUpdatePlan(const ActiveNavigationThumbnailRowUpdatePlan&) = delete;
    ActiveNavigationThumbnailRowUpdatePlan& operator=(const ActiveNavigationThumbnailRowUpdatePlan&)
        = delete;

    [[nodiscard]] ActiveNavigationThumbnailRowUpdateKind kind() const { return m_kind; }

private:
    friend class ActiveNavigationThumbnailRowStore;
    ActiveNavigationThumbnailRowUpdatePlan(ActiveNavigationThumbnailRowUpdateKind kind,
        int currentNumber, quint64 navigationGeneration,
        std::vector<ActiveNavigationThumbnailRow> rows,
        std::vector<ThumbnailSourceRevisionKey> sourceKeys)
        : m_kind(kind)
        , m_currentNumber(currentNumber)
        , m_navigationGeneration(navigationGeneration)
        , m_rows(std::move(rows))
        , m_sourceKeys(std::move(sourceKeys))
    {
    }

    ActiveNavigationThumbnailRowUpdateKind m_kind
        = ActiveNavigationThumbnailRowUpdateKind::ProjectionOnly;
    int m_currentNumber = 0;
    quint64 m_navigationGeneration = 0;
    std::vector<ActiveNavigationThumbnailRow> m_rows;
    std::vector<ThumbnailSourceRevisionKey> m_sourceKeys;
};

struct ActiveNavigationThumbnailRowCommit
{
    ActiveNavigationThumbnailRowUpdateKind kind
        = ActiveNavigationThumbnailRowUpdateKind::ProjectionOnly;
    int currentNumber = 0;
    std::optional<ActiveNavigationThumbnailSchedulingSnapshot> schedulingSnapshot;
};

class ActiveNavigationThumbnailRowPort
{
public:
    virtual ~ActiveNavigationThumbnailRowPort() = default;
    Q_DISABLE_COPY_MOVE(ActiveNavigationThumbnailRowPort)

    [[nodiscard]] virtual bool hasUsableReadyImage(
        const ThumbnailSourceRevisionKey& sourceKey) const
        = 0;
    virtual void applyPending(const ThumbnailSourceRevisionKey& sourceKey) = 0;
    virtual void applyUnsupported(const ThumbnailSourceRevisionKey& sourceKey) = 0;
    virtual void applyFailed(const ThumbnailSourceRevisionKey& sourceKey) = 0;
    virtual bool installReadyImage(const ThumbnailSourceRevisionKey& sourceKey, const QImage& image,
        ThumbnailImageRetentionPriority priority, bool preserveExistingReadyImage)
        = 0;
    virtual void updateRetentionPriority(
        const ThumbnailSourceRevisionKey& sourceKey, ThumbnailImageRetentionPriority priority)
        = 0;

protected:
    ActiveNavigationThumbnailRowPort() = default;
};

class ActiveNavigationThumbnailRowStore final : public ActiveNavigationThumbnailRowPort
{
public:
    explicit ActiveNavigationThumbnailRowStore(
        std::shared_ptr<ThumbnailImageStore> imageStore = {});
    ~ActiveNavigationThumbnailRowStore() override;
    Q_DISABLE_COPY_MOVE(ActiveNavigationThumbnailRowStore)

    [[nodiscard]] QAbstractListModel* model() const;
    [[nodiscard]] quint64 navigationGeneration() const;
    [[nodiscard]] ActiveNavigationThumbnailRowUpdatePlan prepareRows(
        std::vector<ActiveNavigationThumbnailRow> rows) const;
    ActiveNavigationThumbnailRowCommit commitRows(ActiveNavigationThumbnailRowUpdatePlan plan);
    [[nodiscard]] ActiveNavigationThumbnailSchedulingSnapshot schedulingSnapshot() const;
    void setCurrentNumber(int currentNumber);
    [[nodiscard]] bool hasUsableReadyImage(
        const ThumbnailSourceRevisionKey& sourceKey) const override;
    void applyPending(const ThumbnailSourceRevisionKey& sourceKey) override;
    void applyUnsupported(const ThumbnailSourceRevisionKey& sourceKey) override;
    void applyFailed(const ThumbnailSourceRevisionKey& sourceKey) override;
    bool installReadyImage(const ThumbnailSourceRevisionKey& sourceKey, const QImage& image,
        ThumbnailImageRetentionPriority priority, bool preserveExistingReadyImage) override;
    void updateRetentionPriority(const ThumbnailSourceRevisionKey& sourceKey,
        ThumbnailImageRetentionPriority priority) override;

private:
    struct RowState
    {
        ActiveNavigationThumbnailRow row;
        ThumbnailSourceRevisionKey sourceKey;
        ActiveNavigationThumbnailResult result;
        QString imageStoreId;
    };

    static bool sameRowIdentity(const ThumbnailRowKey& left, const ThumbnailRowKey& right);
    [[nodiscard]] std::optional<std::size_t> rowIndexForSourceKey(
        const ThumbnailSourceRevisionKey& sourceKey) const;
    [[nodiscard]] bool hasUsableReadyImage(const RowState& state) const;
    void releaseImage(RowState& state);
    void releaseAllImages();
    void rebuildRowIndexes();
    void publishRows();
    void publishResultAt(std::size_t row);

    std::unique_ptr<ActiveNavigationThumbnailModel> m_model;
    std::shared_ptr<ThumbnailImageStore> m_imageStore;
    std::vector<RowState> m_rows;
    quint64 m_navigationGeneration = 0;
    QHash<ThumbnailSourceRevisionKey, std::size_t> m_rowIndexBySourceKey;
};
}

#endif
