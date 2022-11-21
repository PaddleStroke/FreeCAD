/***************************************************************************
 *   Copyright (c) 2022 Pierre Boyer <pierrelouis.boyer google mail>       *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

#ifndef _PreComp_
# include <QVBoxLayout>
#endif

# include <QEvent>

#include <Gui/Application.h>
#include <Base/Tools.h>

#include "ViewProviderSketch.h"
#include "RenderingOrderWidget.h"

using namespace SketcherGui;

RenderingOrderWidget::RenderingOrderWidget(QWidget *parent, ViewProviderSketch* sketchView)
  : QWidget(parent)
    , sketchView(sketchView)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(list);

    label->setText(QApplication::translate("RenderingOrderWidget", "Rendering order (global):"));
    list->setToolTip(QApplication::translate("RenderingOrderWidget", "To change, drag and drop a geometry type to top or bottom"));
    list->setDragEnabled(true);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setResizeMode(QListView::Fixed);
    list->setSortingEnabled(false);
    list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    list->installEventFilter(this);

    loadSettings();
}

RenderingOrderWidget::~RenderingOrderWidget()
{
}

bool RenderingOrderWidget::eventFilter(QObject *object, QEvent *event)
{
    if (object == list && event->type() == QEvent::ChildRemoved) {
        saveSettings();
        if(sketchView)
            sketchView->updateColor();
    }

    return false;
}

void RenderingOrderWidget::saveSettings()
{
    int topid = list->item(0)->data(Qt::UserRole).toInt();
    int midid = list->item(1)->data(Qt::UserRole).toInt();
    int lowid = list->item(2)->data(Qt::UserRole).toInt();

    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/General");
    hGrp->SetInt("TopRenderGeometryId", topid);
    hGrp->SetInt("MidRenderGeometryId", midid);
    hGrp->SetInt("LowRenderGeometryId", lowid);
}

void RenderingOrderWidget::loadSettings()
{
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/General");

    // 1->Normal Geometry, 2->Construction, 3->External
    int topid = hGrp->GetInt("TopRenderGeometryId", 1);
    int midid = hGrp->GetInt("MidRenderGeometryId", 2);
    int lowid = hGrp->GetInt("LowRenderGeometryId", 3);

    {
        QSignalBlocker block(this);
        list->clear();

        QListWidgetItem* newItem = new QListWidgetItem;
        newItem->setData(Qt::UserRole, QVariant(topid));
        newItem->setText(topid == 1 ? tr("Normal Geometry") : topid == 2 ? tr("Construction Geometry") : tr("External Geometry"));
        list->insertItem(0, newItem);

        newItem = new QListWidgetItem;
        newItem->setData(Qt::UserRole, QVariant(midid));
        newItem->setText(midid == 1 ? tr("Normal Geometry") : midid == 2 ? tr("Construction Geometry") : tr("External Geometry"));
        list->insertItem(1, newItem);

        newItem = new QListWidgetItem;
        newItem->setData(Qt::UserRole, QVariant(lowid));
        newItem->setText(lowid == 1 ? tr("Normal Geometry") : lowid == 2 ? tr("Construction Geometry") : tr("External Geometry"));
        list->insertItem(2, newItem);
    }
}

void RenderingOrderWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        label->setText(QApplication::translate("RenderingOrderWidget", "Rendering order (global):"));
        list->setToolTip(QApplication::translate("RenderingOrderWidget", "To change, drag and drop a geometry type to top or bottom"));
        loadSettings();
    }
}