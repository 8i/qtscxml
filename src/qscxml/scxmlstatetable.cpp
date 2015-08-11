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

#include "scxmlstatetable_p.h"
#include "executablecontent_p.h"
#include "scxmlevent_p.h"
#include "scxmlqstates.h"

#include <QAbstractState>
#include <QAbstractTransition>
#include <QFile>
#include <QHash>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QState>
#include <QString>
#include <QTimer>

#include <QtCore/private/qstatemachine_p.h>

#undef DUMP_EVENT
#ifdef DUMP_EVENT
#include "ecmascriptdatamodel.h"
#endif

namespace Scxml {
Q_LOGGING_CATEGORY(scxmlLog, "scxml.table")

QEvent::Type ScxmlEvent::scxmlEventType = (QEvent::Type)QEvent::registerEventType();

namespace {
QByteArray objectId(QObject *obj, bool strict = false)
{
    Q_UNUSED(obj);
    Q_UNUSED(strict);
    Q_UNIMPLEMENTED();
    return QByteArray();
}
} // anonymous namespace

namespace Internal {
class MyQStateMachinePrivate: public QStateMachinePrivate
{
    Q_DECLARE_PUBLIC(MyQStateMachine)

public:
    MyQStateMachinePrivate(StateTable *table)
        : m_table(table)
        , m_queuedEvents(Q_NULLPTR)
    {}
    ~MyQStateMachinePrivate()
    { delete m_queuedEvents; }

    int eventIdForDelayedEvent(const QByteArray &scxmlEventId);

protected: // overrides for QStateMachinePrivate:
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    void noMicrostep() Q_DECL_OVERRIDE;
    void processedPendingEvents(bool didChange) Q_DECL_OVERRIDE;
    void beginMacrostep() Q_DECL_OVERRIDE;
    void endMacrostep(bool didChange) Q_DECL_OVERRIDE;

    void emitStateFinished(QState *forState, QFinalState *guiltyState) Q_DECL_OVERRIDE;
    void startupHook() Q_DECL_OVERRIDE;
#endif

public: // fields
    StateTable *m_table;

    struct QueuedEvent
    {
        QueuedEvent(QEvent *event = Q_NULLPTR, QStateMachine::EventPriority priority = QStateMachine::NormalPriority)
            : event(event)
            , priority(priority)
        {}

        QEvent *event;
        QStateMachine::EventPriority priority;
    };
    QVector<QueuedEvent> *m_queuedEvents;
};

MyQStateMachine::MyQStateMachine(StateTable *parent)
    : QStateMachine(*new MyQStateMachinePrivate(parent), parent)
{}

MyQStateMachine::MyQStateMachine(MyQStateMachinePrivate &dd, StateTable *parent)
    : QStateMachine(dd, parent)
{}

StateTable *MyQStateMachine::stateTable() const
{
    Q_D(const MyQStateMachine);

    return d->m_table;
}

StateTablePrivate *MyQStateMachine::stateTablePrivate()
{
    Q_D(MyQStateMachine);

    return StateTablePrivate::get(d->m_table);
}
} // Internal namespace

class ScxmlError::ScxmlErrorPrivate
{
public:
    ScxmlErrorPrivate()
        : line(-1)
        , column(-1)
    {}

