// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

#include "plugin.h"

#include <QtQml>

#include "../StreamReader.h"

void Plugin::registerTypes(const char* uri)
{
    qmlRegisterSingletonType<StreamReader>(uri, 1, 0, "StreamReader", [](QQmlEngine *, QJSEngine *) -> QObject * {
        return StreamReader::instance();
    });
}
