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

#include <QtScxml/private/executablecontent_p.h>
#include "scxmlcppdumper.h"

#include <algorithm>
#include <QFileInfo>

QT_BEGIN_NAMESPACE
namespace Scxml {

struct StringListDumper {
    StringListDumper &operator <<(const QString &s) {
        text.append(s);
        return *this;
    }

    StringListDumper &operator <<(const QLatin1String &s) {
        text.append(s);
        return *this;
    }
    StringListDumper &operator <<(const char *s) {
        text.append(QLatin1String(s));
        return *this;
    }
    StringListDumper &operator <<(int i) {
        text.append(QString::number(i));
        return *this;
    }
    StringListDumper &operator <<(const QByteArray &s) {
        text.append(QString::fromUtf8(s));
        return *this;
    }

    bool isEmpty() const {
        return text.isEmpty();
    }

    void write(QTextStream &out, const QString &prefix, const QString &suffix, const QString &mainClassName = QString()) const
    {
        foreach (QString line, text) {
            if (!mainClassName.isEmpty() && line.contains(QStringLiteral("%"))) {
                line = line.arg(mainClassName);
            }
            out << prefix << line << suffix;
        }
    }

    void unique()
    {
        text.sort();
        text.removeDuplicates();
    }

    QStringList text;
};

struct Method {
    StringListDumper initializer;
    Method(const QString &decl = QString()): decl(decl) {}
    Method(const StringListDumper &impl): impl(impl) {}
    QString decl; // void f(int i = 0);
    StringListDumper impl; // void f(int i) { m_i = ++i; }
};

struct ClassDump {
    StringListDumper implIncludes;
    QString className;
    QString dataModelClassName;
    StringListDumper classFields;
    StringListDumper tables;
    Method init;
    Method initDataModel;
    StringListDumper dataMethods;
    Method constructor;
    Method destructor;
    StringListDumper properties;
    StringListDumper signalMethods;
    QList<Method> publicMethods;
    StringListDumper publicSlotDeclarations;
    StringListDumper publicSlotDefinitions;

    QList<Method> dataModelMethods;
};

namespace {

QString cEscape(const QString &str)
{
    QString res;
    int lastI = 0;
    for (int i = 0; i < str.length(); ++i) {
        QChar c = str.at(i);
        if (c < QLatin1Char(' ') || c == QLatin1Char('\\') || c == QLatin1Char('\"')) {
            res.append(str.mid(lastI, i - lastI));
            lastI = i + 1;
            if (c == QLatin1Char('\\')) {
                res.append(QLatin1String("\\\\"));
            } else if (c == QLatin1Char('\"')) {
                res.append(QLatin1String("\\\""));
            } else if (c == QLatin1Char('\n')) {
                res.append(QLatin1String("\\n"));
            } else if (c == QLatin1Char('\r')) {
                res.append(QLatin1String("\\r"));
            } else {
                char buf[6];
                ushort cc = c.unicode();
                buf[0] = '\\';
                buf[1] = 'u';
                for (int i = 0; i < 4; ++i) {
                    ushort ccc = cc & 0xF;
                    buf[5 - i] = ccc <= 9 ? '0' + ccc : 'a' + ccc - 10;
                    cc >>= 4;
                }
                res.append(QLatin1String(&buf[0], 6));
            }
        }
    }
    if (lastI != 0) {
        res.append(str.mid(lastI));
        return res;
    }
    return str;
}

static QString toHex(const QString &str)
{
    QString res;
    for (int i = 0, ei = str.length(); i != ei; ++i) {
        int ch = str.at(i).unicode();
        if (ch < 256) {
            res += QStringLiteral("\\x%1").arg(ch, 2, 16, QLatin1Char('0'));
        } else {
            res += QStringLiteral("\\x%1").arg(ch, 4, 16, QLatin1Char('0'));
        }
    }
    return res;
}

const char *headerStart =
        "#include <QtScxml/scxmlstatemachine.h>\n"
        "\n";

using namespace DocumentModel;

enum class Evaluator
{
    ToVariant,
    ToString,
    ToBool,
    Assignment,
    Foreach,
    Script
};

class DumperVisitor: public QScxmlExecutableContent::Builder
{
    Q_DISABLE_COPY(DumperVisitor)

public:
    DumperVisitor(ClassDump &clazz, TranslationUnit *tu)
        : clazz(clazz)
        , translationUnit(tu)
        , m_bindLate(false)
    {}

    void process(ScxmlDocument *doc)
    {
        Q_ASSERT(doc);

        clazz.className = translationUnit->classnameForDocument.value(doc);

        doc->root->accept(this);

        addEvents();

        generateTables();
    }

    ~DumperVisitor()
    {
        Q_ASSERT(m_parents.isEmpty());
    }

protected:
    using NodeVisitor::visit;

