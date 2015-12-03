/******************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtScxml module.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#ifndef QSCXMLDATAMODEL_P_H
#define QSCXMLDATAMODEL_P_H

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

#include "qscxmldatamodel.h"
#include "qscxmlparser_p.h"

QT_BEGIN_NAMESPACE

class QScxmlDataModelPrivate
{
public:
    QScxmlDataModelPrivate(QScxmlStateMachine *stateMachine)
        : m_stateMachine(stateMachine)
    {
        Q_ASSERT(stateMachine != Q_NULLPTR);
    }

    static QScxmlDataModelPrivate *get(QScxmlDataModel *dataModel)
    { return dataModel->d; }

    static QScxmlDataModel *instantiateDataModel(DocumentModel::Scxml::DataModelType type,
                                                 QScxmlStateMachine *stateMachine);

    void setStateMachine(QScxmlStateMachine *stateMachine);

public:
    QScxmlStateMachine *m_stateMachine;
};

QT_END_NAMESPACE

#endif // QSCXMLDATAMODEL_P_H
