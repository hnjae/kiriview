#pragma once

#include <QtCore/QString>

namespace ImageViewportInternal {

class PublicDiagnosticText
{
public:
    PublicDiagnosticText() = default;

    static PublicDiagnosticText fromUntrusted(QString diagnostic);

    PublicDiagnosticText withFallback(QString fallback) const;

    const QString& text() const { return m_text; }
    bool isEmpty() const { return m_text.isEmpty(); }
    void clear() { m_text.clear(); }

    friend bool operator==(const PublicDiagnosticText& lhs, const PublicDiagnosticText& rhs)
    {
        return lhs.m_text == rhs.m_text;
    }
    friend bool operator!=(const PublicDiagnosticText& lhs, const PublicDiagnosticText& rhs)
    {
        return !(lhs == rhs);
    }

private:
    explicit PublicDiagnosticText(QString text);

    QString m_text;
};

} // namespace ImageViewportInternal
