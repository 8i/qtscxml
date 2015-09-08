/****************************************************************************
 **
 ** Copyright (c) 2015 Digia Plc
 ** For any questions to Digia, please use contact form at http://qt.digia.com/
 **
 ** All Rights Reserved.
 **
 ** NOTICE: All information contained herein is, and remains
 ** the property of Digia Plc and its suppliers,
 ** if any. The intellectual and technical concepts contained
 ** herein are proprietary to Digia Plc
 ** and its suppliers and may be covered by Finnish and Foreign Patents,
 ** patents in process, and are protected by trade secret or copyright law.
 ** Dissemination of this information or reproduction of this material
 ** is strictly forbidden unless prior written permission is obtained
 ** from Digia Plc.
 ****************************************************************************/

#ifndef SCXMLEVENT_H
#define SCXMLEVENT_H

#include <QtScxml/scxmlglobals.h>

#include <QEvent>
#include <QStringList>
#include <QVariantList>

QT_BEGIN_NAMESPACE

namespace Scxml {
namespace Internal {
class MyQStateMachine;
}
}

class QScxmlEventPrivate;

class Q_SCXML_EXPORT QScxmlEvent: public QEvent
{
public:
    QScxmlEvent();
    ~QScxmlEvent();

    QScxmlEvent &operator=(const QScxmlEvent &other);
    QScxmlEvent(const QScxmlEvent &other);

    enum EventType {
        PlatformEvent,
        InternalEvent,
        ExternalEvent
    };

    QByteArray name() const;
    void setName(const QByteArray &name);

    EventType eventType() const;
    void setEventType(const EventType &type);

    QString scxmlType() const;

    QByteArray sendId() const;
    void setSendId(const QByteArray &sendId);

    QString origin() const;
    void setOrigin(const QString &origin);

    QString originType() const;
    void setOriginType(const QString &originType);

    QString invokeId() const;
    void setInvokeId(const QString &invokeId);

    QVariantList dataValues() const;
    void setDataValues(const QVariantList &dataValues);

    QStringList dataNames() const;
    void setDataNames(const QStringList &dataNames);

    void reset(const QByteArray &name, EventType eventType = ExternalEvent,
               QVariantList dataValues = QVariantList(), const QByteArray &sendId = QByteArray(),
               const QString &origin = QString(), const QString &originType = QString(),
               const QString &invokeId = QString());
    void clear();

    QVariant data() const;

protected:
    friend Scxml::Internal::MyQStateMachine;
    static QEvent::Type scxmlEventType;
    static QEvent::Type ignoreEventType;
    void makeIgnorable();

private:
    QScxmlEventPrivate *d;

};

QT_END_NAMESPACE

#endif // SCXMLEVENT_H
