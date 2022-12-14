// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
// SPDX-FileCopyrightText: 2018-2022 Harald Sitter <sitter@kde.org>

import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0

Item {
    property alias cfg_Background: showBackground.checked
    property alias cfg_Pride: pride.checked
    property alias cfg_Anchor: anchorCombo.currentIndex
    property alias cfg_Width: widthSpin.value
    property alias cfg_Spacing: spacingSpin.value

    ColumnLayout {
        Layout.fillWidth: true

        GridLayout {
            columns: 2
            Layout.fillWidth: true

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18nc("@label Show the applet background or not", "Show Background:")
            }
            CheckBox {
                id: showBackground
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18nc("@label colorful pride mode", "Pride:")
            }
            CheckBox {
                id: pride
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18nc("@label where to anchor the bars (top or bottom)", "Anchor Position:")
            }
            ComboBox {
                id: anchorCombo
                model: [i18nc("@option anchor position", "Bottom"), i18nc("@option anchor position", "Top")]
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18nc("@label width of spectrum bars (in pixels)", "Bar Width:")
            }
            SpinBox {
                id: widthSpin
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: i18nc("@label pixel spacing between bars", "Spacing:")
            }
            SpinBox {
                id: spacingSpin
            }
        }
    }
}
