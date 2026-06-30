#pragma once

#include "imageviewport_test_support.h"

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

namespace {

QString componentErrors(const QQmlComponent& component)
{
    QStringList messages;
    const QList<QQmlError> errors = component.errors();
    for (const QQmlError& error : errors) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

}
