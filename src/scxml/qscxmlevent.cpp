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

#include "qscxmlexecutablecontent_p.h"
#include "qscxmlevent_p.h"

QT_USE_NAMESPACE

using namespace QScxmlExecutableContent;

QEvent::Type QScxmlEvent::scxmlEventType = (QEvent::Type) QEvent::registerEventType();
QEvent::Type QScxmlEvent::ignoreEventType = (QEvent::Type) QEvent::registerEventType();

static bool evaluate(const QScxmlExecutableContent::Param &param, QScxmlStateMachine *stateMachine,
                     QVariantMap &keyValues)
{
    auto dataModel = stateMachine->dataModel();
    auto tableData = stateMachine->tableData();
    if (param.expr != NoEvaluator) {
        bool success = false;
        auto v = dataModel->evaluateToVariant(param.expr, &success);
        keyValues.insert(tableData->string(param.name), v);
        return success;
    }

    QString loc;
    if (param.location != QScxmlExecutableContent::NoString) {
        loc = tableData->string(param.location);
    }

    if (loc.isEmpty()) {
        return false;
    }

    if (dataModel->hasProperty(loc)) {
        keyValues.insert(tableData->string(param.name), dataModel->property(loc));
        return true;
    } else {
        stateMachine->submitError(QByteArray("error.execution"),
                           QStringLiteral("Error in <param>: %1 is not a valid location")
                           .arg(loc),
                           /*sendid =*/ QByteArray());
        return false;
    }
}

static bool evaluate(const QScxmlExecutableContent::Array<QScxmlExecutableContent::Param> *params,
                     QScxmlStateMachine *stateMachine, QVariantMap &keyValues)
{
    if (!params)
        return true;

    auto paramPtr = params->const_data();
    for (qint32 i = 0; i != params->count; ++i, ++paramPtr) {
        if (!evaluate(*paramPtr, stateMachine, keyValues))
            return false;
    }

    return true;
}

QAtomicInt EventBuilder::idCounter = QAtomicInt(0);

QScxmlEvent *EventBuilder::buildEvent()
{
    auto dataModel = stateMachine ? stateMachine->dataModel() : Q_NULLPTR;
    auto tableData = stateMachine ? stateMachine->tableData() : Q_NULLPTR;

    QByteArray eventName = event;
    bool ok = true;
    if (eventexpr != NoEvaluator) {
        eventName = dataModel->evaluateToString(eventexpr, &ok).toUtf8();
        ok = true; // ignore failure.
    }

    QVariant data;
    if ((!params || params->count == 0) && (!namelist || namelist->count == 0)) {
        if (contentExpr == NoEvaluator) {
            data = contents;
        } else {
            data = dataModel->evaluateToString(contentExpr, &ok);
        }
        if (!ok) {
            // expr evaluation failure results in the data property of the event being set to null. See e.g. test528.
            data = QVariant(QMetaType::VoidStar, 0);
        }
    } else {
        QVariantMap keyValues;
        if (evaluate(params, stateMachine, keyValues)) {
            if (namelist) {
                for (qint32 i = 0; i < namelist->count; ++i) {
                    QString name = tableData->string(namelist->const_data()[i]);
                    keyValues.insert(name, dataModel->property(name));
                }
            }
            data = keyValues;
        } else {
            // If the evaluation of the <param> tags fails, set _event.data to an empty string.
            // See test343.
            data = QVariant(QMetaType::VoidStar, 0);
        }
    }

    QByteArray sendid = id;
    if (!idLocation.isEmpty()) {
        sendid = generateId();
        stateMachine->dataModel()->setProperty(idLocation, QString::fromUtf8(sendid), tableData->string(instructionLocation), &ok);
        if (!ok)
            return Q_NULLPTR;
    }

    QString origin = target;
    if (targetexpr != NoEvaluator) {
        origin = dataModel->evaluateToString(targetexpr, &ok);
        if (!ok)
            return Q_NULLPTR;
    }
    if (origin.isEmpty()) {
        if (eventType == QScxmlEvent::ExternalEvent) {
            origin = QStringLiteral("#_internal");
        }
    } else if (origin == QStringLiteral("#_parent")) {
        // allow sending messages to the parent, independently of whether we're invoked or not.
    } else if (!stateMachine->isLegalTarget(origin)) {
        // [6.2.4] and test194.
        stateMachine->submitError(QByteArray("error.execution"),
                           QStringLiteral("Error in %1: %2 is not a legal target")
                           .arg(tableData->string(instructionLocation), origin),
                           sendid);
        return Q_NULLPTR;
    } else if (!stateMachine->isDispatchableTarget(origin)) {
        // [6.2.4] and test521.
        stateMachine->submitError(QByteArray("error.communication"),
                           QStringLiteral("Error in %1: cannot dispatch to target '%2'")
                           .arg(tableData->string(instructionLocation), origin),
                           sendid);
        return Q_NULLPTR;
    }

    QString origintype = type;
    if (origintype.isEmpty()) {
        // [6.2.5] and test198
        origintype = QStringLiteral("http://www.w3.org/TR/scxml/#SCXMLEventProcessor");
    }
    if (typeexpr != NoEvaluator) {
        origintype = dataModel->evaluateToString(typeexpr, &ok);
        if (!ok)
            return Q_NULLPTR;
    }
    if (!origintype.isEmpty()
            && origintype != QStringLiteral("qt:signal")
            && origintype != QStringLiteral("http://www.w3.org/TR/scxml/#SCXMLEventProcessor")) {
        // [6.2.5] and test199
        stateMachine->submitError(QByteArray("error.execution"),
                           QStringLiteral("Error in %1: %2 is not a valid type")
                           .arg(tableData->string(instructionLocation), origintype),
                           sendid);
        return Q_NULLPTR;
    }

    QString invokeid;
    if (stateMachine && stateMachine->isInvoked()) {
        invokeid = stateMachine->sessionId();
    }

    QScxmlEvent *event = new QScxmlEvent;
    event->setName(eventName);
    event->setEventType(eventType);
    event->setData(data);
    event->setSendId(sendid);
    event->setOrigin(origin);
    event->setOriginType(origintype);
    event->setInvokeId(invokeid);
    return event;
}

