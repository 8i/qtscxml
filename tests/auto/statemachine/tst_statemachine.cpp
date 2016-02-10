/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest>
#include <QObject>
#include <QXmlStreamReader>
#include <QtScxml/qscxmlparser.h>
#include <QtScxml/qscxmlstatemachine.h>

Q_DECLARE_METATYPE(QScxmlError);

enum { SpyWaitTime = 8000 };

class tst_StateMachine: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stateNames_data();
    void stateNames();
    void activeStateNames_data();
    void activeStateNames();
    void connectToFinal();
};

void tst_StateMachine::stateNames_data()
{
    QTest::addColumn<QString>("scxmlFileName");
    QTest::addColumn<bool>("compressed");
    QTest::addColumn<QStringList>("expectedStates");

    QTest::newRow("test1-compressed") << QString(":/tst_statemachine/test1.scxml")
                                      << true
                                      << (QStringList() << QString("a1") << QString("a2") << QString("final"));
    QTest::newRow("test1-not-compressed") << QString(":/tst_statemachine/test1.scxml")
                                      << false
                                      << (QStringList() << QString("a") << QString("a1") << QString("a2") << QString("b") << QString("final") << QString("top"));
}

void tst_StateMachine::stateNames()
{
    QFETCH(QString, scxmlFileName);
    QFETCH(bool, compressed);
    QFETCH(QStringList, expectedStates);

    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(scxmlFileName));
    QVERIFY(!stateMachine.isNull());

    QCOMPARE(stateMachine->stateNames(compressed), expectedStates);
}

void tst_StateMachine::activeStateNames_data()
{
    QTest::addColumn<QString>("scxmlFileName");
    QTest::addColumn<bool>("compressed");
    QTest::addColumn<QStringList>("expectedStates");

    QTest::newRow("test1-compressed") << QString(":/tst_statemachine/test1.scxml")
                                      << true
                                      << (QStringList() << QString("a1") << QString("final"));
    QTest::newRow("test1-not-compressed") << QString(":/tst_statemachine/test1.scxml")
                                      << false
                                      << (QStringList() << QString("a") << QString("a1") << QString("b") << QString("final") << QString("top"));
}

void tst_StateMachine::activeStateNames()
{
    QFETCH(QString, scxmlFileName);
    QFETCH(bool, compressed);
    QFETCH(QStringList, expectedStates);

    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(scxmlFileName));
    QVERIFY(!stateMachine.isNull());

    QSignalSpy stableStateSpy(stateMachine.data(), SIGNAL(reachedStableState(bool)));

    stateMachine->init();
    stateMachine->start();

    stableStateSpy.wait(5000);

    QCOMPARE(stateMachine->activeStateNames(compressed), expectedStates);
}

void tst_StateMachine::connectToFinal()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(QString(":/tst_statemachine/test1.scxml")));
    QVERIFY(!stateMachine.isNull());

    QState dummy;
    QVERIFY(stateMachine->connectToState(QString("final"), &dummy, SLOT(deleteLater())));
}

QTEST_MAIN(tst_StateMachine)

#include "tst_statemachine.moc"


