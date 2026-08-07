// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_KIRIMEDIAINFORMATION_H
#define KIRIVIEW_KIRIMEDIAINFORMATION_H

#include "session/mediainformationprojection.h"

#include <QAbstractListModel>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <vector>

class KiriDocumentSession;

namespace kiriview {
struct MediaInformationEffects
{
    std::function<void(QString)> copyText;
    std::function<void(QUrl, QPointer<QObject>)> openContainingFolder;
};

MediaInformationEffects mediaInformationEffectsWithDefaults(MediaInformationEffects effects = {});
}

class KiriMediaInformationRowModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        ValueRole,
    };
    Q_ENUM(Role)

    using Row = kiriview::MediaInformationProjectionRow;

    explicit KiriMediaInformationRowModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setRows(std::vector<Row> rows);

private:
    std::vector<Row> m_rows;
};

class KiriMediaInformation : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KiriMediaInformation)
    QML_UNCREATABLE("KiriMediaInformation is owned by KiriDocumentSession")

    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(quint64 revision READ revision NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString mediaSectionTitle READ mediaSectionTitle NOTIFY changed)
    Q_PROPERTY(bool hasCameraSection READ hasCameraSection NOTIFY changed)
    Q_PROPERTY(bool hasAdvancedSection READ hasAdvancedSection NOTIFY changed)
    Q_PROPERTY(QAbstractListModel* generalRows READ generalRows CONSTANT)
    Q_PROPERTY(QAbstractListModel* mediaRows READ mediaRows CONSTANT)
    Q_PROPERTY(QAbstractListModel* cameraRows READ cameraRows CONSTANT)
    Q_PROPERTY(QAbstractListModel* advancedRows READ advancedRows CONSTANT)
    Q_PROPERTY(bool canCopyFilePath READ canCopyFilePath NOTIFY changed)
    Q_PROPERTY(bool canOpenContainingFolder READ canOpenContainingFolder NOTIFY changed)

public:
    explicit KiriMediaInformation(KiriDocumentSession& session,
        kiriview::MediaInformationEffects effects = {}, QObject* parent = nullptr);

    [[nodiscard]] bool available() const;
    [[nodiscard]] quint64 revision() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString summary() const;
    [[nodiscard]] QString mediaSectionTitle() const;
    [[nodiscard]] bool hasCameraSection() const;
    [[nodiscard]] bool hasAdvancedSection() const;
    QAbstractListModel* generalRows();
    QAbstractListModel* mediaRows();
    QAbstractListModel* cameraRows();
    QAbstractListModel* advancedRows();
    [[nodiscard]] bool canCopyFilePath() const;
    [[nodiscard]] bool canOpenContainingFolder() const;

    Q_INVOKABLE void copyFilePath();
    Q_INVOKABLE void openContainingFolder();

Q_SIGNALS:
    void changed();

private:
    void refresh();
    [[nodiscard]] QUrl targetUrl() const;
    [[nodiscard]] QString copiedFilePath() const;

    KiriDocumentSession& m_session;
    kiriview::MediaInformationEffects m_effects;
    KiriMediaInformationRowModel m_generalRows;
    KiriMediaInformationRowModel m_mediaRows;
    KiriMediaInformationRowModel m_cameraRows;
    KiriMediaInformationRowModel m_advancedRows;
    quint64 m_revision = 0;
    bool m_available = false;
    bool m_canCopyFilePath = false;
    bool m_canOpenContainingFolder = false;
    QUrl m_targetUrl;
    QString m_title;
    QString m_summary;
    QString m_mediaSectionTitle;
};

#endif