    QString fileName;
    int line;
    int column;
    QString description;
};

ScxmlError::ScxmlError()
    : d(Q_NULLPTR)
{}

ScxmlError::ScxmlError(const QString &fileName, int line, int column, const QString &description)
    : d(new ScxmlErrorPrivate)
{
    d->fileName = fileName;
    d->line = line;
    d->column = column;
    d->description = description;
}

ScxmlError::ScxmlError(const ScxmlError &other)
    : d(Q_NULLPTR)
{
    *this = other;
}

ScxmlError &ScxmlError::operator=(const ScxmlError &other)
{
    if (other.d) {
        if (!d)
            d = new ScxmlErrorPrivate;
        d->fileName = other.d->fileName;
        d->line = other.d->line;
        d->column = other.d->column;
        d->description = other.d->description;
    } else {
        delete d;
        d = Q_NULLPTR;
    }
    return *this;
}

ScxmlError::~ScxmlError()
{
    delete d;
    d = Q_NULLPTR;
}

bool ScxmlError::isValid() const
{
    return d != Q_NULLPTR;
}

QString ScxmlError::fileName() const
{
    return isValid() ? d->fileName : QString();
}

int ScxmlError::line() const
{
    return isValid() ? d->line : -1;
}

int ScxmlError::column() const
{
    return isValid() ? d->column : -1;
}

QString ScxmlError::description() const
{
    return isValid() ? d->description : QString();
}

QString ScxmlError::toString() const
{
    QString str;
    if (!isValid())
        return str;

    if (d->fileName.isEmpty())
        str = QStringLiteral("<Unknown File>");
    else
        str = d->fileName;
    if (d->line != -1) {
        str += QStringLiteral(":%1").arg(d->line);
        if (d->column != -1)
            str += QStringLiteral(":%1").arg(d->column);
    }
    str += QStringLiteral(": ") + d->description;

    return str;
}

QDebug operator<<(QDebug debug, const ScxmlError &error)
{
    debug << error.toString();
    return debug;
}

TableData::~TableData()
{}

QAtomicInt StateTablePrivate::m_sessionIdCounter = QAtomicInt(0);

StateTablePrivate::StateTablePrivate()
    : QObjectPrivate()
    , m_sessionId(m_sessionIdCounter++)
    , m_dataModel(Q_NULLPTR)
    , m_dataBinding(StateTable::EarlyBinding)
    , m_executionEngine(Q_NULLPTR)
    , m_tableData(Q_NULLPTR)
    , m_qStateMachine(Q_NULLPTR)
{}

StateTablePrivate::~StateTablePrivate()
{
    delete m_executionEngine;
}

void StateTablePrivate::setQStateMachine(Internal::MyQStateMachine *stateMachine)
{
    m_qStateMachine = stateMachine;
}

StateTablePrivate::ParserData *StateTablePrivate::parserData()
{
    if (m_parserData.isNull())
        m_parserData.reset(new ParserData);
    return m_parserData.data();
}

StateTable *StateTable::fromFile(const QString &fileName, DataModel *dataModel)
{
    QFile scxmlFile(fileName);
    if (!scxmlFile.open(QIODevice::ReadOnly)) {
        QVector<ScxmlError> errors({
                                ScxmlError(scxmlFile.fileName(), 0, 0, QStringLiteral("cannot open for reading"))
                            });
        auto table = new StateTable;
        StateTablePrivate::get(table)->parserData()->m_errors = errors;
        return table;
    }

    StateTable *table = fromData(&scxmlFile, dataModel);
    scxmlFile.close();
    return table;
}

StateTable *StateTable::fromData(QIODevice *data, DataModel *dataModel)
{
    QXmlStreamReader xmlReader(data);
    Scxml::ScxmlParser parser(&xmlReader);
    parser.parse();
    auto table = parser.instantiateStateMachine();
    if (dataModel) {
        table->setDataModel(dataModel);
    } else {
        parser.instantiateDataModel(table);
    }
    return table;
}

QVector<ScxmlError> StateTable::errors() const
{
    Q_D(const StateTable);
    return d->m_parserData ? d->m_parserData->m_errors : QVector<ScxmlError>();
}

StateTable::StateTable(QObject *parent)
    : QObject(*new StateTablePrivate, parent)
{
    Q_D(StateTable);
    d->m_executionEngine = new ExecutableContent::ExecutionEngine(this);
    d->setQStateMachine(new Internal::MyQStateMachine(this));
    connect(d->m_qStateMachine, &QStateMachine::finished, this, &StateTable::onFinished);
    connect(d->m_qStateMachine, &QStateMachine::finished, this, &StateTable::finished);
}

StateTable::StateTable(StateTablePrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    Q_D(StateTable);
    d->m_executionEngine = new ExecutableContent::ExecutionEngine(this);
    connect(d->m_qStateMachine, &QStateMachine::finished, this, &StateTable::onFinished);
    connect(d->m_qStateMachine, &QStateMachine::finished, this, &StateTable::finished);
}

QStateMachine *StateTable::qStateMachine() const
{
    Q_D(const StateTable);

    return d->m_qStateMachine;
}

int StateTable::sessionId() const
{
    Q_D(const StateTable);

    return d->m_sessionId;
}

DataModel *StateTable::dataModel() const
{
    Q_D(const StateTable);

    return d->m_dataModel;
}

void StateTable::setDataModel(DataModel *dataModel)
{
    Q_D(StateTable);

    d->m_dataModel = dataModel;
    dataModel->setTable(this);
}

void StateTable::setDataBinding(StateTable::BindingMethod b)
{
    Q_D(StateTable);

    d->m_dataBinding = b;
}

StateTable::BindingMethod StateTable::dataBinding() const
{
    Q_D(const StateTable);

    return d->m_dataBinding;
}

ExecutableContent::ExecutionEngine *StateTable::executionEngine() const
{
    Q_D(const StateTable);

    return d->m_executionEngine;
}

TableData *StateTable::tableData() const
{
    Q_D(const StateTable);

    return d->m_tableData;
}

void StateTable::setTableData(TableData *tableData)
{
    Q_D(StateTable);

    d->m_tableData = tableData;
}

void StateTable::doLog(const QString &label, const QString &msg)
{
    qCDebug(scxmlLog) << label << ":" << msg;
    emit log(label, msg);
}

void Internal::MyQStateMachine::beginSelectTransitions(QEvent *event)
{
    Q_D(MyQStateMachine);

    if (event && event->type() != QEvent::None) {
        switch (event->type()) {
        case QEvent::StateMachineSignal: {
            QStateMachine::SignalEvent* e = (QStateMachine::SignalEvent*)event;
            QByteArray signalName = e->sender()->metaObject()->method(e->signalIndex()).methodSignature();
            //signalName.replace(signalName.indexOf('('), 1, QLatin1Char('.'));
            //signalName.chop(1);
            //if (signalName.endsWith(QLatin1Char('.')))
            //    signalName.chop(1);
            ScxmlEvent::EventType eventType = ScxmlEvent::External;
            QObject *s = e->sender();
            if (s == this) {
                if (signalName.startsWith(QByteArray("event_"))){
                    stateTablePrivate()->m_event.reset(signalName.mid(6), eventType, e->arguments());
                    break;
                } else {
                    qCWarning(scxmlLog) << "Unexpected signal event sent to StateMachine "
                                        << d->m_table->tableData()->name() << ":" << signalName;
                }
            }
            QByteArray senderName = QByteArray("@0");
            if (s) {
                senderName = objectId(s, true);
                if (senderName.isEmpty() && !s->objectName().isEmpty())
                    senderName = s->objectName().toUtf8();
            }
            QList<QByteArray> namePieces;
            namePieces << QByteArray("qsignal") << senderName << signalName;
            QByteArray eventName = namePieces.join('.');
            stateTablePrivate()->m_event.reset(eventName, eventType, e->arguments());
        } break;
        case QEvent::StateMachineWrapped: {
            QStateMachine::WrappedEvent * e = (QStateMachine::WrappedEvent *)event;
            QObject *s = e->object();
            QByteArray senderName = QByteArray("@0");
            if (s) {
                senderName = objectId(s, true);
                if (senderName.isEmpty() && !s->objectName().isEmpty())
                    senderName = s->objectName().toUtf8();
            }
            QEvent::Type qeventType = e->event()->type();
            QByteArray eventName;
            QMetaObject metaObject = QEvent::staticMetaObject;
            int maxIenum = metaObject.enumeratorCount();
            for (int ienum = metaObject.enumeratorOffset(); ienum < maxIenum; ++ienum) {
                QMetaEnum en = metaObject.enumerator(ienum);
                if (QByteArray(en.name()) == QByteArray("Type")) {
                    eventName = QByteArray(en.valueToKey(qeventType));
                    break;
                }
            }
            if (eventName.isEmpty())
                eventName = QStringLiteral("E%1").arg((int)qeventType).toUtf8();
            QList<QByteArray> namePieces;
            namePieces << QByteArray("qevent") << senderName << eventName;
            QByteArray name = namePieces.join('.');
            ScxmlEvent::EventType eventType = ScxmlEvent::External;
            // use e->spontaneous(); to choose internal/external?
            stateTablePrivate()->m_event.reset(name, eventType); // put something more in data for some elements like keyEvents and mouseEvents?
        } break;
        default:
            if (event->type() == ScxmlEvent::scxmlEventType) {
                stateTablePrivate()->m_event = *static_cast<ScxmlEvent *>(event);
            } else {
                QEvent::Type qeventType = event->type();
                QByteArray eventName = QStringLiteral("qdirectevent.E%1").arg((int)qeventType).toUtf8();
                stateTablePrivate()->m_event.reset(eventName);
                qCWarning(scxmlLog) << "Unexpected event directly sent to StateMachine "
                                    << d->m_table->tableData()->name() << ":" << event->type();
            }
            break;
        }
    } else {
        stateTablePrivate()->m_event.clear();
    }

    d->m_table->dataModel()->setEvent(stateTablePrivate()->m_event);
}

void Internal::MyQStateMachine::beginMicrostep(QEvent *event)
{
    Q_D(MyQStateMachine);

    qCDebug(scxmlLog) << d->m_table->tableData()->name() << " started microstep from state" << d->m_table->activeStates()
                      << "with event" << stateTablePrivate()->m_event.name() << "from event type" << event->type();
}

void Internal::MyQStateMachine::endMicrostep(QEvent *event)
{
    Q_D(MyQStateMachine);
    Q_UNUSED(event);

    qCDebug(scxmlLog) << d->m_table->tableData()->name() << " finished microstep in state (" << d->m_table->activeStates() << ")";
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)

void Internal::MyQStateMachinePrivate::noMicrostep()
{
    qCDebug(scxmlLog) << m_table->tableData()->name() << " had no transition, stays in state (" << m_table->activeStates() << ")";
}

void Internal::MyQStateMachinePrivate::processedPendingEvents(bool didChange)
{
    qCDebug(scxmlLog) << m_table->tableData()->name() << " finishedPendingEvents " << didChange << " in state ("
                      << m_table->activeStates() << ")";
    emit m_table->reachedStableState(didChange);
}

void Internal::MyQStateMachinePrivate::beginMacrostep()
{
}

void Internal::MyQStateMachinePrivate::endMacrostep(bool didChange)
{
    qCDebug(scxmlLog) << m_table->tableData()->name() << " endMacrostep " << didChange << " in state ("
                      << m_table->activeStates() << ")";
}

void Internal::MyQStateMachinePrivate::emitStateFinished(QState *forState, QFinalState *guiltyState)
{
    Q_Q(MyQStateMachine);

    if (ScxmlFinalState *finalState = qobject_cast<ScxmlFinalState *>(guiltyState)) {
        if (!q->isRunning())
            return;
        m_table->executionEngine()->execute(finalState->doneData(), forState->objectName());
    }

    QStateMachinePrivate::emitStateFinished(forState, guiltyState);
}

void Internal::MyQStateMachinePrivate::startupHook()
{
    Q_Q(MyQStateMachine);

    q->submitQueuedEvents();
}

#endif // QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)

int Internal::MyQStateMachinePrivate::eventIdForDelayedEvent(const QByteArray &scxmlEventId)
{
    QMutexLocker locker(&delayedEventsMutex);

    QHash<int, DelayedEvent>::const_iterator it;
    for (it = delayedEvents.constBegin(); it != delayedEvents.constEnd(); ++it) {
        if (ScxmlEvent *e = dynamic_cast<ScxmlEvent *>(it->event)) {
            if (e->sendid() == scxmlEventId) {
                return it.key();
            }
        }
    }

    return -1;
}

QStringList StateTable::activeStates(bool compress)
{
    Q_D(StateTable);

    QSet<QAbstractState *> config = QStateMachinePrivate::get(d->m_qStateMachine)->configuration;
    if (compress)
        foreach (const QAbstractState *s, config)
            config.remove(s->parentState());
    QStringList res;
    foreach (const QAbstractState *s, config) {
        QString id = s->objectName();
        if (!id.isEmpty()) {
            res.append(id);
        }
    }
    std::sort(res.begin(), res.end());
    return res;
}

bool StateTable::isActive(const QString &scxmlStateName) const
{
    Q_D(const StateTable);
    QSet<QAbstractState *> config = QStateMachinePrivate::get(d->m_qStateMachine)->configuration;
    foreach (QAbstractState *s, config) {
        if (s->objectName() == scxmlStateName) {
            return true;
        }
    }
    return false;
}

static ScxmlState *findState(const QString &scxmlName, QStateMachine *parent)
{
    QList<QObject *> worklist;
    worklist.reserve(parent->children().size() + parent->configuration().size());
    worklist.append(parent);

    while (!worklist.isEmpty()) {
        QObject *obj = worklist.takeLast();
        if (ScxmlState *state = qobject_cast<ScxmlState *>(obj)) {
            if (state->objectName() == scxmlName)
                return state;
        }
        worklist.append(obj->children());
    }

    return Q_NULLPTR;
}

bool StateTable::hasState(const QString &scxmlStateName) const
{
    Q_D(const StateTable);
    return findState(scxmlStateName, d->m_qStateMachine) != Q_NULLPTR;
}

QMetaObject::Connection StateTable::connect(const QString &scxmlStateName, const char *signal,
                                            const QObject *receiver, const char *method,
                                            Qt::ConnectionType type)
{
    Q_D(StateTable);
    ScxmlState *state = findState(scxmlStateName, d->m_qStateMachine);
    return QObject::connect(state, signal, receiver, method, type);
}

void StateTable::executeInitialSetup()
{
    executionEngine()->execute(tableData()->initialSetup());
}

static bool loopOnSubStates(QState *startState,
                            std::function<bool(QState *)> enteringState = Q_NULLPTR,
                            std::function<void(QState *)> exitingState = Q_NULLPTR,
                            std::function<void(QAbstractState *)> inAbstractState = Q_NULLPTR)
{
    QList<int> pos;
    QState *parentAtt = startState;
    QObjectList childs = startState->children();
    pos << 0;
    while (!pos.isEmpty()) {
        bool goingDeeper = false;
        for (int i = pos.last(); i < childs.size() ; ++i) {
            if (QAbstractState *as = qobject_cast<QAbstractState *>(childs.at(i))) {
                if (QState *s = qobject_cast<QState *>(as)) {
                    if (enteringState && !enteringState(s))
                        continue;
                    pos.last() = i + 1;
                    parentAtt = s;
                    childs = s->children();
                    pos << 0;
                    goingDeeper = !childs.isEmpty();
                    break;
                } else if (inAbstractState) {
                    inAbstractState(as);
                }
            }
        }
        if (!goingDeeper) {
            do {
                pos.removeLast();
                if (pos.isEmpty())
                    break;
                if (exitingState)
                    exitingState(parentAtt);
                parentAtt = parentAtt->parentState();
                childs = parentAtt->children();
            } while (!pos.isEmpty() && pos.last() >= childs.size());
        }
    }
    return true;
}

bool StateTable::init()
{
    Q_D(StateTable);

    dataModel()->setup();
    executeInitialSetup();

    bool res = true;
    loopOnSubStates(d->m_qStateMachine, std::function<bool(QState *)>(), [&res](QState *state) {
        if (ScxmlState *s = qobject_cast<ScxmlState *>(state))
            if (!s->init())
                res = false;
        if (ScxmlFinalState *s = qobject_cast<ScxmlFinalState *>(state))
            if (!s->init())
                res = false;
        foreach (QAbstractTransition *t, state->transitions()) {
            if (ScxmlTransition *scTransition = qobject_cast<ScxmlTransition *>(t))
                if (!scTransition->init())
                    res = false;
        }
    });
    foreach (QAbstractTransition *t, d->m_qStateMachine->transitions()) {
        if (ScxmlTransition *scTransition = qobject_cast<ScxmlTransition *>(t))
            if (!scTransition->init())
                res = false;
    }
    return res;
}

bool StateTable::isRunning() const
{
    Q_D(const StateTable);

    return d->m_qStateMachine->isRunning();
}

QString StateTable::name() const
{
    return tableData()->name();
}

void StateTable::submitError(const QByteArray &type, const QString &msg, const QByteArray &sendid)
{
    qCDebug(scxmlLog) << "machine" << name() << "had error" << type << ":" << msg;
    submitEvent(EventBuilder::errorEvent(type, sendid));
}

void StateTable::submitEvent1(const QString &event)
{
    submitEvent(event.toUtf8(), QVariantList());
}

void StateTable::submitEvent2(const QString &event, QVariant data)
{
    QVariantList dataValues;
    if (data.isValid())
        dataValues << data;
    submitEvent(event.toUtf8(), dataValues);
}

void StateTable::submitEvent(ScxmlEvent *e)
{
    Q_D(StateTable);

    if (!e)
        return;

    QStateMachine::EventPriority priority =
            e->eventType() == ScxmlEvent::External ? QStateMachine::NormalPriority
                                                   : QStateMachine::HighPriority;

    if (d->m_qStateMachine->isRunning())
        d->m_qStateMachine->postEvent(e, priority);
    else
        d->m_qStateMachine->queueEvent(e, priority);
}

void StateTable::submitEvent(const QByteArray &event, const QVariantList &dataValues,
                             const QStringList &dataNames, ScxmlEvent::EventType type,
                             const QByteArray &sendid, const QString &origin,
                             const QString &origintype, const QByteArray &invokeid)
{
    qCDebug(scxmlLog) << name() << ": submitting event" << event;

    ScxmlEvent *e = new ScxmlEvent(event, type, dataValues, dataNames, sendid, origin, origintype, invokeid);
    submitEvent(e);
}

void StateTable::submitDelayedEvent(int delayInMiliSecs, ScxmlEvent *e)
{
    Q_ASSERT(delayInMiliSecs > 0);
    Q_D(StateTable);

    if (!e)
        return;

    qCDebug(scxmlLog) << name() << ": submitting event" << e->name() << "with delay" << delayInMiliSecs << "ms" << "and sendid" << e->sendid();

    Q_ASSERT(e->eventType() == ScxmlEvent::External);
    int id = d->m_qStateMachine->postDelayedEvent(e, delayInMiliSecs);

    qCDebug(scxmlLog) << name() << ": delayed event" << e->name() << "(" << e << ") got id:" << id;
}

void StateTable::cancelDelayedEvent(const QByteArray &sendid)
{
    Q_D(StateTable);

    int id = d->m_qStateMachine->eventIdForDelayedEvent(sendid);

    qCDebug(scxmlLog) << name() << ": canceling event" << sendid << "with id" << id;

    if (id != -1)
        d->m_qStateMachine->cancelDelayedEvent(id);
}

void Internal::MyQStateMachine::queueEvent(ScxmlEvent *event, EventPriority priority)
{
    Q_D(MyQStateMachine);

    qCDebug(scxmlLog) << d->m_table->name() << ": queueing event" << event->name();

    if (!d->m_queuedEvents)
        d->m_queuedEvents = new QVector<MyQStateMachinePrivate::QueuedEvent>();
    d->m_queuedEvents->append(MyQStateMachinePrivate::QueuedEvent(event, priority));
}

void Internal::MyQStateMachine::submitQueuedEvents()
{
    Q_D(MyQStateMachine);

    qCDebug(scxmlLog) << d->m_table->name() << ": submitting queued events";

    if (d->m_queuedEvents) {
        foreach (const MyQStateMachinePrivate::QueuedEvent &e, *d->m_queuedEvents)
            postEvent(e.event, e.priority);
        delete d->m_queuedEvents;
        d->m_queuedEvents = Q_NULLPTR;
    }
}

int Internal::MyQStateMachine::eventIdForDelayedEvent(const QByteArray &scxmlEventId)
{
    Q_D(MyQStateMachine);
    return d->eventIdForDelayedEvent(scxmlEventId);
}

bool StateTable::isLegalTarget(const QString &target) const
{
    return target.startsWith(QLatin1Char('#'));
}

bool StateTable::isDispatchableTarget(const QString &target) const
{
    return target == QStringLiteral("#_internal")
            || target == QStringLiteral("#_scxml_%1").arg(sessionId());
}

void StateTable::onFinished()
{
    // The final state is also a stable state.
    emit reachedStableState(true);
}

void StateTable::start()
{
    Q_D(StateTable);

    d->m_qStateMachine->start();
}

ScxmlEvent::ScxmlEvent(const QByteArray &name, ScxmlEvent::EventType eventType,
                       const QVariantList &dataValues, const QStringList &dataNames,
                       const QByteArray &sendid, const QString &origin,
                       const QString &origintype, const QByteArray &invokeid)
    : QEvent(scxmlEventType), m_name(name), m_type(eventType), m_dataValues(dataValues), m_dataNames(dataNames)
    , m_sendid(sendid), m_origin(origin), m_origintype(origintype), m_invokeid(invokeid)
{ }

QString ScxmlEvent::scxmlType() const {
    switch (m_type) {
    case Platform:
        return QLatin1String("platform");
    case Internal:
        return QLatin1String("internal");
    case External:
        break;
    }
    return QLatin1String("external");
}

void ScxmlEvent::reset(const QByteArray &name, ScxmlEvent::EventType eventType, QVariantList dataValues,
                       const QByteArray &sendid, const QString &origin,
                       const QString &origintype, const QByteArray &invokeid) {
    m_name = name;
    m_type = eventType;
    m_sendid = sendid;
    m_origin = origin;
    m_origintype = origintype;
    m_invokeid = invokeid;
    m_dataValues = dataValues;
}

void ScxmlEvent::clear() {
    m_name = QByteArray();
    m_type = External;
    m_sendid = QByteArray();
    m_origin = QString();
    m_origintype = QString();
    m_invokeid = QByteArray();
    m_dataValues = QVariantList();
}

} // namespace Scxml
