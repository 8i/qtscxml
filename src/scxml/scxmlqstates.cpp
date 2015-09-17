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

#include "scxmlqstates_p.h"
#include "scxmlstatemachine_p.h"

#define DUMP_EVENT
#ifdef DUMP_EVENT
#include "ecmascriptdatamodel.h"
#endif

QT_BEGIN_NAMESPACE

namespace Scxml {

class ScxmlBaseTransition::Data
{
public:
    QList<QByteArray> eventSelector;
};

ScxmlBaseTransition::ScxmlBaseTransition(QState *sourceState, const QList<QByteArray> &eventSelector)
    : QAbstractTransition(sourceState)
    , d(new Data)
{
    d->eventSelector = eventSelector;
}

ScxmlBaseTransition::ScxmlBaseTransition(QAbstractTransitionPrivate &dd, QState *parent,
                                         const QList<QByteArray> &eventSelector)
    : QAbstractTransition(dd, parent)
    , d(new Data)
{
    d->eventSelector = eventSelector;
}

ScxmlBaseTransition::~ScxmlBaseTransition()
{
    delete d;
}

StateMachine *ScxmlBaseTransition::stateMachine() const {
    if (Internal::MyQStateMachine *t = qobject_cast<Internal::MyQStateMachine *>(parent()))
        return t->stateTable();
    if (QState *s = sourceState())
        return qobject_cast<Internal::MyQStateMachine *>(s->machine())->stateTable();
    qCWarning(scxmlLog) << "could not find Scxml::StateMachine in " << transitionLocation();
    return 0;
}

QString ScxmlBaseTransition::transitionLocation() const {
    if (QState *state = sourceState()) {
        QString stateName = state->objectName();
        int transitionIndex = state->transitions().indexOf(const_cast<ScxmlBaseTransition *>(this));
        return QStringLiteral("transition #%1 in state %2").arg(transitionIndex).arg(stateName);
    }
    return QStringLiteral("unbound transition @%1").arg((size_t)(void*)this);
}

bool ScxmlBaseTransition::eventTest(QEvent *event)
{
    if (d->eventSelector.isEmpty())
        return true;
    if (event->type() == QEvent::None)
        return false;
    StateMachine *stateTable = stateMachine();
    Q_ASSERT(stateTable);
    QByteArray eventName = StateMachinePrivate::get(stateTable)->m_event.name();
    bool selected = false;
    foreach (QByteArray eventStr, d->eventSelector) {
        if (eventStr == "*") {
            selected = true;
            break;
        }
        if (eventStr.endsWith(".*"))
            eventStr.chop(2);
        if (eventName.startsWith(eventStr)) {
            char nextC = '.';
            if (eventName.size() > eventStr.size())
                nextC = eventName.at(eventStr.size());
            if (nextC == '.' || nextC == '(') {
                selected = true;
                break;
            }
        }
    }
    return selected;
}

bool ScxmlBaseTransition::clear()
{
    return true;
}

bool ScxmlBaseTransition::init()
{
    return true;
}

void ScxmlBaseTransition::onTransition(QEvent *event)
{
    Q_UNUSED(event);
}

/////////////

static QList<QByteArray> filterEmpty(const QList<QByteArray> &events) {
    QList<QByteArray> res;
    int oldI = 0;
    for (int i = 0; i < events.size(); ++i) {
        if (events.at(i).isEmpty()) {
            res.append(events.mid(oldI, i - oldI));
            oldI = i + 1;
        }
    }
    if (oldI > 0) {
        res.append(events.mid(oldI));
        return res;
    }
    return events;
}

class ScxmlTransition::Data
{
public:
    Data()
        : conditionalExp(NoEvaluator)
        , instructionsOnTransition(QScxmlExecutableContent::NoInstruction)
    {}

