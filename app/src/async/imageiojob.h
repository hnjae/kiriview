// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_IMAGEIOJOB_H
#define KIRIVIEW_IMAGEIOJOB_H

#include <QObject>
#include <QPointer>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace kiriview {
enum class ImageIoJobCancellationRetirement {
    Synchronous,
    Explicit,
};

class ImageIoJobState final
{
public:
    using CancelCallback = std::function<void(QObject*)>;
    using RetirementCallback = std::function<void()>;

    ImageIoJobState(QObject* object, CancelCallback cancelCallback,
        ImageIoJobCancellationRetirement cancellationRetirement);

    bool claim(QObject* object);
    template <typename Finish> bool claimAndRun(QObject* object, Finish&& finish)
    {
        if (!claim(object)) {
            return false;
        }

        std::forward<Finish>(finish)();
        return true;
    }
    void cancel();
    void retire();
    void setRetirementCallback(RetirementCallback callback);
    [[nodiscard]] bool isActive() const;

private:
    friend class ImageIoJob;
    friend class ImageIoJobCompletion;

    [[nodiscard]] QObject* token() const;

    QPointer<QObject> m_token;
    QPointer<QObject> m_activeObject;
    CancelCallback m_cancelCallback;
    ImageIoJobCancellationRetirement m_cancellationRetirement
        = ImageIoJobCancellationRetirement::Synchronous;
    mutable std::mutex m_retirementMutex;
    RetirementCallback m_retirementCallback;
    bool m_retired = false;
};

class ImageIoJobCompletion final
{
public:
    ImageIoJobCompletion() = default;
    explicit ImageIoJobCompletion(std::shared_ptr<ImageIoJobState> state);

    [[nodiscard]] QObject* object() const;
    [[nodiscard]] bool isActive() const;
    void cancel() const;
    void retire() const;

    template <typename Finish> bool claimAndRun(Finish&& finish) const
    {
        QObject* object = this->object();
        if (m_state == nullptr || object == nullptr) {
            return false;
        }

        return m_state->claimAndRun(object, std::forward<Finish>(finish));
    }

    template <typename Finish> bool claimAndDelete(Finish&& finish) const
    {
        QObject* object = this->object();
        if (m_state == nullptr || object == nullptr) {
            return false;
        }

        const QPointer<QObject> guardedObject(object);
        return m_state->claimAndRun(object, [&]() {
            std::forward<Finish>(finish)();
            if (guardedObject != nullptr) {
                guardedObject->deleteLater();
            }
        });
    }

private:
    std::shared_ptr<ImageIoJobState> m_state;
};

class ImageIoJob final
{
public:
    using CancelCallback = ImageIoJobState::CancelCallback;
    using RetirementCallback = ImageIoJobState::RetirementCallback;

    ImageIoJob() = default;
    ImageIoJob(QObject* object, CancelCallback cancelCallback,
        ImageIoJobCancellationRetirement cancellationRetirement
        = ImageIoJobCancellationRetirement::Synchronous);
    ~ImageIoJob();

    ImageIoJob(const ImageIoJob&) = delete;
    ImageIoJob& operator=(const ImageIoJob&) = delete;
    ImageIoJob(ImageIoJob&&) noexcept = default;
    ImageIoJob& operator=(ImageIoJob&& other) noexcept;

    void cancel();
    void setRetirementCallback(RetirementCallback callback);
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] ImageIoJobCompletion completion() const;

private:
    std::shared_ptr<ImageIoJobState> m_state;
};
}

#endif