    bool visit(Scxml *node) Q_DECL_OVERRIDE
    {
        // init:
        if (!node->name.isEmpty()) {
            clazz.dataMethods << QStringLiteral("QString name() const Q_DECL_OVERRIDE")
                              << QStringLiteral("{ return string(%1); }").arg(addString(node->name))
                              << QString();
            clazz.init.impl << QStringLiteral("stateMachine.setObjectName(string(%1));").arg(addString(node->name));
        } else {
            clazz.dataMethods << QStringLiteral("QString name() const Q_DECL_OVERRIDE")
                              << QStringLiteral("{ return QString(); }")
                              << QString();
        }
        if (node->dataModel == Scxml::CppDataModel) {
            setIsCppDataModel(true);
            clazz.dataModelClassName = node->cppDataModelClassName;
        } else {
            clazz.init.impl << QStringLiteral("stateMachine.setDataModel(&dataModel);");
        }

        QString binding;
        switch (node->binding) {
        case DocumentModel::Scxml::EarlyBinding:
            binding = QStringLiteral("Early");
            break;
        case DocumentModel::Scxml::LateBinding:
            binding = QStringLiteral("Late");
            m_bindLate = true;
            break;
        default:
            Q_UNREACHABLE();
        }
        clazz.init.impl << QStringLiteral("stateMachine.setDataBinding(Scxml::StateMachine::%1Binding);").arg(binding);

        clazz.implIncludes << QStringLiteral("QtScxml/executablecontent.h");
        clazz.init.impl << QStringLiteral("stateMachine.setTableData(this);");

        foreach (AbstractState *s, node->initialStates) {
            clazz.init.impl << QStringLiteral("state_%1.setAsInitialStateFor(&stateMachine);").arg(mangledName(s));
        }

        // visit the kids:
        m_parents.append(node);
        visit(node->children);
        visit(node->dataElements);

        m_dataElements.append(node->dataElements);
        if (node->script || !m_dataElements.isEmpty() || !node->initialSetup.isEmpty()) {
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlExecutableContent::ContainerId initialSetup() const Q_DECL_OVERRIDE")
                              << QStringLiteral("{ return %1; }").arg(startNewSequence())
                              << QString();
            generate(m_dataElements);
            if (node->script) {
                node->script->accept(this);
            }
            visit(&node->initialSetup);
            endSequence();
        } else {
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlExecutableContent::ContainerId initialSetup() const Q_DECL_OVERRIDE")
                              << QStringLiteral("{ return Scxml::QScxmlExecutableContent::NoInstruction; }")
                              << QString();
        }

        m_parents.removeLast();

        { // the data model:
            switch (node->dataModel) {
            case Scxml::NullDataModel:
                clazz.classFields << QStringLiteral("Scxml::QScxmlNullDataModel dataModel;");
                clazz.implIncludes << QStringLiteral("QtScxml/nulldatamodel.h");
                break;
            case Scxml::JSDataModel:
                clazz.classFields << QStringLiteral("Scxml::QScxmlEcmaScriptDataModel dataModel;");
                clazz.implIncludes << QStringLiteral("QtScxml/ecmascriptdatamodel.h");
                break;
            case Scxml::CppDataModel:
                clazz.implIncludes << node->cppDataModelHeaderName;
                break;
            default:
                Q_UNREACHABLE();
            }
        }
        return false;
    }

    bool visit(State *node) Q_DECL_OVERRIDE
    {
        auto name = mangledName(node);
        auto stateName = QStringLiteral("state_") + name;
        // Property stuff:
        clazz.properties << QStringLiteral("Q_PROPERTY(QAbstractState *%1 READ %1() CONSTANT)").arg(name); // FIXME: notify!
        Method getter(QStringLiteral("QAbstractState *%1() const").arg(name));
        getter.impl << QStringLiteral("QAbstractState *%2::%1() const").arg(name)
                    << QStringLiteral("{ return &data->%1; }").arg(stateName);
        clazz.publicMethods << getter;

        // Declaration:
        if (node->type == State::Final) {
            clazz.classFields << QStringLiteral("Scxml::QScxmlFinalState ") + stateName + QLatin1Char(';');
        } else {
            clazz.classFields << QStringLiteral("Scxml::QScxmlState ") + stateName + QLatin1Char(';');
        }

        // Initializer:
        clazz.constructor.initializer << generateInitializer(node);

        // init:
        if (!node->id.isEmpty()) {
            clazz.init.impl << stateName + QStringLiteral(".setObjectName(string(%1));").arg(addString(node->id));
        }
        foreach (AbstractState *initialState, node->initialStates) {
            clazz.init.impl << stateName + QStringLiteral(".setInitialState(&state_")
                               + mangledName(initialState) + QStringLiteral(");");
        }
        if (node->type == State::Parallel) {
            clazz.init.impl << stateName + QStringLiteral(".setChildMode(QState::ParallelStates);");
        }

        // visit the kids:
        m_parents.append(node);
        if (!node->dataElements.isEmpty()) {
            if (m_bindLate) {
                clazz.init.impl << stateName + QStringLiteral(".setInitInstructions(%1);").arg(startNewSequence());
                generate(node->dataElements);
                endSequence();
            } else {
                m_dataElements.append(node->dataElements);
            }
        }

        visit(node->children);
        if (!node->onEntry.isEmpty())
            clazz.init.impl << stateName + QStringLiteral(".setOnEntryInstructions(%1);").arg(generate(node->onEntry));
        if (!node->onExit.isEmpty())
            clazz.init.impl << stateName + QStringLiteral(".setOnExitInstructions(%1);").arg(generate(node->onExit));
        if (!node->invokes.isEmpty()) {
            clazz.init.impl << stateName + QStringLiteral(".setInvokableServiceFactories({");
            for (int i = 0, ei = node->invokes.size(); i != ei; ++i) {
                Invoke *invoke = node->invokes.at(i);
                QString line = QStringLiteral("    new Scxml::InvokeScxmlFactory<%1>(").arg(scxmlClassName(invoke->content.data()));
                line += QStringLiteral("%1, ").arg(Builder::createContext(QStringLiteral("invoke")));
                line += QStringLiteral("%1, ").arg(addString(invoke->id));
                line += QStringLiteral("%1, ").arg(addString(node->id + QStringLiteral(".session-")));
                line += QStringLiteral("%1, ").arg(addString(invoke->idLocation));
                if (invoke->namelist.isEmpty()) {
                    line += QStringLiteral("{}, ");
                } else {
                    line += QStringLiteral("{");
                    bool first = true;
                    foreach (const QString &name, invoke->namelist) {
                        if (first) {
                            first = false;
                        } else {
                            line += QLatin1Char(',');
                        }
                        line += QStringLiteral(" %1").arg(addString(name));
                    }
                    line += QStringLiteral(" }, ");
                }
                line += QStringLiteral("%1, ").arg(invoke->autoforward ? QStringLiteral("true") : QStringLiteral("false"));
                if (invoke->params.isEmpty()) {
                    line += QStringLiteral("{}, ");
                } else {
                    line += QLatin1Char('{');
                    foreach (DocumentModel::Param *param, invoke->params) {
                        line += QStringLiteral("{ %1, %2, %3 }")
                                .arg(addString(param->name))
                                .arg(createEvaluatorVariant(QStringLiteral("param"), QStringLiteral("expr"), param->expr))
                                .arg(addString(param->location));
                    }
                    line += QStringLiteral("}, ");
                }
                if (invoke->finalize.isEmpty()) {
                    line += QStringLiteral("Scxml::QScxmlExecutableContent::NoInstruction");
                } else {
                    line += QString::number(startNewSequence());
                    visit(&invoke->finalize);
                    endSequence();
                }
                line += QLatin1Char(')');
                if (i + 1 != ei)
                    line += QLatin1Char(',');
                clazz.init.impl << line;
            }
            clazz.init.impl << QStringLiteral("});");
        }

        if (node->type == State::Final) {
            auto id = generate(node->doneData);
            clazz.init.impl << stateName + QStringLiteral(".setDoneData(%1);").arg(id);
        }

        m_parents.removeLast();
        return false;
    }

