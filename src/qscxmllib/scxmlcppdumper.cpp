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

#include "executablecontent_p.h"
#include "scxmlcppdumper.h"

#include <algorithm>

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

    void write(QTextStream &out, const QString &prefix, const QString &suffix) const
    {
        foreach (const QString &line, text)
            out << prefix << line << suffix;
    }

    QStringList text;
};

struct Method {
    QString name;
    StringListDumper publicDeclaration; // void f(int i = 0);
    StringListDumper implementationHeader; // void f(int i)
    StringListDumper initializer;
    StringListDumper publicImplementationCode; // d->f(i);
    StringListDumper impl; // m_i = ++i;
};

struct MainClass {
    StringListDumper implIncludes;
    QString className;
    StringListDumper classFields;
    StringListDumper tables;
    Method init;
    Method initDataModel;
    Method constructor;
    Method destructor;
    StringListDumper signalMethods;
    QList<Method> publicMethods;
    StringListDumper publicSlotDeclarations;
    StringListDumper publicSlotDefinitions;
    QList<Method> privateMethods;
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

QString qba(const QString &bytes)
{
    QString str = QString::fromLatin1("QByteArray::fromRawData(\"");
    auto esc = cEscape(bytes);
    str += esc + QLatin1String("\", ") + QString::number(esc.length()) + QLatin1String(")");
    return str;
}

QString strLit(const QString &str)
{
    if (str.isNull())
        return QStringLiteral("QString()");
    return QStringLiteral("QStringLiteral(\"%1\")").arg(cEscape(str));
}

const char *headerStart =
        "#include <QScxmlLib/scxmlstatetable.h>\n"
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

class DumperVisitor: public ExecutableContent::Builder
{
    Q_DISABLE_COPY(DumperVisitor)

public:
    DumperVisitor(MainClass &clazz, const QString &mainClassName, const CppDumpOptions &options)
        : clazz(clazz)
        , m_mainClassName(mainClassName)
        , m_options(options)
    {}

    void process(ScxmlDocument *doc)
    {
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
            clazz.init.impl << QStringLiteral("table.setName(QStringLiteral(\"") + cEscape(node->name) + QStringLiteral("\"));");
            if (m_options.nameQObjects)
                clazz.init.impl << QStringLiteral("table.setObjectName(QStringLiteral(\"") + cEscape(node->name) + QStringLiteral("\"));");
        }
        QString dmName;
        switch (node->dataModel) {
        case Scxml::NullDataModel:
            dmName = QStringLiteral("Null");
            clazz.implIncludes << QStringLiteral("QScxmlLib/nulldatamodel.h");
            break;
        case Scxml::JSDataModel:
            dmName = QStringLiteral("EcmaScript");
            clazz.implIncludes << QStringLiteral("QScxmlLib/ecmascriptdatamodel.h");
            break;
        default:
            Q_UNREACHABLE();
        }
        clazz.init.impl << QStringLiteral("table.setDataModel(new Scxml::") + dmName + QStringLiteral("DataModel(&table));");
        clazz.init.impl << QStringLiteral("table.dataModel()->setEvaluators(evaluators, assignments, foreaches);");
        clazz.init.impl << QStringLiteral("table.setDataItemNames(dataIds);");

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
        clazz.init.impl << QStringLiteral("table.setDataBinding(Scxml::StateTable::%1Binding);").arg(binding);

        clazz.implIncludes << QStringLiteral("QScxmlLib/executablecontent.h");
        clazz.init.impl << QStringLiteral("table.setTableData(this);");

        foreach (AbstractState *s, node->initialStates) {
            clazz.init.impl << QStringLiteral("table.setInitialState(&state_") + mangledName(s) + QStringLiteral(");");
        }

        // visit the kids:
        m_parents.append(node);
        visit(node->children);
        visit(node->dataElements);

