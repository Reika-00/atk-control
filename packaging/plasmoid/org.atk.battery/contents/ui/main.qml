import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasma5support as Plasma5Support
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    property var battery: ({})
    property bool connected: battery.connected === true
    property int percent: connected ? (battery.percent !== undefined ? battery.percent : -1) : -1
    property bool charging: connected && battery.charging === true
    property int voltage: connected ? (battery.voltage_mv !== undefined ? battery.voltage_mv : 0) : 0
    property string model: connected ? (battery.model || "") : ""
    property string percentText: connected ? (percent >= 0 ? percent + "%" : "?") : "--"
    property string batteryIcon: {
        if (!connected) {
            return "battery-missing";
        }
        if (charging) {
            return "battery-charging";
        }
        if (percent >= 80) {
            return "battery-full";
        }
        if (percent >= 40) {
            return "battery-good";
        }
        if (percent >= 20) {
            return "battery-low";
        }
        return "battery-caution";
    }
    property string statusText: {
        if (!connected) {
            return "No ATK/VXE mouse connected";
        }
        var text = model.length > 0 ? model : "ATK/VXE mouse";
        text += " · " + percentText;
        if (charging) {
            text += " · charging";
        }
        if (voltage > 0) {
            text += " · " + voltage + " mV";
        }
        return text;
    }

    Plasmoid.title: "ATK Mouse Battery"
    Plasmoid.icon: batteryIcon
    Plasmoid.toolTipMainText: "ATK Mouse Battery"
    Plasmoid.toolTipSubText: statusText
    Plasmoid.preferredRepresentation: Plasmoid.compactRepresentation

    function refresh() {
        executable.connectSource("cat \"$HOME/.cache/atk-control/battery.json\" 2>/dev/null");
    }

    Plasma5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []
        onNewData: function(sourceName, data) {
            if (data["exit code"] !== 0) {
                return;
            }
            try {
                root.battery = JSON.parse(data.stdout);
            } catch (error) {
                root.battery = ({});
            }
        }
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: root.refresh()
    }

    compactRepresentation: RowLayout {
        spacing: 4

        Kirigami.Icon {
            source: root.batteryIcon
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
        }

        PlasmaComponents3.Label {
            text: root.percentText
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
        }
    }

    fullRepresentation: ColumnLayout {
        Layout.minimumWidth: 220
        Layout.preferredWidth: 250
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: root.batteryIcon
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
            }

            PlasmaComponents3.Label {
                text: "ATK Mouse Battery"
                font.bold: true
                Layout.fillWidth: true
            }
        }

        PlasmaComponents3.Label {
            text: root.statusText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        PlasmaComponents3.Label {
            visible: root.connected && root.battery.updated !== undefined
            text: "Updated: " + new Date(root.battery.updated * 1000).toLocaleTimeString()
            opacity: 0.7
            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents3.Button {
                text: "Refresh"
                onClicked: root.refresh()
            }

            PlasmaComponents3.Button {
                text: "Open ATK Control"
                onClicked: executable.connectSource("nohup atk-control >/dev/null 2>&1 &")
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
