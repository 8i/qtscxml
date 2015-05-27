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

#ifndef EXECUTABLECONTENT_H
#define EXECUTABLECONTENT_H

#include "scxmlglobals.h"

#include <QByteArray>
#include <QString>
#include <QVariant>

namespace Scxml {

class StateTable;

namespace ExecutableContent {

typedef int ContainerId;
enum { NoInstruction = -1 };
typedef qint32 StringId;
enum { NoString = -1 };
typedef qint32 ByteArrayId;
typedef qint32 *Instructions;

class SCXML_EXPORT ExecutionEngine
{
public:
    ExecutionEngine(StateTable *table);
    ~ExecutionEngine();

    void setStringTable(const QVector<QString> &strings);
    QString string(StringId id) const;

    void setByteArrayTable(const QVector<QByteArray> &byteArrays);
    QByteArray byteArray(ByteArrayId id) const;

    void setInstructions(const QVector<qint32> &instructions);
    bool execute(ContainerId ip, const QVariant &extraData = QVariant());

private:
    class Data;
    Data *data;
};

} // ExecutableContent namespace
} // namespace Scxml

#endif // EXECUTABLECONTENT_H
