import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 980
    height: 720
    minimumWidth: 700
    minimumHeight: 520
    visible: true
    title: qsTr("Spot")
    color: "#0c0b0c"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool playlistDialogVisible: false
    property bool settingsDialogVisible: false

    // Connections from C++ Backend (Tray activate, etc.)
    Connections {
        target: backend
        function onRequestShowWindow() {
            win.show()
            win.raise()
            win.requestActivate()
        }
        function onRequestHideWindow() {
            win.hide()
        }
    }

    // Keyboard Shortcuts
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.primaryAction() }
    Shortcut { sequence: "Ctrl+B"; context: Qt.ApplicationShortcut; onActivated: backend.runInBackground() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) win.helpVisible = false
            else if (win.playlistDialogVisible) win.playlistDialogVisible = false
            else if (win.settingsDialogVisible) win.settingsDialogVisible = false
            else if (backend.isListening) backend.cancelListening()
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // Top Header Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // App Brand
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 32
                        height: 32
                        radius: Math.min(8, theme.radius)
                        color: theme.accent

                        Text {
                            anchors.centerIn: parent
                            text: "♫"
                            font.pixelSize: 18
                            font.bold: true
                            color: theme.accentForeground
                        }
                    }

                    Text {
                        text: qsTr("Spot")
                        font.pixelSize: 20
                        font.bold: true
                        color: "#FAFCFB"
                    }
                }

                Item { Layout.fillWidth: true }

                // Audio Input Selector
                RowLayout {
                    spacing: 6

                    Text {
                        text: qsTr("Input:")
                        color: "#888888"
                        font.pixelSize: 13
                    }

                    ComboBox {
                        id: deviceCombo
                        Layout.preferredWidth: 260
                        model: backend.audioDevices
                        textRole: "name"
                        currentIndex: backend.selectedDeviceIndex
                        onActivated: function(index) {
                            backend.selectedDeviceIndex = index
                        }

                        background: Rectangle {
                            color: "#181818"
                            radius: Math.min(6, theme.radius)
                            border.color: deviceCombo.activeFocus ? theme.accent : "#2b2b2b"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: deviceCombo.displayText
                            color: "#FAFCFB"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }
                    }

                    Button {
                        text: "↻"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Refresh audio devices")
                        onClicked: backend.refreshAudioDevices()
                        background: Rectangle {
                            color: parent.down ? "#333333" : parent.hovered ? "#262626" : "#181818"
                            radius: Math.min(6, theme.radius)
                            border.color: "#2b2b2b"
                        }
                    }
                }

                // Spotify Status Button
                Button {
                    id: spotifyBtn
                    text: backend.spotifyConnected
                          ? (backend.spotifyUser.length > 0 ? qsTr("Spotify: %1").arg(backend.spotifyUser) : qsTr("Spotify Connected"))
                          : qsTr("Connect Spotify")
                    icon.color: backend.spotifyConnected ? "#1DB954" : theme.accent

                    background: Rectangle {
                        color: backend.spotifyConnected ? "#182c1f" : "#1f1a18"
                        radius: Math.min(6, theme.radius)
                        border.color: backend.spotifyConnected ? "#1DB954" : theme.accent
                        border.width: 1
                    }
                    contentItem: Text {
                        text: spotifyBtn.text
                        color: backend.spotifyConnected ? "#1DB954" : "#FAFCFB"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: win.settingsDialogVisible = true
                }

                // Continuous Auto-Listen Toggle
                Button {
                    id: contBtn
                    text: backend.continuousListening ? qsTr("Auto-Listen: ON") : qsTr("Auto-Listen: OFF")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Automatically keep identifying songs continuously")
                    background: Rectangle {
                        color: backend.continuousListening ? "#2b2318" : "#18181c"
                        radius: Math.min(6, theme.radius)
                        border.color: backend.continuousListening ? theme.accent : "#2b2b2b"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: contBtn.text
                        color: backend.continuousListening ? theme.accent : "#888888"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: backend.continuousListening = !backend.continuousListening
                }

                // Run in Background Button
                Button {
                    id: bgBtn
                    text: qsTr("Background")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Hide to tray and keep listening in the background (Ctrl+B)")
                    background: Rectangle {
                        color: parent.down ? "#33333a" : parent.hovered ? "#24242c" : "#1a1a20"
                        radius: Math.min(6, theme.radius)
                        border.color: "#333340"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "⤓"
                            color: theme.accent
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            text: bgBtn.text
                            color: "#FAFCFB"
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }
                    onClicked: backend.runInBackground()
                }

                // Help Button
                Button {
                    text: "?"
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Shortcuts")
                    background: Rectangle {
                        color: parent.hovered ? "#262626" : "#181818"
                        radius: Math.min(6, theme.radius)
                        border.color: "#2b2b2b"
                    }
                    onClicked: win.helpVisible = !win.helpVisible
                }
            }

            // Status Notice Bar
            Rectangle {
                Layout.fillWidth: true
                height: 32
                color: "#141416"
                radius: Math.min(6, theme.radius)
                visible: backend.statusText.length > 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Text {
                        text: backend.statusText
                        color: theme.accent
                        font.pixelSize: 12
                        font.bold: true
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }
            }

            // Main Hero Stage
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                color: "#121214"
                radius: theme.radius
                border.color: "#1e1e24"
                border.width: 1

                // 1. Listening State (Pulsing Radar Wave)
                Item {
                    anchors.fill: parent
                    visible: backend.isListening

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20

                        // Pulse animation rings
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            width: 120
                            height: 120

                            Rectangle {
                                id: ring1
                                anchors.centerIn: parent
                                width: 70
                                height: 70
                                radius: 35
                                color: "transparent"
                                border.color: theme.accent
                                border.width: 3
                                opacity: 0.8

                                NumberAnimation on scale {
                                    from: 1.0; to: 2.2; duration: 1600; loops: Animation.Infinite
                                }
                                NumberAnimation on opacity {
                                    from: 0.8; to: 0.0; duration: 1600; loops: Animation.Infinite
                                }
                            }

                            Rectangle {
                                id: ring2
                                anchors.centerIn: parent
                                width: 70
                                height: 70
                                radius: 35
                                color: "transparent"
                                border.color: theme.accent
                                border.width: 2
                                opacity: 0.8

                                NumberAnimation on scale {
                                    from: 1.0; to: 2.2; duration: 1600; loops: Animation.Infinite; running: true
                                    // slight offset
                                }
                                NumberAnimation on opacity {
                                    from: 0.8; to: 0.0; duration: 1600; loops: Animation.Infinite; running: true
                                }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: 70
                                height: 70
                                radius: 35
                                color: theme.accent

                                Text {
                                    anchors.centerIn: parent
                                    text: "♫"
                                    font.pixelSize: 32
                                    font.bold: true
                                    color: theme.accentForeground
                                }
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Listening for music...")
                            color: "#FAFCFB"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Cancel (Esc)")
                            onClicked: backend.cancelListening()
                            background: Rectangle {
                                color: "#242428"
                                radius: Math.min(6, theme.radius)
                            }
                        }
                    }
                }

                // 2. Idle State (No song identified yet)
                Item {
                    anchors.fill: parent
                    visible: !backend.isListening && !backend.hasTrack

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 16

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 80
                            height: 80
                            radius: 40
                            color: theme.accent

                            Text {
                                anchors.centerIn: parent
                                text: "♫"
                                font.pixelSize: 38
                                font.bold: true
                                color: theme.accentForeground
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.startListening()
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Identify any song playing around you")
                            color: "#FAFCFB"
                            font.pixelSize: 20
                            font.bold: true
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Press Space or click the button above to listen")
                            color: "#888888"
                            font.pixelSize: 14
                        }
                    }
                }

                // 3. Identified Track State (Spotify layout style)
                Item {
                    anchors.fill: parent
                    anchors.margins: 24
                    visible: !backend.isListening && backend.hasTrack

                    RowLayout {
                        anchors.fill: parent
                        spacing: 24

                        // Album Artwork
                        Rectangle {
                            Layout.preferredWidth: 230
                            Layout.preferredHeight: 230
                            radius: theme.radius
                            color: "#181818"
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: backend.trackCoverUrl
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: parent.children[0].status !== Image.Ready
                                text: "♫"
                                font.pixelSize: 64
                                color: "#444444"
                            }
                        }

                        // Track Details and Actions
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 8

                            Text {
                                text: qsTr("RECOGNIZED TRACK")
                                font.pixelSize: 11
                                font.bold: true
                                color: theme.accent
                            }

                            Text {
                                text: backend.trackTitle
                                font.pixelSize: 26
                                font.bold: true
                                color: "#FAFCFB"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: backend.trackArtist
                                font.pixelSize: 18
                                font.bold: true
                                color: "#e2dddc"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: backend.trackAlbum.length > 0
                                      ? (backend.trackYear.length > 0 ? qsTr("%1 • %2").arg(backend.trackAlbum, backend.trackYear) : backend.trackAlbum)
                                      : backend.trackYear
                                font.pixelSize: 13
                                color: "#8a8588"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Item { Layout.fillHeight: true }

                            // Action Bar
                            RowLayout {
                                spacing: 14

                                // Big Spotify Action Button
                                Button {
                                    id: openSpotBtn
                                    text: qsTr("Open in Spotify")
                                    font.bold: true
                                    background: Rectangle {
                                        color: parent.down ? "#169c46" : parent.hovered ? "#1ed760" : "#1DB954"
                                        radius: Math.min(20, theme.radius)
                                    }
                                    contentItem: RowLayout {
                                        spacing: 6
                                        Text {
                                            text: "▶"
                                            color: "black"
                                            font.pixelSize: 12
                                        }
                                        Text {
                                            text: openSpotBtn.text
                                            color: "black"
                                            font.bold: true
                                            font.pixelSize: 13
                                        }
                                    }
                                    onClicked: backend.openSpotify()
                                }

                                // Spotify Green Checkmark / Add to Playlist Button
                                Rectangle {
                                    id: playlistCheckBtn
                                    width: 36
                                    height: 36
                                    radius: 18
                                    color: backend.isTrackInPlaylists ? "#1DB954" : "transparent"
                                    border.color: backend.isTrackInPlaylists ? "#1DB954" : "#888888"
                                    border.width: 2

                                    Text {
                                        anchors.centerIn: parent
                                        text: backend.isTrackInPlaylists ? "✓" : "+"
                                        font.pixelSize: backend.isTrackInPlaylists ? 18 : 20
                                        font.bold: true
                                        color: backend.isTrackInPlaylists ? "black" : "#FAFCFB"
                                    }

                                    ToolTip.visible: checkMouse.containsMouse
                                    ToolTip.text: backend.isTrackInPlaylists
                                                  ? qsTr("Added to playlists (click to view/change)")
                                                  : qsTr("Add to playlist")

                                    MouseArea {
                                        id: checkMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!backend.spotifyConnected) {
                                                win.settingsDialogVisible = true
                                            } else {
                                                backend.refreshPlaylists()
                                                win.playlistDialogVisible = true
                                            }
                                        }
                                    }
                                }

                                // Liked Songs Heart Button
                                Rectangle {
                                    width: 36
                                    height: 36
                                    radius: 18
                                    color: "transparent"
                                    border.color: backend.isTrackLiked ? "#1DB954" : "#666666"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: backend.isTrackLiked ? "♥" : "♡"
                                        font.pixelSize: 18
                                        color: backend.isTrackLiked ? "#1DB954" : "#aaaaaa"
                                    }

                                    ToolTip.visible: heartMouse.containsMouse
                                    ToolTip.text: backend.isTrackLiked ? qsTr("In Liked Songs") : qsTr("Save to Liked Songs")

                                    MouseArea {
                                        id: heartMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!backend.spotifyConnected)
                                                win.settingsDialogVisible = true
                                            else
                                                backend.toggleLikedSongs()
                                        }
                                    }
                                }

                                // Copy Link Button
                                Button {
                                    text: qsTr("Copy Link")
                                    onClicked: backend.copySpotifyLink()
                                    background: Rectangle {
                                        color: parent.hovered ? "#282828" : "#181818"
                                        radius: Math.min(6, theme.radius)
                                        border.color: "#333333"
                                    }
                                }

                                // Listen Again Button
                                Button {
                                    text: qsTr("Listen Again (Space)")
                                    onClicked: backend.startListening()
                                    background: Rectangle {
                                        color: parent.hovered ? "#282828" : "#181818"
                                        radius: Math.min(6, theme.radius)
                                        border.color: "#333333"
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // History Section (Spotify Track List Style)
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Recently Identified")
                    font.pixelSize: 16
                    font.bold: true
                    color: "#FAFCFB"
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Clear History")
                    visible: backend.history.length > 0
                    background: Rectangle {
                        color: "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#888888"
                        font.pixelSize: 12
                    }
                    onClicked: backend.clearHistory()
                }
            }

            // Track List Table
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#121214"
                radius: theme.radius
                border.color: "#1e1e24"
                border.width: 1
                clip: true

                ListView {
                    id: historyList
                    anchors.fill: parent
                    anchors.margins: 8
                    model: backend.history
                    spacing: 4

                    delegate: Rectangle {
                        width: historyList.width
                        height: 52
                        radius: Math.min(6, theme.radius)
                        color: rowMouse.containsMouse ? "#202024" : "transparent"

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.selectHistoryTrack(index)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 14
                            z: 1

                            Text {
                                text: (index + 1).toString()
                                color: "#666666"
                                font.pixelSize: 13
                                Layout.preferredWidth: 24
                            }

                            // Small Album art
                            Rectangle {
                                width: 36
                                height: 36
                                radius: Math.min(4, theme.radius)
                                color: "#222222"
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: modelData.coverUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                }
                            }

                            // Title & Artist
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: modelData.title || ""
                                    color: "#FAFCFB"
                                    font.pixelSize: 14
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: modelData.artist || ""
                                    color: "#888888"
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                text: modelData.album || ""
                                color: "#777777"
                                font.pixelSize: 12
                                Layout.preferredWidth: 200
                                elide: Text.ElideRight
                            }

                            Button {
                                text: qsTr("Load")
                                onClicked: backend.selectHistoryTrack(index)
                                background: Rectangle {
                                    color: parent.down ? "#444444" : parent.hovered ? "#333333" : "#222222"
                                    radius: Math.min(4, theme.radius)
                                }
                            }
                        }
                    }

                    // Empty history notice
                    Text {
                        anchors.centerIn: parent
                        visible: historyList.count === 0
                        text: qsTr("No identified songs yet. Press Space to identify what's playing.")
                        color: "#555555"
                        font.pixelSize: 14
                    }
                }
            }
        }

        // ==========================================
        // Modal / Dialog 1: Playlist Selector Popover
        // ==========================================
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.75)
            visible: win.playlistDialogVisible

            MouseArea {
                anchors.fill: parent
                onClicked: win.playlistDialogVisible = false
            }

            Rectangle {
                width: 480
                height: 480
                anchors.centerIn: parent
                color: "#18181c"
                radius: theme.radius
                border.color: "#2a2a30"
                border.width: 1

                MouseArea {
                    anchors.fill: parent // prevent clicks from falling through
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Add to Playlist")
                            font.pixelSize: 18
                            font.bold: true
                            color: "#FAFCFB"
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "✕"
                            onClicked: win.playlistDialogVisible = false
                            background: Rectangle { color: "transparent" }
                        }
                    }

                    // Liked Songs row
                    Rectangle {
                        Layout.fillWidth: true
                        height: 46
                        radius: Math.min(6, theme.radius)
                        color: "#222226"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            Text {
                                text: "♥"
                                color: "#1DB954"
                                font.pixelSize: 18
                            }

                            Text {
                                text: qsTr("Liked Songs")
                                color: "#FAFCFB"
                                font.bold: true
                                font.pixelSize: 14
                                Layout.fillWidth: true
                            }

                            CheckBox {
                                checked: backend.isTrackLiked
                                onToggled: backend.toggleLikedSongs()
                            }
                        }
                    }

                    Text {
                        text: qsTr("Your Playlists")
                        color: "#888888"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    // Playlists ListView
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: playlistsView
                            model: backend.playlists
                            spacing: 6

                            delegate: Rectangle {
                                width: playlistsView.width
                                height: 48
                                radius: Math.min(6, theme.radius)
                                color: plRowMouse.containsMouse ? "#24242a" : "#1d1d22"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: Math.min(4, theme.radius)
                                        color: "#333333"
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            source: modelData.imageUrl || ""
                                            fillMode: Image.PreserveAspectCrop
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: modelData.name || ""
                                            color: "#FAFCFB"
                                            font.pixelSize: 13
                                            font.bold: true
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: qsTr("%n songs", "", modelData.tracksCount || 0).arg(modelData.tracksCount || 0)
                                            color: "#888888"
                                            font.pixelSize: 11
                                        }
                                    }

                                    CheckBox {
                                        checked: backend.trackPlaylistIds.indexOf(modelData.id) !== -1
                                        onToggled: backend.toggleTrackInPlaylist(modelData.id)
                                    }
                                }

                                MouseArea {
                                    id: plRowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: backend.toggleTrackInPlaylist(modelData.id)
                                }
                            }
                        }
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Done")
                        onClicked: win.playlistDialogVisible = false
                        background: Rectangle {
                            color: theme.accent
                            radius: Math.min(6, theme.radius)
                        }
                        contentItem: Text {
                            text: parent.text
                            color: theme.accentForeground
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }

        // ==========================================
        // Modal / Dialog 2: Spotify Settings Popover
        // ==========================================
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.75)
            visible: win.settingsDialogVisible

            MouseArea {
                anchors.fill: parent
                onClicked: win.settingsDialogVisible = false
            }

            Rectangle {
                width: 500
                height: 380
                anchors.centerIn: parent
                color: "#18181c"
                radius: theme.radius
                border.color: "#2a2a30"
                border.width: 1

                MouseArea { anchors.fill: parent }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Text {
                        text: qsTr("Spotify Account & API")
                        font.pixelSize: 18
                        font.bold: true
                        color: "#FAFCFB"
                    }

                    Text {
                        text: qsTr("Connect your Spotify account to directly add identified songs to your playlists and check your saved music.")
                        color: "#aaaaaa"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: qsTr("Spotify Client ID:")
                            color: "#888888"
                            font.pixelSize: 12
                        }

                        TextField {
                            id: clientIdInput
                            Layout.fillWidth: true
                            text: backend.spotifyClientId
                            color: "#FAFCFB"
                            placeholderText: qsTr("Enter Spotify Client ID")
                            background: Rectangle {
                                color: "#222226"
                                radius: Math.min(6, theme.radius)
                                border.color: "#333333"
                            }
                            onEditingFinished: backend.spotifyClientId = text
                        }
                    }

                    RowLayout {
                        spacing: 12

                        Button {
                            text: backend.spotifyConnected ? qsTr("Reconnect Spotify") : qsTr("Connect with Spotify")
                            font.bold: true
                            onClicked: {
                                backend.spotifyClientId = clientIdInput.text
                                backend.startSpotifyAuth()
                            }
                            background: Rectangle {
                                color: "#1DB954"
                                radius: Math.min(6, theme.radius)
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "black"
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        Button {
                            text: qsTr("Disconnect")
                            visible: backend.spotifyConnected
                            onClicked: backend.disconnectSpotify()
                            background: Rectangle {
                                color: "#333338"
                                radius: Math.min(6, theme.radius)
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Close")
                        onClicked: win.settingsDialogVisible = false
                        background: Rectangle {
                            color: "#2a2a30"
                            radius: Math.min(6, theme.radius)
                        }
                    }
                }
            }
        }

        // ==========================================
        // Modal / Dialog 3: Keyboard Shortcuts Overlay (?)
        // ==========================================
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.8)
            visible: win.helpVisible

            MouseArea {
                anchors.fill: parent
                onClicked: win.helpVisible = false
            }

            Rectangle {
                width: 440
                height: 280
                anchors.centerIn: parent
                color: "#18181c"
                radius: theme.radius
                border.color: "#2a2a30"
                border.width: 1

                MouseArea { anchors.fill: parent }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("Keyboard Shortcuts")
                            font.pixelSize: 18
                            font.bold: true
                            color: "#FAFCFB"
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "✕"
                            onClicked: win.helpVisible = false
                            background: Rectangle { color: "transparent" }
                        }
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 10

                        Text { text: "Space"; font.bold: true; color: theme.accent; font.pixelSize: 14 }
                        Text { text: qsTr("Identify song / Stop listening"); color: "#FAFCFB"; font.pixelSize: 14 }

                        Text { text: "Ctrl+B"; font.bold: true; color: theme.accent; font.pixelSize: 14 }
                        Text { text: qsTr("Run in background (tray)"); color: "#FAFCFB"; font.pixelSize: 14 }

                        Text { text: "?"; font.bold: true; color: theme.accent; font.pixelSize: 14 }
                        Text { text: qsTr("Toggle shortcuts help"); color: "#FAFCFB"; font.pixelSize: 14 }

                        Text { text: "Esc"; font.bold: true; color: theme.accent; font.pixelSize: 14 }
                        Text { text: qsTr("Close open popups / dialogs"); color: "#FAFCFB"; font.pixelSize: 14 }

                        Text { text: "Q"; font.bold: true; color: theme.accent; font.pixelSize: 14 }
                        Text { text: qsTr("Quit Spot"); color: "#FAFCFB"; font.pixelSize: 14 }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
