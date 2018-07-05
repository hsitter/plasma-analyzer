/*
    Copyright 2018 Harald Sitter <sitter@kde.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0

Item {
    property alias cfg_Background: showBackground.checked
    property alias cfg_Pride: pride.checked
    property alias cfg_Anchor: anchorCombo.currentIndex

    ColumnLayout {
        Layout.fillWidth: true

        GridLayout {
            columns: 2
            Layout.fillWidth: true

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("Show Background:")
            }
            CheckBox {
                id: showBackground
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("Pride:")
            }
            CheckBox {
                id: pride
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18n("Anchor Position:")
            }
            ComboBox {
                id: anchorCombo
                model: ["Bottom", "Top"]
            }
        }
    }

}