    bool visit(Transition *node) Q_DECL_OVERRIDE
    {
        const QString tName = transitionName(node);
        m_knownEvents.unite(node->events.toSet());

        // Declaration:
        clazz.classFields << QStringLiteral("Scxml::QScxmlTransition ") + tName + QLatin1Char(';');

        // Initializer:
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0) // QTBUG-46703 work-around. See bug report why it's a bad one.
        QString parentName;
        auto parent = m_parents.last();
        if (HistoryState *historyState = parent->asHistoryState()) {
            parentName = QStringLiteral("state_") + mangledName(historyState) + QStringLiteral("_defaultConfiguration");
        } else {
            parentName = parentStateMemberName();
        }
        Q_ASSERT(!parentName.isEmpty());
        QString initializer = tName + QStringLiteral("(&") + parentName + QStringLiteral(", ");
#else
        QString initializer = tName + QStringLiteral("(");
#endif
        QStringList elements;
        foreach (const QString &event, node->events)
            elements.append(qba(event));
        initializer += createList(QStringLiteral("QByteArray"), elements);
        initializer += QStringLiteral(")");
        clazz.constructor.initializer << initializer;

        // init:
        if (node->condition) {
            QString condExpr = *node->condition.data();
            auto cond = createEvaluatorBool(QStringLiteral("transition"), QStringLiteral("cond"), condExpr);
            clazz.init.impl << tName + QStringLiteral(".setConditionalExpression(%1);").arg(cond);
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
        if (m_parents.last()->asHistoryState()) {
            clazz.init.impl << QStringLiteral("%1.setDefaultTransition(&%2);").arg(parentStateMemberName(), tName);
        } else {
            clazz.init.impl << QStringLiteral("%1.addTransitionTo(&%2);").arg(tName, parentStateMemberName());
        }
#else // QTBUG-46703: no default transition for QHistoryState yet...
        clazz.init.impl << parentName + QStringLiteral(".addTransition(&") + tName + QStringLiteral(");");
#endif

        if (node->type == Transition::Internal) {
            clazz.init.impl << tName + QStringLiteral(".setTransitionType(QAbstractTransition::InternalTransition);");
        }
        QStringList targetNames;
        foreach (DocumentModel::AbstractState *s, node->targetStates)
            targetNames.append(QStringLiteral("&state_") + mangledName(s));
        QString targets = tName + QStringLiteral(".setTargetStates(") + createList(QStringLiteral("QAbstractState*"), targetNames);
        clazz.init.impl << targets + QStringLiteral(");");

        // visit the kids:
        if (!node->instructionsOnTransition.isEmpty()) {
            m_parents.append(node);
            m_currentTransitionName = tName;
            clazz.init.impl << tName + QStringLiteral(".setInstructionsOnTransition(%1);").arg(startNewSequence());
            visit(&node->instructionsOnTransition);
            endSequence();
            m_parents.removeLast();
            m_currentTransitionName.clear();
        }
        return false;
    }

    bool visit(DocumentModel::HistoryState *node) Q_DECL_OVERRIDE
    {
        // Includes:
        clazz.implIncludes << "QHistoryState";

        auto stateName = QStringLiteral("state_") + mangledName(node);
        // Declaration:
        clazz.classFields << QStringLiteral("QHistoryState ") + stateName + QLatin1Char(';');

        // Initializer:
        clazz.constructor.initializer << generateInitializer(node);

        // init:
        if (!node->id.isEmpty()) {
            clazz.init.impl << stateName + QStringLiteral(".setObjectName(string(%1));").arg(addString(node->id));
        }
        QString depth;
        switch (node->type) {
        case DocumentModel::HistoryState::Shallow:
            depth = QStringLiteral("Shallow");
            break;
        case DocumentModel::HistoryState::Deep:
            depth = QStringLiteral("Deep");
            break;
        default:
            Q_UNREACHABLE();
        }
        clazz.init.impl << stateName + QStringLiteral(".setHistoryType(QHistoryState::") + depth + QStringLiteral("History);");

        // visit the kid:
        if (Transition *t = node->defaultConfiguration()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0) // work-around for QTBUG-46703
            // Declaration:
            clazz.classFields << QStringLiteral("QState ") + stateName + QStringLiteral("_defaultConfiguration;");

            // Initializer:
            QString init = stateName + QStringLiteral("_defaultConfiguration(&state_");
            if (State *parentState = node->parent->asState()) {
                init += mangledName(parentState);
            }
            clazz.constructor.initializer << init + QLatin1Char(')');

            // init:
            clazz.init.impl << stateName + QStringLiteral(".setDefaultState(&")
                               + stateName + QStringLiteral("_defaultConfiguration);");
#endif

            m_parents.append(node);
            t->accept(this);
            m_parents.removeLast();
        }
        return false;
    }

    bool visit(Send *node) Q_DECL_OVERRIDE
    {
        if (node->type == QStringLiteral("qt:signal")) {
            if (!m_signals.contains(node->event)) {
                m_signals.insert(node->event);
                clazz.signalMethods << QStringLiteral("void event_%1(const QVariant &data);").arg(CppDumper::mangleId(node->event));
            }
        }

        return QScxmlExecutableContent::Builder::visit(node);
    }

private:
    QString mangledName(AbstractState *state)
    {
        Q_ASSERT(state);

        QString name = m_mangledNames.value(state);
        if (!name.isEmpty())
            return name;

        QString id = state->id;
        if (State *s = state->asState()) {
            if (s->type == State::Initial) {
                id = s->parent->asState()->id + QStringLiteral("_initial");
            }
        }

        name = CppDumper::mangleId(id);
        m_mangledNames.insert(state, name);
        return name;
    }

