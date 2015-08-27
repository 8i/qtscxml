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

#ifndef SCXMLPARSER_P_H
#define SCXMLPARSER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "scxmlparser.h"

#include <QStringList>
#include <QString>

QT_BEGIN_NAMESPACE

namespace Scxml {
class ScxmlParser;

namespace DocumentModel {

struct XmlLocation
{
    int line;
    int column;

    XmlLocation(int theLine, int theColumn): line(theLine), column(theColumn) {}
};

struct If;
struct Send;
struct Invoke;
struct Script;
struct AbstractState;
struct State;
struct Transition;
struct HistoryState;
struct Scxml;
class NodeVisitor;
struct Node {
    XmlLocation xmlLocation;

    Node(const XmlLocation &theLocation): xmlLocation(theLocation) {}
    virtual ~Node();
    virtual void accept(NodeVisitor *visitor) = 0;

    virtual If *asIf() { return Q_NULLPTR; }
    virtual Send *asSend() { return Q_NULLPTR; }
    virtual Invoke *asInvoke() { return Q_NULLPTR; }
    virtual Script *asScript() { return Q_NULLPTR; }
    virtual State *asState() { return Q_NULLPTR; }
    virtual Transition *asTransition() { return Q_NULLPTR; }
    virtual HistoryState *asHistoryState() { return Q_NULLPTR; }
    virtual Scxml *asScxml() { return Q_NULLPTR; }

private:
    Q_DISABLE_COPY(Node)
};

struct DataElement: public Node
{
    QString id;
    QString src;
    QString expr;
    QString content;

