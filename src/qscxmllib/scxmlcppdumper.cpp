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

//StringListDumper &endl(StringListDumper &s)
//{
//    s << QLatin1String("\n");
//    return s;
//}

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
    Method init;
    Method constructor;
    Method destructor;
    StringListDumper signalMethods;
    QList<Method> publicMethods;
    StringListDumper publicSlotDeclarations;
    StringListDumper publicSlotDefinitions;
    QList<Method> privateMethods;
};

namespace {
using namespace ExecutableContent;

QByteArray cEscape(const QByteArray &str)
{ // should handle even trigraphs?
    QByteArray res;
    int lastI = 0;
    for (int i = 0; i < str.length(); ++i) {
        unsigned char c = str.at(i);
        if (c < ' ' || c == '\\' || c == '\"') {
            res.append(str.mid(lastI, i - lastI));
            lastI = i + 1;
            if (c == '\\') {
                res.append("\\\\");
            } else if (c == '\"') {
                res.append("\"");
            } else {
                char buf[4];
                buf[0] = '\\';
                buf[3] = '0' + (c & 0x7);
                c >>= 3;
                buf[2] = '0' + (c & 0x7);
                c >>= 3;
                buf[1] = '0' + (c & 0x7);
                res.append(&buf[0], 4);
            }
        }
    }
    if (lastI != 0) {
        res.append(str.mid(lastI));
        return res;
    }
    return str;
}

QString cEscape(const QString &str)
{// should handle even trigraphs?
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
                res.append(QLatin1String("\""));
            } else {
                char buf[6];
                ushort cc = c.unicode();
                buf[0] = '\\';
                buf[1] = 'u';
                for (int i = 0; i < 4; ++i) {
                    buf[5 - i] = '0' + (cc & 0xF);
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

//QString cEscape(const char *str)
//{
//    return cEscape(QString::fromLatin1(str));
//}

QString qba(const QByteArray &bytes)
{
    QString str = QString::fromLatin1("QByteArray::fromRawData(\"");
    auto esc = cEscape(bytes);
    str += QString::fromLatin1(esc) + QLatin1String("\", ") + QString::number(esc.length()) + QLatin1String(")");
    return str;
}

QString qba(const QString &bytes)
{
    QString str = QString::fromLatin1("QByteArray::fromRawData(\"");
    auto esc = cEscape(bytes);
    str += esc + QLatin1String("\", ") + QString::number(esc.length()) + QLatin1String(")");
    return str;
}

class DumpCppInstructionVisitor : public InstructionVisitor {
    QLatin1String l(const char *str) { return QLatin1String(str); }
    QTextStream &s;
    int indentSize;
    bool didDumpTable;
public:
    DumpCppInstructionVisitor(QTextStream &dumper, int indentSize = 0)
        : s(dumper)
        , indentSize(indentSize)
        , didDumpTable(false) { }
    QTextStream & indented() {
        for (int i = 0; i < indentSize; ++i)
            s << QLatin1Char(' ');
        return s;
    }
protected:
    void maybeTable() {
        if (didDumpTable)
            return;
        didDumpTable = true;
        indented() << "StateTable *sTable = table();\n";
    }

    void visitRaise(const Raise *r) Q_DECL_OVERRIDE {
        maybeTable();
        indented() << l("sTable->submitEvent(") << qba(r->event) << l(");\n");
    }

    void visitSend(const Send *send) Q_DECL_OVERRIDE {
        maybeTable();
#if 1
        Q_UNIMPLEMENTED();
        Q_UNUSED(send);
#else // FIXME
        if (!send->delayexpr.isEmpty() || !send->delay.isEmpty()) {
            if (!send->delayexpr.isEmpty()) {
                indented() << l("int delay = sTable->evalValueInt(QStringLiteral(\"")
                  << cEscape(send->delayexpr)
                  << l("\"), []() -> QString { return QStringLiteral(\"")
                  << cEscape(send->instructionLocation())
                  << l("\"); }, 0);\n");
            } else {
                bool ok;
                send->delay.toInt(&ok);
                if (ok) {
                    indented() << l("int delay = ") << send->delay << l(";\n");
                } else {
                    indented() << l("int delay = 0;");
                    qCWarning(scxmlLog) << "could not convert delay '" << send->delay
                                        << "' to int, using 0 instead in "
                                        << send->instructionLocation();
                }
            }
            indented() << l("sTable->submitDelayedEvent(delay, ");
        } else {
            indented() << l("sTable->submitEvent(");
        }
        if (!send->eventexpr.isEmpty()) {
            s << l("sTable->evalStr(QStringLiteral(\"") << cEscape(send->eventexpr)
              << l("\"),\n");
        } else {
            s << qba(send->event) << "\n";
            if (send->event.isEmpty())
                qCWarning(scxmlLog) << "missing event from send in " << send->instructionLocation();
        }
#endif
        /*if (!send->targetexpr.isEmpty()) {
            l("sTable->evalStr(QStringLiteral(\"") << cEscape(send->targetexpr) << l("\"),\n");
            s.writeAttribute("targetexpr", send->targetexpr);
        else if (!send->target.isEmpty())
            s.writeAttribute("target", send->target);
        if (!send->typeexpr.isEmpty())
            s.writeAttribute("typeexpr", send->typeexpr);
        else if (!send->type.isEmpty())
            s.writeAttribute("type", send->type);
        if (!send->idLocation.isEmpty())
            s.writeAttribute("idlocation", send->idLocation);
        else if (!send->id.isEmpty())
            s.writeAttribute("id", send->id);
        if (!send->namelist.isEmpty())
            s.writeAttribute("namelist", send->namelist.join(QLatin1Char(' ')));
        foreach (const Param &p, send->params) {
            s.writeStartElement("param");
            s.writeAttribute("name", p.name);
            if (!p.location.isEmpty())
                s.writeAttribute("location", p.location);
            else if (!p.expr.isEmpty())
                s.writeAttribute("expr", p.expr);
            s.writeEndElement();
        }
        if (send->content) {
            s.writeStartElement("content");
            send->content->dump(s.s);
            s.writeEndElement();
        }*/
        indented() << l("    QVariant());\n");
    }

    void visitLog(const Log *log) Q_DECL_OVERRIDE {
        maybeTable();
        indented() << l("sTable->doLog(QStringLiteral(\"") << cEscape(log->label) << l("\",\n");
        Q_UNIMPLEMENTED(); // FIXME
//        indented() << l("    sTable->evalValueStr( QStringLiteral(\"") << cEscape(log->expr) << l("\")));\n");
    }

    void visitJavaScript(const JavaScript *script) Q_DECL_OVERRIDE {
        Q_UNUSED(script)

        /*s.writeStartElement("script");
        if (!script->src.isEmpty())
            s.writeAttribute("src", script->src);
        else if (!script->source.isEmpty())
            s.s.writeCharacters(script->source); // escape? use CDATA?
        s.writeEndElement();*/
    }

    void visitAssignExpression(const AssignExpression *assign) Q_DECL_OVERRIDE {
        Q_UNUSED(assign)

        /*
        s.writeStartElement("assign");
        s.writeAttribute("location", assign->location);
        if (assign->content)
            assign->content->dump(s.s);
        else if (!assign->expression.isEmpty())
            s.writeAttribute("expr", assign->expression);
        s.writeEndElement();*/
    }

    void visitCancel(const Cancel *c) Q_DECL_OVERRIDE {
        Q_UNUSED(c)

        /*s.writeStartElement("cancel");
        if (!c->sendidexpr.isEmpty())
            s.writeAttribute("sendidexpr", c->sendidexpr);
        else if (!c->sendid.isEmpty())
            s.writeAttribute("sendid", c->sendid);*/
    }

    bool visitInvoke(const Invoke *invoke) Q_DECL_OVERRIDE {
        Q_UNUSED(invoke)

        /*s.writeStartElement("invoke");
        if (!invoke->typeexpr.isEmpty())
            s.writeAttribute("typeexpr", invoke->typeexpr);
        else if (!invoke->type.isEmpty())
            s.writeAttribute("type", invoke->type);
        if (!invoke->srcexpr.isEmpty())
            s.writeAttribute("srcexpr", invoke->srcexpr);
        else if (!invoke->src.isEmpty())
            s.writeAttribute("src", invoke->src);
        if (!invoke->idLocation.isEmpty())
            s.writeAttribute("idlocation", invoke->idLocation);
        else if (!invoke->id.isEmpty())
            s.writeAttribute("id", invoke->id);
        if (!invoke->namelist.isEmpty())
            s.writeAttribute("namelist", invoke->namelist.join(QLatin1Char(' ')));
        if (invoke->autoforward)
            s.writeAttribute("autoforward", "true");
        foreach (const Param &p, invoke->params) {
            s.writeStartElement("param");
            s.writeAttribute("name", p.name);
            if (!p.location.isEmpty())
                s.writeAttribute("location", p.location);
            else if (!p.expr.isEmpty())
                s.writeAttribute("expr", p.expr);
            s.writeEndElement();
        }
        if (!invoke->finalize.statements.isEmpty()) {
            s.writeStartElement("finalize");
            accept(&invoke->finalize);
            s.writeEndElement();
        }
        if (invoke->content) {
            s.writeStartElement("content");
            invoke->content->dump(s.s);
            s.writeEndElement();
        }
        s.writeEndElement();*/
        return false;
    }

    void endVisitInvoke(const Invoke *) Q_DECL_OVERRIDE { }

    bool visitIf(const If *ifI) Q_DECL_OVERRIDE {
        Q_UNUSED(ifI)

        /*s.writeStartElement("if");
        int maxI = ifI->conditions.size();
        if (ifI->blocks.size() < maxI) {
            qCWarning(scxmlLog) << "if instruction with too few blocks " << ifI->blocks.size()
                                << " for " << ifI->conditions.size() << " conditions "
                                << ifI->instructionLocation();
            maxI = ifI->blocks.size();
        }
        s.writeAttribute("cond", ifI->conditions.value(0));
        if (!ifI->blocks.isEmpty())
            accept(&ifI->blocks[0]);
        for (int i = 1; i < maxI; ++i) {
            s.writeStartElement("elseif");
            s.writeAttribute("cond", ifI->conditions.at(i));
            s.writeEndElement();
            accept(&ifI->blocks[i]);
        }
        if (ifI->blocks.size() > maxI) {
            s.writeStartElement("else");
            s.writeEndElement();
            accept(&ifI->blocks[maxI]);
            if (ifI->blocks.size() > maxI + 1) {
                qCWarning(scxmlLog) << "if instruction with too many blocks " << ifI->blocks.size()
                                    << " for " << ifI->conditions.size() << " conditions "
                                    << ifI->instructionLocation();
            }
        }
        s.writeEndElement();*/
        return false;
    }

    void endVisitIf(const If *) Q_DECL_OVERRIDE { }
    bool visitForeach(const Foreach *foreachI) Q_DECL_OVERRIDE {
        Q_UNUSED(foreachI)

        /*s.writeStartElement("foreach");
        s.writeAttribute("array", foreachI->array);
        s.writeAttribute("item", foreachI->item);
        if (!foreachI->index.isEmpty())
            s.writeAttribute("index", foreachI->index);*/
        return true;
    }

    void endVisitForeach(const Foreach *) Q_DECL_OVERRIDE {
        //s.writeEndElement();
    }
    bool visitSequence(const InstructionSequence *) Q_DECL_OVERRIDE { return true; }
    void endVisitSequence(const InstructionSequence *) Q_DECL_OVERRIDE { }

};

const char *headerStart =
        "#include <QScxmlLib/scxmlstatetable.h>\n"
        "\n";

using namespace DocumentModel;
class DumperVisitor: public DocumentModel::NodeVisitor
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
    }

    ~DumperVisitor()
    {
        Q_ASSERT(m_parents.isEmpty());
    }

protected:
    bool visit(Scxml *node) Q_DECL_OVERRIDE
    {
        // init:
        if (!node->name.isEmpty()) {
            clazz.init.impl << QStringLiteral("table._name = QStringLiteral(\"") + cEscape(node->name) + QStringLiteral("\");");
            if (m_options.nameQObjects)
                clazz.init.impl << QStringLiteral("table.setObjectName(QStringLiteral(\"") + cEscape(node->name) + QStringLiteral("\"));");
        }
        foreach (AbstractState *s, node->initialStates) {
            clazz.init.impl << QStringLiteral("table.setInitialState(&state_") + mangledName(s) + QStringLiteral(");");
        }

        // visit the kids:
        m_parents.append(node);
        return true;
    }

    void endVisit(Scxml *) Q_DECL_OVERRIDE
    {
        m_parents.removeLast();
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
        return true;
    }

    void endVisit(State *) Q_DECL_OVERRIDE
    {
        m_parents.removeLast();
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
        if (State *parentState = parent->asState()) {
            parentName = QStringLiteral("state_") + mangledName(parentState);
        } else if (HistoryState *historyState = parent->asHistoryState()) {
            parentName = QStringLiteral("state_") + mangledName(historyState) + QStringLiteral("_defaultConfiguration");
        } else if (parent->asScxml()) {
            parentName = QStringLiteral("table");
        } else {
            Q_UNREACHABLE();
        }
        QString initializer = tName + QStringLiteral("(&") + parentName + QStringLiteral(", QList<QByteArray>()");
        foreach (const QString &event, node->events) {
            initializer += QStringLiteral(" << ") + qba(event);
        }
        initializer += QLatin1Char(')');
        clazz.constructor.initializer << initializer;

        // init:
        clazz.init.impl << parentName + QStringLiteral(".addTransition(&") + tName + QStringLiteral(");");
        if (node->type == Transition::Internal) {
            clazz.init.impl << tName + QStringLiteral(".setTransitionType(QAbstractTransition::InternalTransition);");
        }
        QString targets = tName + QStringLiteral(".setTargetStates(QList<QAbstractState *>()");
        foreach (AbstractState *target, node->targetStates) {
            targets += QStringLiteral(" << &state_") + mangledName(target);
        }
        clazz.init.impl << targets + QStringLiteral(");");

        // visit the kids:
        m_parents.append(node);
        return true;
    }

    void endVisit(Transition *) Q_DECL_OVERRIDE
    {
        m_parents.removeLast();
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
                               + stateName + QStringLiteral(");");

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

        name = CppDumper::mangleId(state->id);
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

            clazz.publicSlotDeclarations << QStringLiteral("void event_") + event.replace(QLatin1Char('.'), QLatin1Char('_')) + QStringLiteral("();");
            clazz.publicSlotDefinitions << QStringLiteral("void ") + m_mainClassName
                                           + QStringLiteral("::event_")
                                           + event.replace(QLatin1Char('.'), QLatin1Char('_'))
                                           + QStringLiteral("()\n{ submitEvent(") + qba(event)
                                           + QStringLiteral("); }");
        }
    }

private:
    MainClass &clazz;
    const QString &m_mainClassName;
    const CppDumpOptions &m_options;
    QHash<DocumentModel::AbstractState *, QString> m_mangledNames;
    QVector<Node *> m_parents;
    QSet<QString> m_knownEvents;
};
} // anonymous namespace

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
    h << l(headerStart);
    if (!options.namespaceName.isEmpty())
        h << l("namespace ") << options.namespaceName << l(" {") << endl;
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
      << l("    struct Data *data;") << endl
      << l("};") << endl;

    if (!options.namespaceName.isEmpty())
        h << l("} // namespace ") << options.namespaceName << endl;

    // Generate the .cpp file:
    cpp << l("#include \"") << headerName << l("\"") << endl
        << endl;
    if (!clazz.implIncludes.isEmpty()) {
        clazz.implIncludes.write(cpp, QStringLiteral("#include <"), QStringLiteral(">\n"));
        cpp << endl;
    }
    if (!options.namespaceName.isEmpty())
        cpp << l("namespace ") << options.namespaceName << l(" {") << endl;

    cpp << l("struct ") << mainClassName << l("::Data {") << endl;

    cpp << l("    Data(Scxml::StateTable &table)\n        : table(table)") << endl;
    clazz.constructor.initializer.write(cpp, QStringLiteral("        , "), QStringLiteral("\n"));
    cpp << l("    {}") << endl;

    cpp << endl;