QScxmlEvent::QScxmlEvent()
    : QEvent(scxmlEventType), d(new QScxmlEventPrivate)
{ }

QScxmlEvent::~QScxmlEvent()
{
    delete d;
}

QString QScxmlEvent::scxmlType() const
{
    switch (d->eventType) {
    case PlatformEvent:
        return QLatin1String("platform");
    case InternalEvent:
        return QLatin1String("internal");
    case ExternalEvent:
        break;
    }
    return QLatin1String("external");
}

void QScxmlEvent::clear()
{
    d->name = QByteArray();
    d->eventType = ExternalEvent;
    d->sendid = QByteArray();
    d->origin = QString();
    d->originType = QString();
    d->invokeId = QString();
    d->data = QVariantMap();
}

QScxmlEvent &QScxmlEvent::operator=(const QScxmlEvent &other)
{
    QEvent::operator=(other);
    *d = *other.d;
    return *this;
}

QScxmlEvent::QScxmlEvent(const QScxmlEvent &other)
    : QEvent(other), d(new QScxmlEventPrivate(*other.d))
{
}

QByteArray QScxmlEvent::name() const
{
    return d->name;
}

void QScxmlEvent::setName(const QByteArray &name)
{
    d->name = name;
}

QByteArray QScxmlEvent::sendId() const
{
    return d->sendid;
}

void QScxmlEvent::setSendId(const QByteArray &sendid)
{
    d->sendid = sendid;
}

QString QScxmlEvent::origin() const
{
    return d->origin;
}

void QScxmlEvent::setOrigin(const QString &origin)
{
    d->origin = origin;
}

QString QScxmlEvent::originType() const
{
    return d->originType;
}

void QScxmlEvent::setOriginType(const QString &origintype)
{
    d->originType = origintype;
}

QString QScxmlEvent::invokeId() const
{
    return d->invokeId;
}

void QScxmlEvent::setInvokeId(const QString &invokeid)
{
    d->invokeId = invokeid;
}

int QScxmlEvent::delay() const
{
    return d->delayInMiliSecs;
}

void QScxmlEvent::setDelay(int delayInMiliSecs)
{
    d->delayInMiliSecs = delayInMiliSecs;
}

QScxmlEvent::EventType QScxmlEvent::eventType() const
{
    return d->eventType;
}

void QScxmlEvent::setEventType(const EventType &type)
{
    d->eventType = type;
}

QVariant QScxmlEvent::data() const
{
    return d->data;
}

void QScxmlEvent::setData(const QVariant &data)
{
    d->data = data;
}

void QScxmlEvent::makeIgnorable()
{
    t = ignoreEventType;
}