    QString transitionName(Transition *t)
    {
        int idx = 0;
        QString parentName;
        auto parent = m_parents.last();
        if (State *parentState = parent->asState()) {
            parentName = mangledName(parentState);
            idx = childIndex(t, parentState->children);
        } else if (HistoryState *historyState = parent->asHistoryState()) {
            parentName = mangledName(historyState);
        } else if (Scxml *scxml = parent->asScxml()) {
            parentName = QStringLiteral("stateMachine");
            idx = childIndex(t, scxml->children);
        } else {
            Q_UNREACHABLE();
        }
        return QStringLiteral("transition_%1_%2").arg(parentName, QString::number(idx));
    }

    static int childIndex(StateOrTransition *child, const QVector<StateOrTransition *> &children) {
        int idx = 0;
        foreach (StateOrTransition *sot, children) {
            if (sot == child)
                break;
            else
                ++idx;
        }
        return idx;
    }

    QString createList(const QString &elementType, const QStringList &elements) const
    {
        QString result;
        if (translationUnit->useCxx11) {
            if (elements.isEmpty())
                result += QStringLiteral("{}");
            else
                result += QStringLiteral("{ ") + elements.join(QStringLiteral(", ")) + QStringLiteral(" }");
        } else {
            result += QStringLiteral("QList<%1>()").arg(elementType);
            if (!elements.isEmpty())
                result += QStringLiteral(" << ") + elements.join(QStringLiteral(" << "));
        }
        return result;
    }

    QString generateInitializer(AbstractState *node)
    {
        auto stateName = QStringLiteral("state_") + mangledName(node);
        QString init = stateName + QStringLiteral("(");
        if (State *parentState = node->parent->asState()) {
            init += QStringLiteral("&state_") + mangledName(parentState);
        } else {
            init += QStringLiteral("&stateMachine");
        }
        init += QLatin1Char(')');
        return init;
    }

    void addEvents()
    {
        QStringList knownEventsList = m_knownEvents.toList();
        std::sort(knownEventsList.begin(), knownEventsList.end());
        foreach (const QString &event, knownEventsList) {
            if (event.startsWith(QStringLiteral("done.")) || event.startsWith(QStringLiteral("qsignal."))
                    || event.startsWith(QStringLiteral("qevent."))) {
                continue;
            }
            if (event.contains(QLatin1Char('*')))
                continue;

            clazz.publicSlotDeclarations << QStringLiteral("void event_") + CppDumper::mangleId(event) + QStringLiteral("(const QVariant &eventData = QVariant());");
            clazz.publicSlotDefinitions << QStringLiteral("void ") + clazz.className
                                           + QStringLiteral("::event_")
                                           + CppDumper::mangleId(event)
                                           + QStringLiteral("(const QVariant &eventData)\n{ submitEvent(data->") + qba(event)
                                           + QStringLiteral(", eventData); }");
        }

        if (!m_signals.isEmpty()) {
            clazz.init.impl << QStringLiteral("stateMachine.setScxmlEventFilter(this);");
            auto &dm = clazz.dataMethods;
            dm << QStringLiteral("bool handle(QScxmlEvent *event, Scxml::StateMachine *stateMachine) Q_DECL_OVERRIDE {")
               << QStringLiteral("    if (event->originType() != QStringLiteral(\"qt:signal\")) { return true; }")
               << QStringLiteral("    %1 *m = qobject_cast<%1 *>(stateMachine);").arg(clazz.className);
            foreach (const QString &s, m_signals) {
                dm << QStringLiteral("    if (event->name() == %1) { emit m->event_%2(event->data()); return false; }")
                      .arg(qba(s), CppDumper::mangleId(s));
            }
            dm << QStringLiteral("    return true;")
               << QStringLiteral("}")
               << QString();
        }
    }

    QString createContextString(const QString &instrName) const Q_DECL_OVERRIDE
    {
        if (!m_currentTransitionName.isEmpty()) {
            QString state = parentStateName();
            return QStringLiteral("<%1> instruction in transition of state '%2'").arg(instrName, state);
        } else {
            return QStringLiteral("<%1> instruction in state '%2'").arg(instrName, parentStateName());
        }
    }

    QString createContext(const QString &instrName, const QString &attrName, const QString &attrValue) const Q_DECL_OVERRIDE
    {
        QString location = createContextString(instrName);
        return QStringLiteral("%1 with %2=\"%3\"").arg(location, attrName, attrValue);
    }

    QString parentStateName() const
    {
        for (int i = m_parents.size() - 1; i >= 0; --i) {
            Node *node = m_parents.at(i);
            if (State *s = node->asState())
                return s->id;
            else if (HistoryState *h = node->asHistoryState())
                return h->id;
            else if (Scxml *l = node->asScxml())
                return l->name;
        }

        return QString();
    }

    QString parentStateMemberName()
    {
        Node *parent = m_parents.last();
        if (State *s = parent->asState())
            return QStringLiteral("state_") + mangledName(s);
        else if (HistoryState *h = parent->asHistoryState())
            return QStringLiteral("state_") + mangledName(h);
        else if (parent->asScxml())
            return QStringLiteral("stateMachine");
        else
            Q_UNIMPLEMENTED();
        return QString();
    }

    static void generateList(StringListDumper &t, std::function<QString(int)> next)
    {
        const int maxLineLength = 80;
        QString line;
        for (int i = 0; ; ++i) {
            QString nr = next(i);
            if (nr.isNull())
                break;

            if (i != 0)
                line += QLatin1Char(',');

            if (line.length() + nr.length() + 1 > maxLineLength) {
                t << line;
                line.clear();
            } else if (i != 0) {
                line += QLatin1Char(' ');
            }
            line += nr;
        }
        if (!line.isEmpty())
            t << line;
    }

