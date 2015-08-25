/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.5
import QtQuick.Window 2.2
import Scxml 1.0 as Scxml

Window {
    id: root
    property QtObject stateMachine: scxmlLoader.stateMachine
    property alias filename: scxmlLoader.filename

    visible: true
    width: 750
    height: 350
    color: "white"

    ListView {
        id: theList
        width: parent.width / 2
        height: parent.height
        keyNavigationWraps: true
        highlightMoveDuration: 0
        focus: true
        model: ListModel {
            id: theModel
            ListElement { media: "Song 1" }
            ListElement { media: "Song 2" }
            ListElement { media: "Song 3" }
        }
        highlight: Rectangle { color: "lightsteelblue" }
        currentIndex: -1
        delegate: Rectangle {
            height: 40
            width: parent.width
            color: "transparent"
            MouseArea {
                anchors.fill: parent;
                onClicked: tap(index)
            }
            Text {
                id: txt
                anchors.fill: parent
                text: media
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    Text {
        id: theLog
        anchors.left: theList.right
        anchors.top: theText.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    Text {
        id: theText
        anchors.left: theList.right
        anchors.right: parent.right;
        anchors.top:  parent.top
        text: "Stopped"
        color: stateMachine.playing.active ? "green" : "red"
    }

    Scxml.StateMachineLoader {
        id: scxmlLoader
    }

    Connections {
        target: stateMachine
        onEvent_playbackStarted: {
            var media = data.media
            theText.text = "Playing '" + media + "'"
            theLog.text = theLog.text + "\nevent_playbackStarted with data: " + JSON.stringify(data)
        }
        onEvent_playbackStopped: {
            var media = data.media
            theText.text = "Stopped '" + media + "'"
            theLog.text = theLog.text + "\nevent_playbackStopped with data: " + JSON.stringify(data)
        }
    }

    function tap(idx) {
        var media = theModel.get(idx).media
        var data = { "media": media }
        stateMachine.event_tap(data)
    }
}