        m_dataElements.append(node->dataElements);
        if (node->script || !m_dataElements.isEmpty() || !node->initialSetup.isEmpty()) {
            clazz.init.impl << QStringLiteral("table.setInitialSetup(%1);").arg(startNewSequence());
            generate(m_dataElements);
            if (node->script) {
                node->script->accept(this);
            }
            visit(&node->initialSetup);
            endSequence();
        }

        m_parents.removeLast();
        return false;
    }

    bool visit(State *node) Q_DECL_OVERRIDE
    {
        auto stateName = QStringLiteral("state_") + mangledName(node);
        // Declaration:
        if (node->type == State::Final) {
            clazz.classFields << QStringLiteral("Scxml::ScxmlFinalState ") + stateName + QLatin1Char(';');
        } else {
            clazz.classFields << QStringLiteral("Scxml::ScxmlState ") + stateName + QLatin1Char(';');
        }

        // Initializer:
        clazz.constructor.initializer << generateInitializer(node);

        // init:
        if (!node->id.isEmpty() && m_options.nameQObjects) {
            clazz.init.impl << stateName + QStringLiteral(".setObjectName(QStringLiteral(\"") + cEscape(node->id) + QStringLiteral("\"));");
        }
        if (node->initialState) {
            clazz.init.impl << stateName + QStringLiteral(".setInitialState(&state_") + mangledName(node->initialState) + QStringLiteral(");");
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
        clazz.classFields << QStringLiteral("Scxml::ScxmlTransition ") + tName + QLatin1Char(';');

        // Initializer:
        QString parentName;
        auto parent = m_parents.last();
        if (HistoryState *historyState = parent->asHistoryState()) {
            parentName = QStringLiteral("state_") + mangledName(historyState) + QStringLiteral("_defaultConfiguration");
        } else {
            parentName = parentStateMemberName();
        }
        Q_ASSERT(!parentName.isEmpty());
        QString initializer = tName + QStringLiteral("(&") + parentName + QStringLiteral(", {");
        for (int i = 0, ei = node->events.size(); i != ei; ++i) {
            if (i == 0) {
                initializer += QLatin1Char(' ');
            } else {
                initializer += QStringLiteral(", ");
            }
            initializer += qba(node->events.at(i));
            if (i + 1 == ei) {
                initializer += QLatin1Char(' ');
            }
        }
        initializer += QStringLiteral("})");
        clazz.constructor.initializer << initializer;

        // init:
        if (node->condition) {
            QString condExpr = *node->condition.data();
            auto cond = createEvaluatorBool(QStringLiteral("transition"), QStringLiteral("cond"), condExpr);
            clazz.init.impl << tName + QStringLiteral(".setConditionalExpression(%1);").arg(cond);
        }
        clazz.init.impl << parentName + QStringLiteral(".addTransition(&") + tName + QStringLiteral(");");
        if (node->type == Transition::Internal) {
            clazz.init.impl << tName + QStringLiteral(".setTransitionType(QAbstractTransition::InternalTransition);");
        }
        QString targets = tName + QStringLiteral(".setTargetStates({");
        for (int i = 0, ei = node->targetStates.size(); i != ei; ++i) {
            if (i == 0) {
                targets += QLatin1Char(' ');
            } else {
                targets += QStringLiteral(", ");
            }
            targets += QStringLiteral("&state_") + mangledName(node->targetStates.at(i));
            if (i + 1 == ei) {
                targets += QLatin1Char(' ');
            }
        }
        clazz.init.impl << targets + QStringLiteral("});");

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
        if (!node->id.isEmpty() && m_options.nameQObjects) {
            clazz.init.impl << stateName + QStringLiteral(".setObjectName(QStringLiteral(\"") + cEscape(node->id) + QStringLiteral("\"));");
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

        // visit the kids:
        if (Transition *t = node->defaultConfiguration()) {
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

            //  visit the kid:
            m_parents.append(node);
            t->accept(this);
            m_parents.removeLast();
        }
        return false;
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
            parentName = QStringLiteral("table");
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

    QString generateInitializer(AbstractState *node)
    {
        auto stateName = QStringLiteral("state_") + mangledName(node);
        QString init = stateName + QStringLiteral("(&");
        if (State *parentState = node->parent->asState()) {
            init += QStringLiteral("state_") + mangledName(parentState);
        } else {
            init += QStringLiteral("table");
        }
        init += QLatin1Char(')');
        return init;
    }

    void addEvents()
    {
        QStringList knownEventsList = m_knownEvents.toList();
        std::sort(knownEventsList.begin(), knownEventsList.end());
        foreach (QString event, knownEventsList) {
            if (event.startsWith(QStringLiteral("done.")) || event.startsWith(QStringLiteral("qsignal."))
                    || event.startsWith(QStringLiteral("qevent."))) {
                continue;
            }
            if (event.contains(QLatin1Char('*')))
                continue;

            clazz.publicSlotDeclarations << QStringLiteral("void event_") + CppDumper::mangleId(event) + QStringLiteral("();");
            clazz.publicSlotDefinitions << QStringLiteral("void ") + m_mainClassName
                                           + QStringLiteral("::event_")
                                           + CppDumper::mangleId(event)
                                           + QStringLiteral("()\n{ submitEvent(") + qba(event)
                                           + QStringLiteral("); }");
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
            return QStringLiteral("table");
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
        QScopedPointer<ExecutableContent::DynamicTableData> td(tableData());

        { // instructions
            clazz.classFields << QStringLiteral("static qint32 theInstructions[];");
            t << QStringLiteral("qint32 %1::Data::theInstructions[] = {").arg(m_mainClassName);
            auto instr = td->instructionTable();
            generateList(t, [&instr](int idx) -> QString {
                if (idx < instr.size())
                    return QString::number(instr.at(idx));
                else
                    return QString();
            });
            t << QStringLiteral("};") << QStringLiteral("");
        }

        { // strings
            clazz.classFields << QStringLiteral("static QVector<QString> strings;");
            t << QStringLiteral("QVector<QString> %1::Data::strings({").arg(m_mainClassName);
            auto strings = td->stringTable();
            for (int i = 0, ei = strings.size(); i != ei; ++i) {
                QString s = QStringLiteral("    ") + strLit(strings.at(i));
                if (i + 1 != ei)
                    s += QLatin1Char(',');
                t << s;
            }
            t << QStringLiteral("});") << QStringLiteral("");
        }

        { // byte arrays
            t << QStringLiteral("#define BA_LIT(idx, ofs, len) \\")
              << QStringLiteral("    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \\")
              << QStringLiteral("    qptrdiff(offsetof(ByteArrays, stringdata) + ofs - idx * sizeof(QByteArrayData)) \\")
              << QStringLiteral("    )");

            t << QStringLiteral("%1::Data::ByteArrays %1::Data::byteArrays = {{").arg(m_mainClassName);
            auto byteArrays = td->byteArrayTable();
            int charCount = 0;
            QString charData;
            generateList(t, [&charCount, &charData, &byteArrays](int idx) -> QString {
                if (idx >= byteArrays.size())
                    return QString();

                auto ba = cEscape(QString::fromUtf8(byteArrays.at(idx)));
                QString str = QStringLiteral("BA_LIT(%1, %2, %3)").arg(QString::number(idx),
                                                                       QString::number(charCount),
                                                                       QString::number(ba.size()));
                charData += ba + QStringLiteral("\\0");
                charCount += ba.size() + 1;
                return str;
            });
            t << QStringLiteral("}, \"%1\"").arg(charData);
            t << QStringLiteral("};") << QStringLiteral("");

            clazz.classFields << QStringLiteral("static struct ByteArrays {")
                              << QStringLiteral("    QByteArrayData data[%1];").arg(byteArrays.size())
                              << QStringLiteral("    char stringdata[%1];").arg(charCount + 1)
                              << QStringLiteral("} byteArrays;");
        }

        { // dataIds
            auto dataIds = this->dataIds();
            clazz.classFields << QStringLiteral("static Scxml::ExecutableContent::StringIds dataIds;");
            t << QStringLiteral("Scxml::ExecutableContent::StringIds %1::Data::dataIds({").arg(m_mainClassName);
            generateList(t, [&dataIds](int idx) -> QString {
                if (idx < dataIds.size())
                    return QString::number(dataIds.at(idx));
                else
                    return QString();
            });
            t << QStringLiteral("});") << QStringLiteral("");
        }

        { // evaluators
            auto evaluators = this->evaluators();
            clazz.classFields << QStringLiteral("static Scxml::EvaluatorInfos evaluators;");
            t << QStringLiteral("Scxml::EvaluatorInfos %1::Data::evaluators({").arg(m_mainClassName);
            generateList(t, [&evaluators](int idx) -> QString {
                if (idx >= evaluators.size())
                    return QString();

                auto eval = evaluators.at(idx);
                return QStringLiteral("{ %1, %2 }").arg(eval.expr).arg(eval.context);
            });
            t << QStringLiteral("});") << QStringLiteral("");
        }

        { // assignments
            auto assignments = this->assignments();
            clazz.classFields << QStringLiteral("static Scxml::AssignmentInfos assignments;");
            t << QStringLiteral("Scxml::AssignmentInfos %1::Data::assignments({").arg(m_mainClassName);
            generateList(t, [&assignments](int idx) -> QString {
                if (idx >= assignments.size())
                    return QString();

                auto ass = assignments.at(idx);
                return QStringLiteral("{ %1, %2, %3 }").arg(ass.dest).arg(ass.expr).arg(ass.context);
            });
            t << QStringLiteral("});") << QStringLiteral("");
        }

        { // foreaches
            auto foreaches = this->foreaches();
            clazz.classFields << QStringLiteral("static Scxml::ForeachInfos foreaches;");
            t << QStringLiteral("Scxml::ForeachInfos %1::Data::foreaches({").arg(m_mainClassName);
            generateList(t, [&foreaches](int idx) -> QString {
                if (idx >= foreaches.size())
                    return QString();

                auto foreach = foreaches.at(idx);
                return QStringLiteral("{ %1, %2, %3, %4 }").arg(foreach.array).arg(foreach.item).arg(foreach.index).arg(foreach.context);
            });
            t << QStringLiteral("});") << QStringLiteral("");
        }
    }

private:
    MainClass &clazz;
    const QString &m_mainClassName;
    const CppDumpOptions &m_options;
    QHash<DocumentModel::AbstractState *, QString> m_mangledNames;
    QVector<Node *> m_parents;
    QSet<QString> m_knownEvents;
    QString m_currentTransitionName;
    bool m_bindLate = false;
    QVector<DocumentModel::DataElement *> m_dataElements;
};
} // anonymous namespace

static QString tableDataImpl = QLatin1String(
            "    QString string(Scxml::ExecutableContent::StringId id) const Q_DECL_OVERRIDE\n"
            "    { return id == Scxml::ExecutableContent::NoString ? QString() : strings.at(id); }\n"
            "\n"
            "    QByteArray byteArray(Scxml::ExecutableContent::ByteArrayId id) const Q_DECL_OVERRIDE\n"
            "    { return QByteArray({&byteArrays.data[id]}); }\n"
            "\n"
            "    Scxml::ExecutableContent::Instructions instructions() const Q_DECL_OVERRIDE\n"
            "    { return theInstructions; }\n"
            );

void CppDumper::dump(DocumentModel::ScxmlDocument *doc)
{
    m_doc = doc;
    mainClassName = options.classname;
    if (mainClassName.isEmpty()) {
        mainClassName = mangleId(doc->root->name);
        if (!mainClassName.isEmpty())
            mainClassName.append(QLatin1Char('_'));
        mainClassName.append(l("StateMachine"));
    }

    MainClass clazz;
    DumperVisitor(clazz, mainClassName, options).process(doc);

    // Generate the .h file:
    const QString headerGuard = headerName.toUpper().replace(QLatin1Char('.'), QLatin1Char('_'));
    h << QStringLiteral("#ifndef ") << headerGuard << endl
      << QStringLiteral("#define ") << headerGuard << endl
      << endl;
    h << l(headerStart);
    if (!options.namespaceName.isEmpty())
        h << l("namespace ") << options.namespaceName << l(" {") << endl << endl;
    h << l("class ") << mainClassName << l(" : public Scxml::StateTable\n{") << endl;
    h << QLatin1String("    Q_OBJECT\n\n");
    h << QLatin1String("public:\n");
    h << l("    ") << mainClassName << l("(QObject *parent = 0);") << endl;
    h << l("    ~") << mainClassName << "();" << endl;
    h << endl
      << l("    bool init() Q_DECL_OVERRIDE;") << endl;

    if (!clazz.publicSlotDeclarations.isEmpty()) {
        h << "\npublic slots:\n";
        clazz.publicSlotDeclarations.write(h, QStringLiteral("    "), QStringLiteral("\n"));
    }

    h << endl
      << l("private:") << endl
      << l("    struct Data;") << endl
      << l("    friend Data;") << endl
      << l("    struct Data *data;") << endl
      << l("};") << endl;

    if (!options.namespaceName.isEmpty())
        h << endl << l("} // namespace ") << options.namespaceName << endl;
    h << endl
      << QStringLiteral("#endif // ") << headerGuard << endl;

    // Generate the .cpp file:
    cpp << l("#include \"") << headerName << l("\"") << endl
        << endl;
    if (!clazz.implIncludes.isEmpty()) {
        clazz.implIncludes.write(cpp, QStringLiteral("#include <"), QStringLiteral(">\n"));
        cpp << endl;
    }
    if (!options.namespaceName.isEmpty())
        cpp << l("namespace ") << options.namespaceName << l(" {") << endl << endl;

    cpp << l("struct ") << mainClassName << l("::Data: private Scxml::TableData {") << endl;

    cpp << QStringLiteral("    Data(%1 &table)\n        : table(table)").arg(mainClassName) << endl;
    clazz.constructor.initializer.write(cpp, QStringLiteral("        , "), QStringLiteral("\n"));
    cpp << l("    {}") << endl;

    cpp << endl;
    cpp << l("    void init() {\n");    
    clazz.init.impl.write(cpp, QStringLiteral("        "), QStringLiteral("\n"));
    cpp << l("    }") << endl;
    cpp << endl
        << tableDataImpl;

    cpp << endl
        << QStringLiteral("    %1 &table;").arg(mainClassName) << endl;
    clazz.classFields.write(cpp, QStringLiteral("    "), QStringLiteral("\n"));

    cpp << l("};") << endl
        << endl;
    cpp << mainClassName << l("::") << mainClassName << l("(QObject *parent)") << endl
        << l("    : Scxml::StateTable(parent)") << endl
        << l("    , data(new Data(*this))") << endl
        << l("{}") << endl
        << endl;
    cpp << mainClassName << l("::~") << mainClassName << l("()") << endl
        << l("{ delete data; }") << endl
        << endl;
    cpp << l("bool ") << mainClassName << l("::init()") << endl
        << l("{ data->init(); return StateTable::init(); }") << endl;
    cpp << endl;
    clazz.publicSlotDefinitions.write(cpp, QStringLiteral("\n"), QStringLiteral("\n"));
    cpp << endl;

    clazz.tables.write(cpp, QStringLiteral(""), QStringLiteral("\n"));

    if (!options.namespaceName.isEmpty())
        cpp << l("} // namespace ") << options.namespaceName << endl;
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

} // namespace Scxml
