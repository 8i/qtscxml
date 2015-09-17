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

#include <QtScxml/qscxmlglobals.h>

#include <QVector>

QT_BEGIN_NAMESPACE

namespace QScxmlExecutableContent {

typedef int ContainerId;
enum { NoInstruction = -1 };
typedef qint32 StringId;
typedef QVector<StringId> StringIds;
enum { NoString = -1 };
typedef qint32 ByteArrayId;
enum { NoByteArray = -1 };
typedef qint32 *Instructions;

} // ExecutableContent namespace

QT_END_NAMESPACE

#endif // EXECUTABLECONTENT_H
