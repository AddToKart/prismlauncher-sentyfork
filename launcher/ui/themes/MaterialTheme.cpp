// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Tayou <git@tayou.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */
#include "MaterialTheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStyleFactory>
#include "Application.h"
#include "HintOverrideProxyStyle.h"

MaterialTheme::MaterialTheme(const QString& id, const QString& name, const QString& qssPath, const QString& iconPath, bool dark)
    : m_id(id), m_name(name), m_iconPath(iconPath), m_dark(dark)
{
    QFile file(qssPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        m_qss = QString::fromUtf8(file.readAll());
        file.close();
    }
}

void MaterialTheme::apply(bool initial)
{
    APPLICATION->setStyleSheet(QString());
    QApplication::setStyle(new HintOverrideProxyStyle(QStyleFactory::create(qtTheme())));
    QApplication::setPalette(colorScheme());
    APPLICATION->setStyleSheet(appStyleSheet());
    QDir::setSearchPaths("theme", searchPaths());
    QDir::setSearchPaths("icon", { m_iconPath });
}

QString MaterialTheme::id()
{
    return m_id;
}

QString MaterialTheme::name()
{
    return m_name;
}

bool MaterialTheme::hasStyleSheet()
{
    return true;
}

QString MaterialTheme::appStyleSheet()
{
    return m_qss;
}

QString MaterialTheme::qtTheme()
{
    return QString("Fusion");
}

QPalette MaterialTheme::colorScheme()
{
    QPalette pal;

    if (m_dark) {
        pal.setColor(QPalette::Window, QColor(48, 48, 48));
        pal.setColor(QPalette::WindowText, QColor(224, 224, 224));
        pal.setColor(QPalette::Base, QColor(30, 30, 30));
        pal.setColor(QPalette::AlternateBase, QColor(42, 42, 42));
        pal.setColor(QPalette::ToolTipBase, QColor(97, 97, 97));
        pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
        pal.setColor(QPalette::Text, QColor(224, 224, 224));
        pal.setColor(QPalette::Button, QColor(48, 48, 48));
        pal.setColor(QPalette::ButtonText, QColor(224, 224, 224));
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Link, QColor(66, 165, 245));
        pal.setColor(QPalette::Highlight, QColor(66, 165, 245));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
    } else {
        pal.setColor(QPalette::Window, QColor(250, 250, 250));
        pal.setColor(QPalette::WindowText, QColor(33, 33, 33));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        pal.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        pal.setColor(QPalette::ToolTipBase, QColor(97, 97, 97));
        pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
        pal.setColor(QPalette::Text, QColor(33, 33, 33));
        pal.setColor(QPalette::Button, QColor(250, 250, 250));
        pal.setColor(QPalette::ButtonText, QColor(33, 33, 33));
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Link, QColor(25, 118, 210));
        pal.setColor(QPalette::Highlight, QColor(25, 118, 210));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::PlaceholderText, QColor(180, 180, 180));
    }

    return fadeInactive(pal, fadeAmount(), fadeColor());
}

double MaterialTheme::fadeAmount()
{
    return 0.5;
}

QColor MaterialTheme::fadeColor()
{
    if (m_dark) {
        return QColor(38, 38, 38);
    }
    return QColor(200, 200, 200);
}

LogColors MaterialTheme::logColorScheme()
{
    return defaultLogColors(colorScheme());
}

QStringList MaterialTheme::searchPaths()
{
    return { m_iconPath };
}