    DataElement(const XmlLocation &xmlLocation): Node(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Param: public Node
{
    QString name;
    QString expr;
    QString location;

    Param(const XmlLocation &xmlLocation): Node(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct DoneData: public Node
{
    QString contents;
    QString expr;
    QVector<Param *> params;

    DoneData(const XmlLocation &xmlLocation): Node(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Instruction: public Node
{
    Instruction(const XmlLocation &xmlLocation): Node(xmlLocation) {}
    virtual ~Instruction() {}
};

typedef QVector<Instruction *> InstructionSequence;
typedef QVector<InstructionSequence *> InstructionSequences;

struct Send: public Instruction
{
    QString event;
    QString eventexpr;
    QString type;
    QString typeexpr;
    QString target;
    QString targetexpr;
    QString id;
    QString idLocation;
    QString delay;
    QString delayexpr;
    QStringList namelist;
    QVector<Param *> params;
    QString content;

    Send(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    Send *asSend() Q_DECL_OVERRIDE { return this; }
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Invoke: public Instruction
{
    QString type;
    QString typeexpr;
    QString src;
    QString srcexpr;
    QString id;
    QString idLocation;
    QStringList namelist;
    bool autoforward;
    QVector<Param *> params;
    InstructionSequence finalize;

    Invoke(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    Invoke *asInvoke() Q_DECL_OVERRIDE { return this; }
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Raise: public Instruction
{
    QString event;

    Raise(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Log: public Instruction
{
    QString label, expr;

    Log(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Script: public Instruction
{
    QString src;
    QString content;

    Script(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    Script *asScript() Q_DECL_OVERRIDE { return this; }
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Assign: public Instruction
{
    QString location;
    QString expr;
    QString content;

    Assign(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct If: public Instruction
{
    QStringList conditions;
    InstructionSequences blocks;

    If(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    If *asIf() Q_DECL_OVERRIDE { return this; }
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Foreach: public Instruction
{
    QString array;
    QString item;
    QString index;
    InstructionSequence block;

    Foreach(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Cancel: public Instruction
{
    QString sendid;
    QString sendidexpr;

    Cancel(const XmlLocation &xmlLocation): Instruction(xmlLocation) {}
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct StateOrTransition: public Node
{
    StateOrTransition(const XmlLocation &xmlLocation): Node(xmlLocation) {}
};

struct StateContainer
{
    StateContainer()
        : parent(Q_NULLPTR)
    {}

    StateContainer *parent;

    virtual ~StateContainer() {}
    virtual void add(StateOrTransition *s) = 0;
    virtual AbstractState *asAbstractState() { return Q_NULLPTR; }
    virtual State *asState() { return Q_NULLPTR; }
    virtual Scxml *asScxml() { return Q_NULLPTR; }
    Node *asNode() { return dynamic_cast<Node *>(this); }
};

struct AbstractState: public StateContainer
{
    QString id;

    AbstractState *asAbstractState() Q_DECL_OVERRIDE { return this; }
};

struct State: public AbstractState, public StateOrTransition
{
    enum Type { Normal, Parallel, Initial, Final };

    QStringList initial;
    QVector<DataElement *> dataElements;
    QVector<StateOrTransition *> children;
    InstructionSequences onEntry;
    InstructionSequences onExit;
    DoneData *doneData;
    Type type;

    QVector<AbstractState *> initialStates; // filled during verification

    State(const XmlLocation &xmlLocation)
        : StateOrTransition(xmlLocation)
        , doneData(Q_NULLPTR)
        , type(Normal)
    {}

    void add(StateOrTransition *s) Q_DECL_OVERRIDE
    {
        Q_ASSERT(s);
        children.append(s);
    }

    State *asState() Q_DECL_OVERRIDE { return this; }

    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Transition: public StateOrTransition
{
    enum Type { External, Internal };
    QStringList events;
    QScopedPointer<QString> condition;
    QStringList targets;
    InstructionSequence instructionsOnTransition;
    Type type;

    QVector<AbstractState *> targetStates; // filled during verification

    Transition(const XmlLocation &xmlLocation)
        : StateOrTransition(xmlLocation)
        , type(External)
    {}

    Transition *asTransition() Q_DECL_OVERRIDE { return this; }

    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct HistoryState: public AbstractState, public StateOrTransition
{
    enum Type { Deep, Shallow };
    Type type;
    QVector<StateOrTransition *> children;

    HistoryState(const XmlLocation &xmlLocation)
        : StateOrTransition(xmlLocation)
        , type(Shallow)
    {}

    void add(StateOrTransition *s) Q_DECL_OVERRIDE
    {
        Q_ASSERT(s);
        children.append(s);
    }

    Transition *defaultConfiguration()
    { return children.isEmpty() ? Q_NULLPTR : children.first()->asTransition(); }

    HistoryState *asHistoryState() Q_DECL_OVERRIDE { return this; }
    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct Scxml: public StateContainer, public Node
{
    enum DataModelType {
        NullDataModel,
        JSDataModel
    };
    enum BindingMethod {
        EarlyBinding,
        LateBinding
    };

    QStringList initial;
    QString name;
    QString qtClassname;
    DataModelType dataModel;
    BindingMethod binding;
    QVector<StateOrTransition *> children;
    QVector<DataElement *> dataElements;
    QScopedPointer<Script> script;
    DocumentModel::InstructionSequence initialSetup;

    QVector<AbstractState *> initialStates; // filled during verification

    Scxml(const XmlLocation &xmlLocation)
        : Node(xmlLocation)
        , dataModel(NullDataModel)
        , binding(EarlyBinding)
    {}

    void add(StateOrTransition *s) Q_DECL_OVERRIDE
    {
        Q_ASSERT(s);
        children.append(s);
    }

    Scxml *asScxml() { return this; }

    void accept(NodeVisitor *visitor) Q_DECL_OVERRIDE;
};

struct ScxmlDocument
{
    Scxml *root;
    QVector<AbstractState *> allStates;
    QVector<Transition *> allTransitions;
    QVector<Node *> allNodes;
    QVector<InstructionSequence *> allSequences;
    bool isVerified;

    ScxmlDocument()
        : root(Q_NULLPTR)
        , isVerified(false)
    {}

    ~ScxmlDocument()
    {
        delete root;
        qDeleteAll(allNodes);
        qDeleteAll(allSequences);
    }

    State *newState(StateContainer *parent, State::Type type, const XmlLocation &xmlLocation)
    {
        Q_ASSERT(parent);
        State *s = newNode<State>(xmlLocation);
        s->parent = parent;
        s->type = type;
        allStates.append(s);
        parent->add(s);
        return s;
    }

    HistoryState *newHistoryState(StateContainer *parent, const XmlLocation &xmlLocation)
    {
        Q_ASSERT(parent);
        HistoryState *s = newNode<HistoryState>(xmlLocation);
        s->parent = parent;
        allStates.append(s);
        parent->add(s);
        return s;
    }

    Transition *newTransition(StateContainer *parent, const XmlLocation &xmlLocation)
    {
        Transition *t = newNode<Transition>(xmlLocation);
        allTransitions.append(t);
        parent->add(t);
        return t;
    }

    template<typename T>
    T *newNode(const XmlLocation &xmlLocation)
    {
        T *node = new T(xmlLocation);
        allNodes.append(node);
        return node;
    }

    InstructionSequence *newSequence(InstructionSequences *container)
    {
        Q_ASSERT(container);
        InstructionSequence *is = new InstructionSequence;
        allSequences.append(is);
        container->append(is);
        return is;
    }
};

class Q_SCXML_EXPORT NodeVisitor
{
public:
    virtual ~NodeVisitor();

    virtual void visit(DataElement *) {}
    virtual void visit(Param *) {}
    virtual bool visit(DoneData *) { return true; }
    virtual void endVisit(DoneData *) {}
    virtual bool visit(Send *) { return true; }
    virtual void endVisit(Send *) {}
    virtual bool visit(Invoke *) { return true; }
    virtual void endVisit(Invoke *) {}
    virtual void visit(Raise *) {}
    virtual void visit(Log *) {}
    virtual void visit(Script *) {}
    virtual void visit(Assign *) {}
    virtual bool visit(If *) { return true; }
    virtual void endVisit(If *) {}
    virtual bool visit(Foreach *) { return true; }
    virtual void endVisit(Foreach *) {}
    virtual void visit(Cancel *) {}
    virtual bool visit(State *) { return true; }
    virtual void endVisit(State *) {}
    virtual bool visit(Transition *) { return true; }
    virtual void endVisit(Transition *) {}
    virtual bool visit(HistoryState *) { return true; }
    virtual void endVisit(HistoryState *) {}
    virtual bool visit(Scxml *) { return true; }
    virtual void endVisit(Scxml *) {}

    void visit(InstructionSequence *sequence)
    {
        Q_ASSERT(sequence);
        Q_FOREACH (Instruction *instruction, *sequence) {
            Q_ASSERT(instruction);
            instruction->accept(this);
        }
    }

    void visit(const QVector<DataElement *> &dataElements)
    {
        Q_FOREACH (DataElement *dataElement, dataElements) {
            Q_ASSERT(dataElement);
            dataElement->accept(this);
        }
    }

    void visit(const QVector<StateOrTransition *> &children)
    {
        Q_FOREACH (StateOrTransition *child, children) {
            Q_ASSERT(child);
            child->accept(this);
        }
    }

    void visit(const InstructionSequences &sequences)
    {
        Q_FOREACH (InstructionSequence *sequence, sequences) {
            Q_ASSERT(sequence);
            visit(sequence);
        }
    }

    void visit(const QVector<Param *> &params)
    {
        Q_FOREACH (Param *param, params) {
            Q_ASSERT(param);
            param->accept(this);
        }
    }
};

} // DocumentModel namespace

struct ParserState {
    enum Kind {
        Scxml,
        State,
        Parallel,
        Transition,
        Initial,
        Final,
        OnEntry,
        OnExit,
        History,
        Raise,
        If,
        ElseIf,
        Else,
        Foreach,
        Log,
        DataModel,
        Data,
        DataElement,
        Assign,
        DoneData,
        Content,
        Param,
        Script,
        Send,
        Cancel,
        Invoke,
        Finalize,
        None
    };
    Kind kind;
    QString chars;
    DocumentModel::Instruction *instruction;
    DocumentModel::InstructionSequence *instructionContainer;

    bool collectChars();

    ParserState(Kind someKind = None)
        : kind(someKind)
        , instruction(0)
        , instructionContainer(0)
    {}
    ~ParserState() { }

    bool validChild(ParserState::Kind child) const;
    static bool validChild(ParserState::Kind parent, ParserState::Kind child);
    static bool isExecutableContent(ParserState::Kind kind);
};

class DefaultLoader: public ScxmlParser::Loader
{
public:
    DefaultLoader(ScxmlParser *parser)
        : Loader(parser)
    {}

    QByteArray load(const QString &name, const QString &baseDir, bool *ok) Q_DECL_OVERRIDE;
};

class Q_SCXML_EXPORT ScxmlParserPrivate
{
public:
    static ScxmlParserPrivate *get(ScxmlParser *parser);

    ScxmlParserPrivate(ScxmlParser *parser, QXmlStreamReader *reader);

    ScxmlParser *parser() const;
    DocumentModel::ScxmlDocument *scxmlDocument();

    QString fileName() const;
    void setFilename(const QString &fileName);

    void parse();
    QByteArray load(const QString &name, bool *ok) const;

    ScxmlParser::State state() const;
    QVector<ScxmlError> errors() const;

    void addError(const QString &msg);
    void addError(const DocumentModel::XmlLocation &location, const QString &msg);

private:
    DocumentModel::AbstractState *currentParent() const;
    DocumentModel::XmlLocation xmlLocation() const;
    bool maybeId(const QXmlStreamAttributes &attributes, QString *id);
    bool checkAttributes(const QXmlStreamAttributes &attributes, const char *attribStr);
    bool checkAttributes(const QXmlStreamAttributes &attributes, QStringList requiredNames,
                         QStringList optionalNames);

private:
    ScxmlParser *m_parser;
    QString m_fileName;
    QSet<QString> m_allIds;

    QScopedPointer<DocumentModel::ScxmlDocument> m_doc;
    DocumentModel::StateContainer *m_currentParent;
    DocumentModel::StateContainer *m_currentState;
    DefaultLoader m_defaultLoader;
    ScxmlParser::Loader *m_loader;
    QStringList m_namespacesToIgnore;

    QXmlStreamReader *m_reader;
    QVector<ParserState> m_stack;
    ScxmlParser::State m_state;
    QVector<ScxmlError> m_errors;
};

} // namespace Scxml

QT_END_NAMESPACE

#endif // SCXMLPARSER_P_H