    void generateTables()
    {
        StringListDumper &t = clazz.tables;
        clazz.classFields << QString();
        QScopedPointer<QScxmlExecutableContent::DynamicTableData> td(tableData());

        { // instructions
            clazz.classFields << QStringLiteral("static qint32 theInstructions[];");
            t << QStringLiteral("qint32 %1::Data::theInstructions[] = {").arg(clazz.className);
            auto instr = td->instructionTable();
            generateList(t, [&instr](int idx) -> QString {
                if (instr.isEmpty() && idx == 0) // prevent generation of empty array
                    return QStringLiteral("-1");
                if (idx < instr.size())
                    return QString::number(instr.at(idx));
                else
                    return QString();
            });
            t << QStringLiteral("};") << QStringLiteral("");
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlExecutableContent::Instructions instructions() const Q_DECL_OVERRIDE")
                              << QStringLiteral("{ return theInstructions; }")
                              << QString();
        }

        { // dataIds
            int count;
            auto dataIds = td->dataNames(&count);
            clazz.classFields << QStringLiteral("static Scxml::QScxmlExecutableContent::StringId dataIds[];");
            t << QStringLiteral("Scxml::QScxmlExecutableContent::StringId %1::Data::dataIds[] = {").arg(clazz.className);
            if (isCppDataModel()) {
                t << QStringLiteral("-1");
            } else  {
                generateList(t, [&dataIds, count](int idx) -> QString {
                    if (count == 0 && idx == 0) // prevent generation of empty array
                        return QStringLiteral("-1");
                    if (idx < count)
                        return QString::number(dataIds[idx]);
                    else
                        return QString();
                });
            }
            t << QStringLiteral("};") << QStringLiteral("");
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlExecutableContent::StringId *dataNames(int *count) const Q_DECL_OVERRIDE");
            clazz.dataMethods << QStringLiteral("{ *count = %1; return dataIds; }").arg(count);
            clazz.dataMethods << QString();
        }

        { // evaluators
            auto evaluators = td->evaluators();
            clazz.classFields << QStringLiteral("static Scxml::QScxmlEvaluatorInfo evaluators[];");
            t << QStringLiteral("Scxml::QScxmlEvaluatorInfo %1::Data::evaluators[] = {").arg(clazz.className);
            if (isCppDataModel()) {
                t << QStringLiteral("{ -1, -1 }");
            } else {
                generateList(t, [&evaluators](int idx) -> QString {
                    if (evaluators.isEmpty() && idx == 0) // prevent generation of empty array
                        return QStringLiteral("{ -1, -1 }");
                    if (idx >= evaluators.size())
                        return QString();

                    auto eval = evaluators.at(idx);
                    return QStringLiteral("{ %1, %2 }").arg(eval.expr).arg(eval.context);
                });
            }
            t << QStringLiteral("};") << QStringLiteral("");
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlEvaluatorInfo evaluatorInfo(Scxml::EvaluatorId evaluatorId) const Q_DECL_OVERRIDE");
            clazz.dataMethods << QStringLiteral("{ Q_ASSERT(evaluatorId >= 0); Q_ASSERT(evaluatorId < %1); return evaluators[evaluatorId]; }").arg(evaluators.size());
            clazz.dataMethods << QString();

            if (isCppDataModel()) {
                {
                    StringListDumper stringEvals;
                    stringEvals << QStringLiteral("QString %1::evaluateToString(Scxml::EvaluatorId id, bool *ok) {").arg(clazz.dataModelClassName)
                              << QStringLiteral("    *ok = true;")
                              << QStringLiteral("    switch (id) {");
                    auto evals = stringEvaluators();
                    for (auto it = evals.constBegin(), eit = evals.constEnd(); it != eit; ++it) {
                        stringEvals << QStringLiteral("    case %1:").arg(it.key())
                                  << QStringLiteral("        return [this]()->QString{ return %1; }();").arg(it.value());
                    }
                    stringEvals << QStringLiteral("    default:")
                              << QStringLiteral("        Q_UNREACHABLE();")
                              << QStringLiteral("        *ok = false;")
                              << QStringLiteral("        return QString();")
                              << QStringLiteral("    }");
                    stringEvals << QStringLiteral("}");
                    clazz.dataModelMethods.append(Method(stringEvals));
                }

                {
                    StringListDumper boolEvals;
                    boolEvals << QStringLiteral("bool %1::evaluateToBool(Scxml::EvaluatorId id, bool *ok) {").arg(clazz.dataModelClassName)
                              << QStringLiteral("    *ok = true;")
                              << QStringLiteral("    switch (id) {");
                    auto evals = boolEvaluators();
                    for (auto it = evals.constBegin(), eit = evals.constEnd(); it != eit; ++it) {
                        boolEvals << QStringLiteral("    case %1:").arg(it.key())
                                  << QStringLiteral("        return [this]()->bool{ return %1; }();").arg(it.value());
                    }
                    boolEvals << QStringLiteral("    default:")
                              << QStringLiteral("        Q_UNREACHABLE();")
                              << QStringLiteral("        *ok = false;")
                              << QStringLiteral("        return false;")
                              << QStringLiteral("    }");
                    boolEvals << QStringLiteral("}");
                    clazz.dataModelMethods.append(Method(boolEvals));
                }

                {
                    StringListDumper variantEvals;
                    variantEvals << QStringLiteral("QVariant %1::evaluateToVariant(Scxml::EvaluatorId id, bool *ok) {").arg(clazz.dataModelClassName)
                                 << QStringLiteral("    *ok = true;")
                                 << QStringLiteral("    switch (id) {");
                    auto evals = variantEvaluators();
                    for (auto it = evals.constBegin(), eit = evals.constEnd(); it != eit; ++it) {
                        variantEvals << QStringLiteral("    case %1:").arg(it.key())
                                     << QStringLiteral("        return [this]()->QVariant{ return %1; }();").arg(it.value());
                    }
                    variantEvals << QStringLiteral("    default:")
                                 << QStringLiteral("        Q_UNREACHABLE();")
                                 << QStringLiteral("        *ok = false;")
                                 << QStringLiteral("        return QVariant();")
                                 << QStringLiteral("    }");
                    variantEvals << QStringLiteral("}");
                    clazz.dataModelMethods.append(Method(variantEvals));
                }

                {
                    StringListDumper voidEvals;
                    voidEvals << QStringLiteral("void %1::evaluateToVoid(Scxml::EvaluatorId id, bool *ok) {").arg(clazz.dataModelClassName)
                                 << QStringLiteral("    *ok = true;")
                                 << QStringLiteral("    switch (id) {");
                    auto evals = voidEvaluators();
                    for (auto it = evals.constBegin(), eit = evals.constEnd(); it != eit; ++it) {
                        voidEvals << QStringLiteral("    case %1:").arg(it.key())
                                     << QStringLiteral("        [this]()->void{ %1 }();").arg(it.value())
                                     << QStringLiteral("        break;");
                    }
                    voidEvals << QStringLiteral("    default:")
                                 << QStringLiteral("        Q_UNREACHABLE();")
                                 << QStringLiteral("        *ok = false;")
                                 << QStringLiteral("    }");
                    voidEvals << QStringLiteral("}");
                    clazz.dataModelMethods.append(Method(voidEvals));
                }
            }
        }

        { // assignments
            auto assignments = td->assignments();
            clazz.classFields << QStringLiteral("static Scxml::QScxmlAssignmentInfo assignments[];");
            t << QStringLiteral("Scxml::QScxmlAssignmentInfo %1::Data::assignments[] = {").arg(clazz.className);
            if (isCppDataModel()) {
                t << QStringLiteral("{ -1, -1, -1 }");
            } else {
                generateList(t, [&assignments](int idx) -> QString {
                    if (assignments.isEmpty() && idx == 0) // prevent generation of empty array
                        return QStringLiteral("{ -1, -1, -1 }");
                    if (idx >= assignments.size())
                        return QString();

                    auto ass = assignments.at(idx);
                    return QStringLiteral("{ %1, %2, %3 }").arg(ass.dest).arg(ass.expr).arg(ass.context);
                });
            }
            t << QStringLiteral("};") << QStringLiteral("");
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlAssignmentInfo assignmentInfo(Scxml::EvaluatorId assignmentId) const Q_DECL_OVERRIDE");
            clazz.dataMethods << QStringLiteral("{ Q_ASSERT(assignmentId >= 0); Q_ASSERT(assignmentId < %1); return assignments[assignmentId]; }").arg(assignments.size());
            clazz.dataMethods << QString();
        }

        { // foreaches
            auto foreaches = td->foreaches();
            clazz.classFields << QStringLiteral("static Scxml::QScxmlForeachInfo foreaches[];");
            t << QStringLiteral("Scxml::QScxmlForeachInfo %1::Data::foreaches[] = {").arg(clazz.className);
            generateList(t, [&foreaches](int idx) -> QString {
                if (foreaches.isEmpty() && idx == 0) // prevent generation of empty array
                    return QStringLiteral("{ -1, -1, -1, -1 }");
                if (idx >= foreaches.size())
                    return QString();

                auto foreach = foreaches.at(idx);
                return QStringLiteral("{ %1, %2, %3, %4 }").arg(foreach.array).arg(foreach.item).arg(foreach.index).arg(foreach.context);
            });
            t << QStringLiteral("};") << QStringLiteral("");
            clazz.dataMethods << QStringLiteral("Scxml::QScxmlForeachInfo foreachInfo(Scxml::EvaluatorId foreachId) const Q_DECL_OVERRIDE");
            clazz.dataMethods << QStringLiteral("{ Q_ASSERT(foreachId >= 0); Q_ASSERT(foreachId < %1); return foreaches[foreachId]; }").arg(foreaches.size());
        }

        { // strings
            t << QStringLiteral("#define STR_LIT(idx, ofs, len) \\")
              << QStringLiteral("    Q_STATIC_STRING_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \\")
              << QStringLiteral("    qptrdiff(offsetof(Strings, stringdata) + ofs * sizeof(qunicodechar) - idx * sizeof(QArrayData)) \\")
              << QStringLiteral("    )");

            t << QStringLiteral("%1::Data::Strings %1::Data::strings = {{").arg(clazz.className);
            auto strings = td->stringTable();
            if (strings.isEmpty()) // prevent generation of empty array
                strings.append(QStringLiteral(""));
            int ucharCount = 0;
            generateList(t, [&ucharCount, &strings](int idx) -> QString {
                if (idx >= strings.size())
                    return QString();

                int length = strings.at(idx).size();
                QString str = QStringLiteral("STR_LIT(%1, %2, %3)").arg(QString::number(idx),
                                                                        QString::number(ucharCount),
                                                                        QString::number(length));
                ucharCount += length + 1;
                return str;
            });
            t << QStringLiteral("},");
            if (strings.isEmpty()) {
                t << QStringLiteral("QT_UNICODE_LITERAL_II(\"\")");
            } else {
                for (int i = 0, ei = strings.size(); i < ei; ++i) {
                    QString s = strings.at(i);
                    QString comment = cEscape(s);
                    t << QStringLiteral("QT_UNICODE_LITERAL_II(\"%1\") // %2").arg(toHex(s) + QStringLiteral("\\x00"), comment);
                }
            }
            t << QStringLiteral("};") << QStringLiteral("");

            clazz.classFields << QStringLiteral("static struct Strings {")
                              << QStringLiteral("    QArrayData data[%1];").arg(strings.size())
                              << QStringLiteral("    qunicodechar stringdata[%1];").arg(ucharCount + 1)
                              << QStringLiteral("} strings;");

            clazz.dataMethods << QStringLiteral("QString string(Scxml::QScxmlExecutableContent::StringId id) const Q_DECL_OVERRIDE Q_DECL_FINAL");
            clazz.dataMethods << QStringLiteral("{");
            clazz.dataMethods << QStringLiteral("    Q_ASSERT(id >= Scxml::QScxmlExecutableContent::NoString); Q_ASSERT(id < %1);").arg(strings.size());
            clazz.dataMethods << QStringLiteral("    if (id == Scxml::QScxmlExecutableContent::NoString) return QString();");
            if (translationUnit->useCxx11) {
                clazz.dataMethods << QStringLiteral("    return QString({static_cast<QStringData*>(strings.data + id)});");
            } else {
                clazz.dataMethods << QStringLiteral("    QStringDataPtr data;");
                clazz.dataMethods << QStringLiteral("    data.ptr = static_cast<QStringData*>(strings.data + id);");
                clazz.dataMethods << QStringLiteral("    return QString(data);");
            }
            clazz.dataMethods << QStringLiteral("}");
            clazz.dataMethods << QString();
        }

        { // byte arrays
            t << QStringLiteral("#define BA_LIT(idx, ofs, len) \\")
              << QStringLiteral("    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \\")
              << QStringLiteral("    qptrdiff(offsetof(ByteArrays, stringdata) + ofs - idx * sizeof(QByteArrayData)) \\")
              << QStringLiteral("    )");

            t << QStringLiteral("%1::Data::ByteArrays %1::Data::byteArrays = {{").arg(clazz.className);
            auto byteArrays = td->byteArrayTable();
            if (byteArrays.isEmpty()) // prevent generation of empty array
                byteArrays.append(QByteArray(""));
            int charCount = 0;
            QString charData;
            generateList(t, [&charCount, &charData, &byteArrays](int idx) -> QString {
                if (idx >= byteArrays.size())
                    return QString();

                auto ba = byteArrays.at(idx);
                QString str = QStringLiteral("BA_LIT(%1, %2, %3)").arg(QString::number(idx),
                                                                       QString::number(charCount),
                                                                       QString::number(ba.size()));
                charData += toHex(QString::fromUtf8(ba)) + QStringLiteral("\\x00");
                charCount += ba.size() + 1;
                return str;
            });
            t << QStringLiteral("},");
            if (byteArrays.isEmpty()) {
                t << QStringLiteral("\"\"");
            } else {
                foreach (const QByteArray &ba, byteArrays) {
                    auto s = QString::fromUtf8(ba);
                    t << QStringLiteral("\"%1\" // %2").arg(toHex(s) + QStringLiteral("\\x00"), cEscape(s));
                }
            }
            t << QStringLiteral("};") << QStringLiteral("");

            clazz.classFields << QStringLiteral("static struct ByteArrays {")
                              << QStringLiteral("    QByteArrayData data[%1];").arg(byteArrays.size())
                              << QStringLiteral("    char stringdata[%1];").arg(charCount + 1)
                              << QStringLiteral("} byteArrays;");

            clazz.dataMethods << QStringLiteral("QByteArray byteArray(Scxml::QScxmlExecutableContent::ByteArrayId id) const Q_DECL_OVERRIDE Q_DECL_FINAL");
            clazz.dataMethods << QStringLiteral("{");
            clazz.dataMethods << QStringLiteral("    Q_ASSERT(id >= Scxml::QScxmlExecutableContent::NoByteArray); Q_ASSERT(id < %1);").arg(byteArrays.size());
            clazz.dataMethods << QStringLiteral("    if (id == Scxml::QScxmlExecutableContent::NoByteArray) return QByteArray();");
            if (translationUnit->useCxx11) {
                clazz.dataMethods << QStringLiteral("    return QByteArray({byteArrays.data + id});");
            } else {
                clazz.dataMethods << QStringLiteral("    QByteArrayDataPtr data;");
                clazz.dataMethods << QStringLiteral("    data.ptr = byteArrays.data + id;");
                clazz.dataMethods << QStringLiteral("    return QByteArray(data);");
            }
            clazz.dataMethods << QStringLiteral("}");
            clazz.dataMethods << QString();
        }
    }

