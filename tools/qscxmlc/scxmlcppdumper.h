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

#ifndef CPPDUMPER_H
#define CPPDUMPER_H

#include "scxmlglobals.h"
#include "scxmlstatemachine.h"

#include <QScxml/private/scxmlparser_p.h>

#include <QTextStream>

namespace Scxml {

struct MainClass;

struct CppDumpOptions
{
    CppDumpOptions()
        : usePrivateApi(false)
        , nameQObjects(false)
        , useCxx11(true)
    {}

    QString classname;
    QString namespaceName;
    bool usePrivateApi;
    bool nameQObjects;
    bool useCxx11;
};

class CppDumper
{
public:
    CppDumper(QTextStream &headerStream, QTextStream &cppStream, const QString &theHeaderName, const CppDumpOptions &theOptions)
        : h(headerStream)
        , cpp(cppStream)
        , headerName(theHeaderName)
        , m_doc(Q_NULLPTR)
        , options(theOptions)
    {}

    void dump(DocumentModel::ScxmlDocument *doc);

    static QString mangleId(const QString &id);

private:
    QTextStream &h;
    QTextStream &cpp;
    QString headerName;

    static QByteArray b(const char *str) { return QByteArray(str); }
    static QLatin1String l (const char *str) { return QLatin1String(str); }

    DocumentModel::ScxmlDocument *m_doc;
    QString mainClassName;
    CppDumpOptions options;
};

} // namespace Scxml
#endif // CPPDUMPER_H
