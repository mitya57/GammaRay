/*
  framegraphmodel.h

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GAMMARAY_FRAMEGRAPHMODEL_H
#define GAMMARAY_FRAMEGRAPHMODEL_H

#include <core/objectmodelbase.h>

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Qt3DRender {
class QFrameGraphNode;
}
QT_END_NAMESPACE

namespace GammaRay {

class FrameGraphModel : public ObjectModelBase<QAbstractItemModel>
{
    Q_OBJECT
public:
    explicit FrameGraphModel(QObject *parent = nullptr);
    ~FrameGraphModel();

    void setFrameGraph(Qt3DRender::QFrameGraphNode* frameGraph);

    QVariant data(const QModelIndex& index, int role) const override;
    int rowCount(const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;

private:
    void clear();
    void populateFromNode(Qt3DRender::QFrameGraphNode* node);
    QModelIndex indexForNode(Qt3DRender::QFrameGraphNode* node) const;

private:
    QHash<Qt3DRender::QFrameGraphNode*, Qt3DRender::QFrameGraphNode*> m_childParentMap;
    QHash<Qt3DRender::QFrameGraphNode*, QVector<Qt3DRender::QFrameGraphNode*> > m_parentChildMap;

};
}

#endif // GAMMARAY_FRAMEGRAPHMODEL_H