    QString qba(const QString &bytes)
    {
//        QString str = QString::fromLatin1("QByteArray::fromRawData(\"");
//        auto esc = cEscape(bytes);
//        str += esc + QLatin1String("\", ") + QString::number(esc.length()) + QLatin1String(")");
//        return str;
        return QStringLiteral("byteArray(%1)").arg(addByteArray(bytes.toUtf8()));
    }

    QString scxmlClassName(DocumentModel::ScxmlDocument *doc) const
    {
        auto name = translationUnit->classnameForDocument.value(doc);
        Q_ASSERT(!name.isEmpty());
        return name;
    }

private:
    ClassDump &clazz;
    TranslationUnit *translationUnit;
    QHash<AbstractState *, QString> m_mangledNames;
    QVector<Node *> m_parents;
    QSet<QString> m_knownEvents;
    QSet<QString> m_signals;
    QString m_currentTransitionName;
    bool m_bindLate;
    QVector<DocumentModel::DataElement *> m_dataElements;
};
} // anonymous namespace


void CppDumper::dump(TranslationUnit *unit)
{
    Q_ASSERT(unit);
    Q_ASSERT(unit->mainDocument);

    m_translationUnit = unit;

    QVector<ClassDump> clazzes;
    auto docs = m_translationUnit->otherDocuments();
    clazzes.resize(docs.size() + 1);
    DumperVisitor(clazzes[0], m_translationUnit).process(unit->mainDocument);
    for (int i = 0, ei = docs.size(); i != ei; ++i) {
        auto doc = docs.at(i);
        DumperVisitor(clazzes[i + 1], m_translationUnit).process(doc);
    }

    auto headerName = QFileInfo(unit->outHFileName).fileName();
    const QString headerGuard = headerName.toUpper()
            .replace(QLatin1Char('.'), QLatin1Char('_'))
            .replace(QLatin1Char('-'), QLatin1Char('_'));
    writeHeaderStart(headerGuard);
    writeImplStart(clazzes);

    foreach (const ClassDump &clazz, clazzes) {
        writeClass(clazz);
        writeImplBody(clazz);
    }

    writeHeaderEnd(headerGuard);
    writeImplEnd();
}

