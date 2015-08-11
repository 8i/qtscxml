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

#include "datamodel.h"

using namespace Scxml;

class DataModel::Data
{
public:
    Data()
        : m_stateMachine(Q_NULLPTR)
    {}

    StateMachine *m_stateMachine;
};

DataModel::ForeachLoopBody::~ForeachLoopBody()
{}

DataModel::DataModel()
    : d(new Data)
{}

DataModel::~DataModel()
{
    delete d;
}

StateMachine *DataModel::stateMachine() const
{
    return d->m_stateMachine;
}

void DataModel::setTable(StateMachine *table)
{
    d->m_stateMachine = table;
}

NullDataModel *DataModel::asNullDataModel()
{
    return Q_NULLPTR;
}

EcmaScriptDataModel *DataModel::asEcmaScriptDataModel()
{
    return Q_NULLPTR;
}
