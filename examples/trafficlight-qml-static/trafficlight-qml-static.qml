/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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
    visible: true
    width: 100
    height: 350
    color: "black"

    Item {
        id: lights
        width: parent.width
        height: 300

        MouseArea {
            anchors.fill: parent
            onClicked: {
                Qt.quit();
            }
        }

        Light {
            id: redLight
            anchors.top: parent.top
            color: "red"
            visible: red.active || redGoingGreen.active
        }

        Light {
            id: yellowLight
            anchors.top: redLight.bottom
            color: "yellow"
            visible: yellow.active || blinking.active
        }

        Light {
            id: greenLight
            anchors.top: yellowLight.bottom
            color: "green"
            visible: green.active
        }
    }

    Item {
        width: parent.width
        anchors.top: lights.bottom
        anchors.bottom: parent.bottom

        Rectangle {
            x: 5
            y: 5
            width: parent.width - 10
            height: parent.height - 10
            radius: 5
            color: "blue"

            Text {
                anchors.fill: parent
                color: "white"
                text: {
                    if (working.active)
                        "Pause"
                    else
                        "Unpause"
                }
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                id: button
                anchors.fill: parent
            }
        }
    }

    Scxml.StateMachine {
        stateMachine: trafficLightStateMachine

        Scxml.State {
            id: red
            scxmlName: "red"
        }

        Scxml.State {
            id: yellow
            scxmlName: "yellow"
        }

        Scxml.State {
            id: redGoingGreen
            scxmlName: "red-going-green"
        }

        Scxml.State {
            id: green
            scxmlName: "green"
        }

        Scxml.State {
            id: blinking
            scxmlName: "blinking"
        }

        Scxml.State {
            id: working
            scxmlName: "working"
        }

        Scxml.SignalEvent {
            signal: button.clicked
            eventName: {
                if (working.active)
                    "smash"
                else
                    "repair"
            }
        }
    }
}
