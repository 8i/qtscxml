/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSCXMLINVOKABLESERVICE_H
#define QSCXMLINVOKABLESERVICE_H

#include <QtScxml/qscxmldatamodel.h>

#include <QString>
#include <QVariantMap>

QT_BEGIN_NAMESPACE

class QScxmlEvent;
class QScxmlStateMachine;
class QScxmlInvokableServiceFactory;

class Q_SCXML_EXPORT QScxmlInvokableService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QScxmlStateMachine *parentStateMachine READ parentStateMachine CONSTANT)
    Q_PROPERTY(bool autoforward READ autoforward CONSTANT)
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    QScxmlInvokableService(QScxmlInvokableServiceFactory *service, QScxmlStateMachine *parentStateMachine);
    virtual ~QScxmlInvokableService();

    bool autoforward() const;
    QScxmlStateMachine *parentStateMachine() const;

    virtual bool start() = 0;
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual void postEvent(QScxmlEvent *event) = 0;

    void finalize();

protected:
    QScxmlInvokableServiceFactory *service() const;

private:
    class Data;
    Data *d;
};

class Q_SCXML_EXPORT QScxmlInvokableServiceFactory
{
public:
    QScxmlInvokableServiceFactory(QScxmlExecutableContent::StringId invokeLocation,
                                  QScxmlExecutableContent::EvaluatorId srcexpr,
                                  QScxmlExecutableContent::StringId id,
                                  QScxmlExecutableContent::StringId idPrefix,
                                  QScxmlExecutableContent::StringId idlocation,
                                  const QVector<QScxmlExecutableContent::StringId> &namelist,
                                  bool autoforward,
                                  const QVector<QScxmlExecutableContent::ParameterInfo> &params,
                                  QScxmlExecutableContent::ContainerId finalizeContent);
    virtual ~QScxmlInvokableServiceFactory();

    virtual QScxmlInvokableService *invoke(QScxmlStateMachine *parent) = 0;

public: // callbacks from the service:
    QString calculateSrcexpr(QScxmlStateMachine *parent, bool *ok) const;
    QString calculateId(QScxmlStateMachine *parent, bool *ok) const;
    QVariantMap calculateData(QScxmlStateMachine *parent, bool *ok) const;
    bool autoforward() const;
    QScxmlExecutableContent::ContainerId finalizeContent() const;

private:
    class Data;
    Data *d;
};

class Q_SCXML_EXPORT QScxmlInvokableScxml: public QScxmlInvokableService
{
    Q_OBJECT
    Q_PROPERTY(QScxmlStateMachine *stateMachine READ stateMachine CONSTANT)
public:
    QScxmlInvokableScxml(QScxmlInvokableServiceFactory *service,
                         QScxmlStateMachine *stateMachine,
                         QScxmlStateMachine *parentStateMachine);
    virtual ~QScxmlInvokableScxml();

    bool start() Q_DECL_OVERRIDE;
    QString id() const Q_DECL_OVERRIDE;
    QString name() const Q_DECL_OVERRIDE;
    void postEvent(QScxmlEvent *event) Q_DECL_OVERRIDE;

    QScxmlStateMachine *stateMachine() const;

private:
    QScxmlStateMachine *m_stateMachine;
};

class Q_SCXML_EXPORT QScxmlInvokableScxmlServiceFactory: public QScxmlInvokableServiceFactory
{
public:
    QScxmlInvokableScxmlServiceFactory(QScxmlExecutableContent::StringId invokeLocation,
                                       QScxmlExecutableContent::EvaluatorId srcexpr,
                                       QScxmlExecutableContent::StringId id,
                                       QScxmlExecutableContent::StringId idPrefix,
                                       QScxmlExecutableContent::StringId idlocation,
                                       const QVector<QScxmlExecutableContent::StringId> &namelist,
                                       bool doAutoforward,
                                       const QVector<QScxmlExecutableContent::ParameterInfo> &params,
                                       QScxmlExecutableContent::ContainerId finalize);

protected:
#ifndef BUILD_QSCXMLC

    QScxmlInvokableService *loadAndInvokeDynamically(QScxmlStateMachine *parent,
                                                     const QString &sourceUrl);
#endif // BUILD_QSCXMLC
    QScxmlInvokableService *finishInvoke(QScxmlStateMachine *child,
                                         QScxmlStateMachine *parent);
};

template<class T>
class QScxmlInvokeScxmlFactory: public QScxmlInvokableScxmlServiceFactory
{
public:
    QScxmlInvokeScxmlFactory(QScxmlExecutableContent::StringId invokeLocation,
                             QScxmlExecutableContent::StringId id,
                             QScxmlExecutableContent::StringId idPrefix,
                             QScxmlExecutableContent::StringId idlocation,
                             const QVector<QScxmlExecutableContent::StringId> &namelist,
                             bool doAutoforward,
                             const QVector<QScxmlExecutableContent::ParameterInfo> &params,
                             QScxmlExecutableContent::ContainerId finalize)
        : QScxmlInvokableScxmlServiceFactory(invokeLocation, QScxmlExecutableContent::NoInstruction,
                                             id, idPrefix, idlocation, namelist, doAutoforward,
                                             params, finalize)
    {}

    QScxmlInvokableService *invoke(QScxmlStateMachine *parent) Q_DECL_OVERRIDE
    {
        return finishInvoke(new T, parent);
    }
};

class Q_SCXML_EXPORT QScxmlDynamicScxmlFactory: public QScxmlInvokableScxmlServiceFactory
{
public:
    QScxmlDynamicScxmlFactory(QScxmlExecutableContent::StringId invokeLocation,
                              QScxmlExecutableContent::EvaluatorId srcexpr,
                              QScxmlExecutableContent::StringId id,
                              QScxmlExecutableContent::StringId idPrefix,
                              QScxmlExecutableContent::StringId idlocation,
                              const QVector<QScxmlExecutableContent::StringId> &namelist,
                              bool doAutoforward,
                              const QVector<QScxmlExecutableContent::ParameterInfo> &params,
                              QScxmlExecutableContent::ContainerId finalize)
        : QScxmlInvokableScxmlServiceFactory(invokeLocation, srcexpr, id, idPrefix, idlocation,
                                             namelist, doAutoforward, params, finalize)
    {}

    QScxmlInvokableService *invoke(QScxmlStateMachine *parent) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QScxmlInvokableService *)

#endif // QSCXMLINVOKABLESERVICE_H
