/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtScxml module.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#ifndef SCXMLSTATEMACHINE_H
#define SCXMLSTATEMACHINE_H

#include <QtScxml/qscxmldatamodel.h>
#include <QtScxml/qscxmlexecutablecontent.h>
#include <QtScxml/qscxmlerror.h>
#include <QtScxml/qscxmlevent.h>

#include <QString>
#include <QVector>
#include <QUrl>
#include <QVariantList>

QT_BEGIN_NAMESPACE
class QIODevice;
class QXmlStreamWriter;
class QTextStream;
class QScxmlEventBuilder;
class QScxmlInvokableServiceFactory;
class QScxmlInvokableService;
class QScxmlParser;
class QScxmlStateMachine;
class QScxmlTableData;

class Q_SCXML_EXPORT QScxmlEventFilter
{
public:
    virtual ~QScxmlEventFilter();
    virtual bool handle(QScxmlEvent *event, QScxmlStateMachine *stateMachine) = 0;
};

class QScxmlStateMachinePrivate;
class Q_SCXML_EXPORT QScxmlStateMachine: public QObject
{
    Q_DECLARE_PRIVATE(QScxmlStateMachine)
    Q_OBJECT
    Q_ENUMS(BindingMethod)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

protected:
#ifndef Q_QDOC
    QScxmlStateMachine(QObject *parent = 0);
    QScxmlStateMachine(QScxmlStateMachinePrivate &dd, QObject *parent);
#endif // Q_QDOC

public:
    enum BindingMethod {
        EarlyBinding,
        LateBinding
    };

    static QScxmlStateMachine *fromFile(const QString &fileName);
    static QScxmlStateMachine *fromData(QIODevice *data, const QString &fileName = QString());
    QVector<QScxmlError> parseErrors() const;

    QString sessionId() const;
    void setSessionId(const QString &id);
    static QString generateSessionId(const QString &prefix);

    bool isInvoked() const;

    QScxmlDataModel *dataModel() const;

    BindingMethod dataBinding() const;

    bool init(const QVariantMap &initialDataValues = QVariantMap());

    bool isRunning() const;

    QString name() const;
    QStringList stateNames(bool compress = true) const;
    QStringList activeStateNames(bool compress = true) const;
    bool isActive(const QString &scxmlStateName) const;

    using QObject::connect;
    QMetaObject::Connection connect(const QString &scxmlStateName, const char *signal,
                                    const QObject *receiver, const char *method,
                                    Qt::ConnectionType type = Qt::AutoConnection);

    QScxmlEventFilter *scxmlEventFilter() const;
    void setScxmlEventFilter(QScxmlEventFilter *newFilter);

    Q_INVOKABLE void submitEvent(QScxmlEvent *event);
    Q_INVOKABLE void submitEvent(const QString &eventName);
    Q_INVOKABLE void submitEvent(const QString &eventName, const QVariant &data);
    void cancelDelayedEvent(const QString &sendId);

    bool isDispatchableTarget(const QString &target) const;

Q_SIGNALS:
    void runningChanged(bool running);
    void log(const QString &label, const QString &msg);
    void reachedStableState(bool didChange);
    void finished();

public Q_SLOTS:
    void start();

protected: // methods for friends:
    friend QScxmlDataModel;
    friend QScxmlEventBuilder;
    friend QScxmlInvokableServiceFactory;
    friend QScxmlExecutableContent::QScxmlExecutionEngine;

#ifndef Q_QDOC
    void setDataBinding(BindingMethod bindingMethod);
    virtual void setService(const QString &id, QScxmlInvokableService *service);

    QScxmlTableData *tableData() const;
    void setTableData(QScxmlTableData *tableData);
#endif // Q_QDOC
};

QT_END_NAMESPACE

#endif // SCXMLSTATEMACHINE_H