//    dumpExecutableContent();
//    dumpInit();
    cpp << l("    bool init() {\n");
    clazz.init.impl.write(cpp, QStringLiteral("        "), QStringLiteral("\n"));
    cpp << endl
        << l("        return true;") << endl
        << l("    }") << endl;


    cpp << endl
        << l("    Scxml::StateTable &table;") << endl;
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
        << l("{ return data->init(); }") << endl;
    cpp << endl;
    clazz.publicSlotDefinitions.write(cpp, QStringLiteral("\n"), QStringLiteral("\n"));
    cpp << endl;

    if (!options.namespaceName.isEmpty())
        cpp << l("} // namespace ") << options.namespaceName << endl;
}

void CppDumper::dumpExecutableContent()
{
#if 0
    // dump special transitions
    bool inSlots = false;
    loopOnSubStates(table, [this, &inSlots](QState *state) -> bool {
        QString stateName = mangledName(state);
        if (ScxmlState *sState = qobject_cast<ScxmlState *>(state)) {
            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onEntryInstruction, sState->onEntryInstructions) {
                if (!onEntryInstruction->statements.isEmpty()) {
                    if (!inSlots) {
                        cpp << l("public slots:\n");
                        inSlots = true;
                    }
                    cpp << l("        void onEnter_") << stateName
                        << l("() {\n");
                    dumpInstructions(onEntryInstruction);
                    cpp << l("        }\n");
                }
            }
            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onExitInstruction, sState->onExitInstructions) {
                if (!onExitInstruction->statements.isEmpty()) {
                    if (!inSlots) {
                        cpp << l("public slots:\n");
                        inSlots = true;
                    }
                    cpp << l("        void onExit_") << stateName
                        << l("() {\n");
                    dumpInstructions(onExitInstruction);
                    cpp << l("        }\n\n");
                }
            }
        }
        for (int tIndex = 0; tIndex < state->transitions().size(); ++tIndex) {
            QAbstractTransition *t = state->transitions().at(tIndex);
            if (::Scxml::ScxmlTransition *scTransition = qobject_cast<::Scxml::ScxmlTransition *>(t)) {
                Q_UNIMPLEMENTED(); // FIXME
                /*if (scTransition->conditionalExp.isEmpty()) {
                    if (scTransition->instructionsOnTransition.statements.isEmpty())
                        continue;
                    if (!inSlots) {
                        cpp << l("public slots:\n");
                        inSlots = true;
                    }
                    cpp << l("        void on") << transitionName(scTransition, true, tIndex, stateName)
                        << l("() {\n");
                    dumpInstructions(&scTransition->instructionsOnTransition);
                    cpp << l("        }\n\n");
                } else*/ {
                    if (inSlots) { // avoid ?
                        cpp << l("public:\n");
                        inSlots = false;
                    }
//                    cpp << l("    class ") << transitionName(scTransition, true, tIndex, stateName) << " : Scxml::ScxmlBaseTransition {\n";
                    Q_UNIMPLEMENTED(); // FIXME
//                    if (!scTransition->conditionalExp.isEmpty()) { // we could cache the function calculating the test
//                        cpp << l("        bool eventTest(QEvent *event) Q_DECL_OVERRIDE {\n");
//                        cpp << l("            if (ScxmlBaseTransition::testEvent(e)\n");
//                        cpp << l("                    && table()->evalBool(\"")
//                            << cEscape(scTransition->conditionalExp) << l("\")\n");
//                        cpp << l("                return true;\n");
//                        cpp << l("            return false;\n");
//                        cpp << l("        }\n");
//                    }
//                    if (!scTransition->instructionsOnTransition.statements.isEmpty()) {
//                        cpp << l("    protected:\n");
//                        cpp << l("        void onTransition(QEvent *event) Q_DECL_OVERRIDE {\n");
//                        dumpInstructions(&scTransition->instructionsOnTransition);
//                        cpp << l("        }\n");
//                    }
//                    cpp << l("    };\n\n");
                }
            }
        }
        return true;
    }, Q_NULLPTR, Q_NULLPTR);
    if (inSlots) {
        cpp << l("public:\n");
    }