QString CppDumper::mangleId(const QString &id)
{
    QString mangled(id);
    mangled = mangled.replace(QLatin1Char('_'), QLatin1String("__"));
    mangled = mangled.replace(QLatin1Char(':'), QLatin1String("_colon_"));
    mangled = mangled.replace(QLatin1Char('-'), QLatin1String("_dash_"));
    mangled = mangled.replace(QLatin1Char('@'), QLatin1String("_at_"));
    mangled = mangled.replace(QLatin1Char('.'), QLatin1String("_dot_"));
    return mangled;
}

void CppDumper::writeHeaderStart(const QString &headerGuard)
{
    h << QStringLiteral("#ifndef ") << headerGuard << endl
      << QStringLiteral("#define ") << headerGuard << endl
      << endl;
    h << l(headerStart);
    if (!m_translationUnit->namespaceName.isEmpty())
        h << l("namespace ") << m_translationUnit->namespaceName << l(" {") << endl << endl;
}

void CppDumper::writeClass(const ClassDump &clazz)
{
    h << l("class ") << clazz.className << QStringLiteral(": public Scxml::StateMachine\n{") << endl;
    h << QLatin1String("    Q_OBJECT\n");
    clazz.properties.write(h, QStringLiteral("    "), QStringLiteral("\n"));
    h << QLatin1String("\npublic:\n");
    h << l("    ") << clazz.className << l("(QObject *parent = 0);") << endl;
    h << l("    ~") << clazz.className << "();" << endl;

    if (!clazz.publicMethods.isEmpty()) {
        h << endl;
        foreach (const Method &m, clazz.publicMethods) {
            h << QStringLiteral("    ") << m.decl << QLatin1Char(';') << endl;
        }
    }

    if (!clazz.signalMethods.isEmpty()) {
        h << endl
          << QStringLiteral("signals:")
          << endl;
        clazz.signalMethods.write(h, QStringLiteral("    "), QStringLiteral("\n"));
    }

    if (!clazz.publicSlotDeclarations.isEmpty()) {
        h << endl
          << QStringLiteral("public slots:") << endl;
        clazz.publicSlotDeclarations.write(h, QStringLiteral("    "), QStringLiteral("\n"));
    }

    h << endl
      << l("private:") << endl
      << l("    struct Data;") << endl
      << l("    friend Data;") << endl
      << l("    struct Data *data;") << endl
      << l("};") << endl << endl;
}

