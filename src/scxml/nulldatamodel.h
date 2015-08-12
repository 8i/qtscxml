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

#ifndef NULLDATAMODEL_H
#define NULLDATAMODEL_H

#include <QtScxml/datamodel.h>

QT_BEGIN_NAMESPACE

namespace Scxml {

class SCXML_EXPORT NullDataModel: public DataModel
{
public:
    NullDataModel();
    ~NullDataModel();

    void setup() Q_DECL_OVERRIDE;

    QString evaluateToString(EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    bool evaluateToBool(EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    QVariant evaluateToVariant(EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    void evaluateToVoid(EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    void evaluateAssignment(EvaluatorId id, bool *ok) Q_DECL_OVERRIDE;
    bool evaluateForeach(EvaluatorId id, bool *ok, ForeachLoopBody *body) Q_DECL_OVERRIDE;

    void setEvent(const ScxmlEvent &event) Q_DECL_OVERRIDE;

    QVariant property(const QString &name) const Q_DECL_OVERRIDE;
    bool hasProperty(const QString &name) const Q_DECL_OVERRIDE;
    void setStringProperty(const QString &name, const QString &value, const QString &context, bool *ok) Q_DECL_OVERRIDE;

    virtual NullDataModel *asNullDataModel() Q_DECL_OVERRIDE;

private:
    class Data;
    Data *d;
};

} // Scxml namespace

QT_END_NAMESPACE

#endif // NULLDATAMODEL_H