#endif
}

void CppDumper::dumpInstructions(const ExecutableContent::Instruction *i)
{
    DumpCppInstructionVisitor visitor(cpp);
    visitor.accept(i);
}

void CppDumper::dumpInit()
{
//    StringListDumper sIni; // state initialization
//    StringListDumper tIni; // transition initialization

//    loopOnSubStates(table, [this,&sIni,&tIni](QState *state) -> bool {
//        QByteArray rawStateName = state->objectName().toUtf8();
//        QString stateName = mangleId(rawStateName);
//        sIni << l("        table->addId(") << qba(rawStateName) << l(", &state_") << stateName
//             << l(");\n");
//        if (options.nameQObjects)
//            sIni << l("        state_") << stateName
//                 << l(".setObjectName(QStringLiteral(\"") << cEscape(rawStateName) << l("\"));\n");
//        if (state->childMode() == QState::ParallelStates)
//            sIni << l("        state_") << stateName << l(".setChildMode(QState::ParallelStates);\n");
//        if (ScxmlState *sState = qobject_cast<ScxmlState *>(state)) {
//            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onEntryInstruction, sState->onEntryInstructions) {
//                if (!onEntryInstruction->statements.isEmpty()) {
//                    sIni << l("        QObject::connect(&state_") << stateName
//                         << l(", &QAbstractState::entered, table, &") << mainClassName << l("::onEnter_")
//                         << stateName << l(");\n");
//                }
//            }
//            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onExitInstruction, sState->onExitInstructions) {
//                if (!onExitInstruction->statements.isEmpty()) {
//                    sIni << l("        QObject::connect(&state_") << stateName
//                         << l(", &QAbstractState::exited, table, &") << mainClassName << l("::onExit_")
//                         << stateName << l(");\n");
//                }
//            }
//        }

//        if (state->childMode() == QState::ExclusiveStates && state->initialState()) {
//            tIni << l("\n        state_") << stateName << l(".setInitialState(&state_")
//                 << mangledName(state->initialState()) << l(");\n");
//        }
//        tIni << "\n";
//        for (int tIndex = 0; tIndex < state->transitions().size(); ++tIndex) {
//            QAbstractTransition *t = state->transitions().at(tIndex);
//            if (::Scxml::ScxmlTransition *scTransition = qobject_cast<::Scxml::ScxmlTransition *>(t)) {
//                QString scName = transitionName(scTransition, false, tIndex, stateName);
//                Q_UNIMPLEMENTED(); // FIXME
////                if (scTransition->conditionalExp.isEmpty()) {
////                    if (!scTransition->instructionsOnTransition.statements.isEmpty()) {
////                        tIni << l("        QObject::connect(&") << scName
////                          << l(", &QAbstractTransition::triggered, table, &")
////                          << mainClassName << l("::onTransition_") << tIndex << l("_") << stateName
////                          << l(");\n");
////                    }
////                }
//                QList<QByteArray> targetStates = scTransition->targetIds();
//                if (targetStates.size() == 1) {
//                    tIni << l("        ") << scName << l(".setTargetState(&state_")
//                      << mangleId(targetStates.first()) << l(");\n");
//                } else if (targetStates.size() > 1) {
//                    tIni << l("        ") << scName << l(".setTargetStates(QList<QAbstractState *>() ");
//                    foreach (const QByteArray &tState, targetStates)
//                        tIni << l("\n            << &state_") << mangleId(tState);
//                    tIni << l(");\n");
//                }
//                tIni << l("        ") << scName << l(".init();\n");
//            }
//        }

//        return true;
//    }, nullptr, [this,&sIni](QAbstractState *state) -> void {
//        QByteArray rawStateName = state->objectName().toUtf8();
//        QString stateName = mangleId(rawStateName);
//        sIni << l("        table->addId(") << qba(rawStateName) << l(", &state_") << stateName
//             << l(");\n");
//        if (options.nameQObjects)
//            sIni << l("        state_") << stateName
//                 << l(".setObjectName(QStringLiteral(\"") << cEscape(rawStateName) << l("\"));\n");
//        if (ScxmlFinalState *sState = qobject_cast<ScxmlFinalState *>(state)) {
//            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onEntryInstruction, sState->onEntryInstructions) {
//                if (!onEntryInstruction->statements.isEmpty()) {
//                    sIni << l("        QObject::connect(&state_") << stateName
//                         << l(", &QAbstractState::entered, table, &") << mainClassName << l("::onEnter_")
//                         << stateName << l(");\n");
//                }
//            }
//            foreach (const ::Scxml::ExecutableContent::InstructionSequence *onExitInstruction, sState->onExitInstructions) {
//                if (!onExitInstruction->statements.isEmpty()) {
//                    sIni << l("        QObject::connect(&state_") << stateName
//                         << l(", &QAbstractState::exited, table, &") << mainClassName << l("::onExit_")
//                         << stateName << l(");\n");
//                }
//            }
//        }
//    });

//    cpp << l("    bool init() {\n");
//    sIni.write(cpp, QStringLiteral(""), QStringLiteral(""));

//    if (table->initialState()) {
//        cpp << l("\n        table->setInitialState(&state_")
//            << mangledName(table->initialState()) << l(");") << endl;
//    }

//    tIni.write(cpp, QStringLiteral(""), QStringLiteral(""));
//    cpp << endl;
//    cpp << l("        return true;\n");
//    cpp << l("    }\n");
}

QString CppDumper::transitionName(QAbstractTransition *transition, bool upcase, int tIndex,
                                  const QString &stateName)
{
    Q_ASSERT(transition->sourceState());
    if (tIndex < 0)
        tIndex = transition->sourceState()->transitions().indexOf(transition);
    Q_ASSERT(tIndex >= 0);
    QString stateNameStr;
    if (stateName.isEmpty())
        stateNameStr = mangledName(transition->sourceState());
    else
        stateNameStr = stateName;
    QString name = QStringLiteral("transition_%1_%2")
            .arg(stateNameStr, QString::number(tIndex));
    if (upcase)
        name[0] = QLatin1Char('T');
    return name;
}

QString CppDumper::mangledName(QAbstractState *state)
{
    QString mangledName = mangledStateNames.value(state);
    if (!mangledName.isEmpty())
        return mangledName;

    mangledName = mangleId(state->objectName());
    mangledStateNames.insert(state, mangledName);
    return mangledName;
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