void CppDumper::writeHeaderEnd(const QString &headerGuard)
{
    if (!m_translationUnit->namespaceName.isEmpty()) {
        h << QStringLiteral("} // %1 namespace ").arg(m_translationUnit->namespaceName) << endl
          << endl;
    }
    h << QStringLiteral("#endif // ") << headerGuard << endl;
}

void CppDumper::writeImplStart(const QVector<ClassDump> &allClazzes)
{
    StringListDumper includes;
    foreach (const ClassDump &clazz, allClazzes) {
        includes.text += clazz.implIncludes.text;
    }
    includes.unique();

    auto headerName = QFileInfo(m_translationUnit->outHFileName).fileName();
    cpp << l("#include \"") << headerName << l("\"") << endl;
    cpp << endl
        << QStringLiteral("#include <QtScxml/scxmlqstates.h>") << endl;
    if (!includes.isEmpty()) {
        includes.write(cpp, QStringLiteral("#include <"), QStringLiteral(">\n"));
        cpp << endl;
    }
    if (!m_translationUnit->namespaceName.isEmpty())
        cpp << l("namespace ") << m_translationUnit->namespaceName << l(" {") << endl << endl;
}

void CppDumper::writeImplBody(const ClassDump &clazz)
{
    cpp << l("struct ") << clazz.className << l("::Data: private Scxml::TableData");
    if (!clazz.signalMethods.isEmpty()) {
        cpp << QStringLiteral(", public Scxml::ScxmlEventFilter");
    }
    cpp << l(" {") << endl;

    cpp << QStringLiteral("    Data(%1 &stateMachine)\n        : stateMachine(stateMachine)").arg(clazz.className) << endl;
    clazz.constructor.initializer.write(cpp, QStringLiteral("        , "), QStringLiteral("\n"));
    cpp << l("    { init(); }") << endl;

    cpp << endl;
    cpp << l("    void init() {\n");
    clazz.init.impl.write(cpp, QStringLiteral("        "), QStringLiteral("\n"));
    cpp << l("    }") << endl;
    cpp << endl;
    clazz.dataMethods.write(cpp, QStringLiteral("    "), QStringLiteral("\n"));

    cpp << endl
        << QStringLiteral("    %1 &stateMachine;").arg(clazz.className) << endl;
    clazz.classFields.write(cpp, QStringLiteral("    "), QStringLiteral("\n"));

    cpp << l("};") << endl
        << endl;
    cpp << clazz.className << l("::") << clazz.className << l("(QObject *parent)") << endl
        << l("    : Scxml::StateMachine(parent)") << endl
        << l("    , data(new Data(*this))") << endl
        << l("{ qRegisterMetaType<QAbstractState *>(); }") << endl
        << endl;
    cpp << clazz.className << l("::~") << clazz.className << l("()") << endl
        << l("{ delete data; }") << endl
        << endl;
    foreach (const Method &m, clazz.publicMethods) {
        m.impl.write(cpp, QStringLiteral(""), QStringLiteral("\n"), clazz.className);
        cpp << endl;
    }
    clazz.publicSlotDefinitions.write(cpp, QStringLiteral("\n"), QStringLiteral("\n"));
    cpp << endl;

    clazz.tables.write(cpp, QStringLiteral(""), QStringLiteral("\n"));

    if (!clazz.dataModelMethods.isEmpty()) {
        bool first = true;
        foreach (const Method &m, clazz.dataModelMethods) {
            if (first) {
                first = false;
            } else {
                cpp << endl;
            }
            m.impl.write(cpp, QStringLiteral(""), QStringLiteral("\n"));
        }
    }
}

void CppDumper::writeImplEnd()
{
    if (!m_translationUnit->namespaceName.isEmpty()) {
        cpp << endl
            << QStringLiteral("} // %1 namespace").arg(m_translationUnit->namespaceName) << endl;
    }
}

} // namespace Scxml
QT_END_NAMESPACE