    EvaluatorId conditionalExp;
    QScxmlExecutableContent::ContainerId instructionsOnTransition;
};

ScxmlTransition::ScxmlTransition(QState *sourceState, const QList<QByteArray> &eventSelector)
    : ScxmlBaseTransition(sourceState, filterEmpty(eventSelector))
    , d(new Data)
{}

ScxmlTransition::ScxmlTransition(const QList<QByteArray> &eventSelector)
    : ScxmlBaseTransition(Q_NULLPTR, filterEmpty(eventSelector))
    , d(new Data)
{}

ScxmlTransition::~ScxmlTransition()
{
    delete d;
}

void ScxmlTransition::addTransitionTo(ScxmlState *state)
{
    state->addTransition(this);
}

void ScxmlTransition::addTransitionTo(StateMachine *stateMachine)
{
    StateMachinePrivate::get(stateMachine)->m_qStateMachine->addTransition(this);
}

bool ScxmlTransition::eventTest(QEvent *event)
{
#ifdef DUMP_EVENT
    if (auto edm = dynamic_cast<QScxmlEcmaScriptDataModel *>(stateMachine()->dataModel()))
        qCDebug(scxmlLog) << qPrintable(edm->engine()->evaluate(QLatin1String("JSON.stringify(_event)")).toString());
#endif

    if (ScxmlBaseTransition::eventTest(event)) {
        bool ok = true;
        if (d->conditionalExp != NoEvaluator)
            return stateMachine()->dataModel()->evaluateToBool(d->conditionalExp, &ok) && ok;
        return true;
    }

    return false;
}

void ScxmlTransition::onTransition(QEvent *)
{
    StateMachinePrivate::get(stateMachine())->m_executionEngine->execute(d->instructionsOnTransition);
}

StateMachine *ScxmlTransition::stateMachine() const {
    // work around a bug in QStateMachine
    if (Internal::MyQStateMachine *t = qobject_cast<Internal::MyQStateMachine *>(sourceState()))
        return t->stateTable();
    return qobject_cast<Internal::MyQStateMachine *>(machine())->stateTable();
}

void ScxmlTransition::setInstructionsOnTransition(QScxmlExecutableContent::ContainerId instructions)
{
    d->instructionsOnTransition = instructions;
}

void ScxmlTransition::setConditionalExpression(EvaluatorId evaluator)
{
    d->conditionalExp = evaluator;
}

ScxmlState::ScxmlState(QState *parent)
    : QState(parent)
    , d(new ScxmlState::Data(this))
{}

ScxmlState::ScxmlState(StateMachine *parent)
    : QState(StateMachinePrivate::get(parent)->m_qStateMachine)
    , d(new ScxmlState::Data(this))
{}

ScxmlState::~ScxmlState()
{
    delete d;
}

void ScxmlState::setAsInitialStateFor(ScxmlState *state)
{
    state->setInitialState(this);
}

void ScxmlState::setAsInitialStateFor(StateMachine *stateMachine)
{
    StateMachinePrivate::get(stateMachine)->m_qStateMachine->setInitialState(this);
}

StateMachine *ScxmlState::stateMachine() const {
    return qobject_cast<Internal::MyQStateMachine *>(machine())->stateTable();
}

bool ScxmlState::init()
{
    return true;
}

QString ScxmlState::stateLocation() const
{
    return QStringLiteral("State %1").arg(objectName());
}

void ScxmlState::setInitInstructions(QScxmlExecutableContent::ContainerId instructions)
{
    d->initInstructions = instructions;
}

void ScxmlState::setOnEntryInstructions(QScxmlExecutableContent::ContainerId instructions)
{
    d->onEntryInstructions = instructions;
}

void ScxmlState::setOnExitInstructions(QScxmlExecutableContent::ContainerId instructions)
{
    d->onExitInstructions = instructions;
}

void ScxmlState::setInvokableServiceFactories(const QVector<ScxmlInvokableServiceFactory *> &factories)
{
    d->invokableServiceFactories = factories;
}

void ScxmlState::onEntry(QEvent *event)
{
    if (d->initInstructions != QScxmlExecutableContent::NoInstruction) {
        StateMachinePrivate::get(stateMachine())->m_executionEngine->execute(d->initInstructions);
        d->initInstructions = QScxmlExecutableContent::NoInstruction;
    }
    QState::onEntry(event);
    auto sm = stateMachine();
    StateMachinePrivate::get(sm)->m_executionEngine->execute(d->onEntryInstructions);
    foreach (ScxmlInvokableServiceFactory *serviceFactory, d->invokableServiceFactories) {
        if (ScxmlInvokableService *service = serviceFactory->invoke(sm)) {
            d->invokedServices.append(service);
            sm->registerService(service);
        }
    }
    emit didEnter();
}

void ScxmlState::onExit(QEvent *event)
{
    emit willExit();
    auto sm = stateMachine();
    StateMachinePrivate::get(sm)->m_executionEngine->execute(d->onExitInstructions);
    QState::onExit(event);
    foreach (ScxmlInvokableService *service, d->invokedServices) {
        sm->unregisterService(service);
        delete service;
    }
    d->invokedServices.clear();
}

ScxmlInitialState::ScxmlInitialState(QState *theParent)
    : ScxmlState(theParent)
{}

ScxmlFinalState::ScxmlFinalState(QState *parent)
    : QFinalState(parent)
    , d(new Data)
{}

ScxmlFinalState::ScxmlFinalState(StateMachine *parent)
    : QFinalState(StateMachinePrivate::get(parent)->m_qStateMachine)
    , d(new Data)
{}

ScxmlFinalState::~ScxmlFinalState()
{
    delete d;
}

void ScxmlFinalState::setAsInitialStateFor(ScxmlState *state)
{
    state->setInitialState(this);
}

void ScxmlFinalState::setAsInitialStateFor(StateMachine *stateMachine)
{
    StateMachinePrivate::get(stateMachine)->m_qStateMachine->setInitialState(this);
}

StateMachine *ScxmlFinalState::stateMachine() const {
    return qobject_cast<Internal::MyQStateMachine *>(machine())->stateTable();
}

bool ScxmlFinalState::init()
{
    return true;
}

Scxml::QScxmlExecutableContent::ContainerId ScxmlFinalState::doneData() const
{
    return d->doneData;
}

void ScxmlFinalState::setDoneData(Scxml::QScxmlExecutableContent::ContainerId doneData)
{
    d->doneData = doneData;
}

void ScxmlFinalState::setOnEntryInstructions(QScxmlExecutableContent::ContainerId instructions)
{
    d->onEntryInstructions = instructions;
}

void ScxmlFinalState::setOnExitInstructions(QScxmlExecutableContent::ContainerId instructions)
{
    d->onExitInstructions = instructions;
}

void ScxmlFinalState::onEntry(QEvent *event)
{
    QFinalState::onEntry(event);
    auto smp = StateMachinePrivate::get(stateMachine());
    smp->m_executionEngine->execute(d->onEntryInstructions);
}

void ScxmlFinalState::onExit(QEvent *event)
{
    QFinalState::onExit(event);
    StateMachinePrivate::get(stateMachine())->m_executionEngine->execute(d->onExitInstructions);
}

} // Scxml namespace

QT_END_NAMESPACE
