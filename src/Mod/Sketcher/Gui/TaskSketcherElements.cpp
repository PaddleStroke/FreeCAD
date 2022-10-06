/***************************************************************************
 *   Copyright (c) 2014 Abdullah Tahiri <abdullah.tahiri.yo@gmail.com>     *
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
# include <QContextMenuEvent>
# include <QMenu>
# include <QRegExp>
# include <QShortcut>
# include <QString>
# include <QImage>
# include <QPixmap>
#endif

#include "TaskSketcherElements.h"
#include "ui_TaskSketcherElements.h"
#include "EditDatumDialog.h"
#include "ViewProviderSketch.h"

#include <Mod/Sketcher/App/SketchObject.h>
#include <Mod/Sketcher/App/GeometryFacade.h>

#include <Base/Tools.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/SelectionObject.h>
#include <Gui/BitmapFactory.h>
#include <Gui/ViewProvider.h>
#include <Gui/BitmapFactory.h>

#include <Gui/Command.h>

using namespace SketcherGui;
using namespace Gui::TaskView;

/// Inserts a QAction into an existing menu
/// ICONSTR is the string of the icon in the resource file
/// NAMESTR is the text appearing in the contextual menuAction
/// CMDSTR is the string registered in the commandManager
/// FUNC is the name of the member function to be executed on selection of the menu item
/// ACTSONSELECTION is a true/false value to activate the command only if a selection is made
#define CONTEXT_ITEM(ICONSTR,NAMESTR,CMDSTR,FUNC,ACTSONSELECTION) \
QIcon icon_ ## FUNC( Gui::BitmapFactory().pixmap(ICONSTR) ); \
    QAction* constr_ ## FUNC = menu.addAction(icon_ ## FUNC,tr(NAMESTR), this, SLOT(FUNC()), \
        QKeySequence(QString::fromUtf8(Gui::Application::Instance->commandManager().getCommandByName(CMDSTR)->getAccel()))); \
    if(ACTSONSELECTION) constr_ ## FUNC->setEnabled(!items.isEmpty()); else constr_ ## FUNC->setEnabled(true);

/// Defines the member function corresponding to the CONTEXT_ITEM macro
#define CONTEXT_MEMBER_DEF(CMDSTR,FUNC) \
void ElementView::FUNC(){ \
   Gui::Application::Instance->commandManager().runCommandByName(CMDSTR);}

ElementView::ElementView(QWidget *parent) : QListWidget(parent) {}

ElementView::~ElementView() {}

void ElementView::contextMenuEvent (QContextMenuEvent* event)
{
    QMenu menu;
    QList<QListWidgetItem *> items = selectedItems();

    // CONTEXT_ITEM(ICONSTR,NAMESTR,CMDSTR,FUNC,ACTSONSELECTION)
    CONTEXT_ITEM("Constraint_PointOnPoint","Point Coincidence","Sketcher_ConstrainCoincident",doPointCoincidence,true)
    CONTEXT_ITEM("Constraint_PointOnObject","Point on Object","Sketcher_ConstrainPointOnObject",doPointOnObjectConstraint,true)
    CONTEXT_ITEM("Constraint_Vertical","Vertical Constraint","Sketcher_ConstrainVertical", doVerticalConstraint,true)
    CONTEXT_ITEM("Constraint_Horizontal","Horizontal Constraint","Sketcher_ConstrainHorizontal",doHorizontalConstraint,true)
    CONTEXT_ITEM("Constraint_Parallel","Parallel Constraint","Sketcher_ConstrainParallel",doParallelConstraint,true)
    CONTEXT_ITEM("Constraint_Perpendicular","Perpendicular Constraint","Sketcher_ConstrainPerpendicular",doPerpendicularConstraint,true)
    CONTEXT_ITEM("Constraint_Tangent","Tangent Constraint","Sketcher_ConstrainTangent",doTangentConstraint,true)
    CONTEXT_ITEM("Constraint_EqualLength","Equal Length","Sketcher_ConstrainEqual",doEqualConstraint,true)
    CONTEXT_ITEM("Constraint_Symmetric","Symmetric","Sketcher_ConstrainSymmetric",doSymmetricConstraint,true)
    CONTEXT_ITEM("Constraint_Block","Block Constraint","Sketcher_ConstrainBlock",doBlockConstraint,true)

    CONTEXT_ITEM("Constraint_Lock","Lock Constraint","Sketcher_ConstrainLock",doLockConstraint,true)
    CONTEXT_ITEM("Constraint_HorizontalDistance","Horizontal Distance","Sketcher_ConstrainDistanceX",doHorizontalDistance,true)
    CONTEXT_ITEM("Constraint_VerticalDistance","Vertical Distance","Sketcher_ConstrainDistanceY",doVerticalDistance,true)
    CONTEXT_ITEM("Constraint_Length","Length Constraint","Sketcher_ConstrainDistance",doLengthConstraint,true)
    CONTEXT_ITEM("Constraint_Radius","Radius Constraint","Sketcher_ConstrainRadius",doRadiusConstraint,true)
    CONTEXT_ITEM("Constraint_Diameter","Diameter Constraint","Sketcher_ConstrainDiameter",doDiameterConstraint,true)
    CONTEXT_ITEM("Constraint_Radiam","Radiam Constraint","Sketcher_ConstrainRadiam",doRadiamConstraint,true)
    CONTEXT_ITEM("Constraint_InternalAngle","Angle Constraint","Sketcher_ConstrainAngle",doAngleConstraint,true)

    menu.addSeparator();

    CONTEXT_ITEM("Sketcher_ToggleConstruction","Toggle construction line","Sketcher_ToggleConstruction",doToggleConstruction,true)

    menu.addSeparator();

    CONTEXT_ITEM("Sketcher_SelectConstraints","Select Constraints","Sketcher_SelectConstraints",doSelectConstraints,true)
    CONTEXT_ITEM("Sketcher_SelectOrigin","Select Origin","Sketcher_SelectOrigin",doSelectOrigin,false)
    CONTEXT_ITEM("Sketcher_SelectHorizontalAxis","Select Horizontal Axis","Sketcher_SelectHorizontalAxis",doSelectHAxis,false)
    CONTEXT_ITEM("Sketcher_SelectVerticalAxis","Select Vertical Axis","Sketcher_SelectVerticalAxis",doSelectVAxis,false)

    menu.addSeparator();

    QAction* remove = menu.addAction(tr("Delete"), this, SLOT(deleteSelectedItems()),
        QKeySequence(QKeySequence::Delete));
    remove->setEnabled(!items.isEmpty());

    menu.menuAction()->setIconVisibleInMenu(true);

    menu.exec(event->globalPos());
}

CONTEXT_MEMBER_DEF("Sketcher_ConstrainCoincident",doPointCoincidence)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainPointOnObject",doPointOnObjectConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainVertical",doVerticalConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainHorizontal",doHorizontalConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainParallel",doParallelConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainPerpendicular",doPerpendicularConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainTangent",doTangentConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainEqual",doEqualConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainSymmetric",doSymmetricConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainBlock",doBlockConstraint)

CONTEXT_MEMBER_DEF("Sketcher_ConstrainLock",doLockConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainDistanceX",doHorizontalDistance)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainDistanceY",doVerticalDistance)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainDistance",doLengthConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainRadius",doRadiusConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainDiameter",doDiameterConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainRadiam",doRadiamConstraint)
CONTEXT_MEMBER_DEF("Sketcher_ConstrainAngle",doAngleConstraint)

CONTEXT_MEMBER_DEF("Sketcher_ToggleConstruction",doToggleConstruction)

CONTEXT_MEMBER_DEF("Sketcher_SelectConstraints",doSelectConstraints)
CONTEXT_MEMBER_DEF("Sketcher_SelectOrigin",doSelectOrigin)
CONTEXT_MEMBER_DEF("Sketcher_SelectHorizontalAxis",doSelectHAxis)
CONTEXT_MEMBER_DEF("Sketcher_SelectVerticalAxis",doSelectVAxis)

void ElementView::deleteSelectedItems()
{
    App::Document* doc = App::GetApplication().getActiveDocument();
    if (!doc)
        return;

    doc->openTransaction("Delete element");
    std::vector<Gui::SelectionObject> sel = Gui::Selection().getSelectionEx(doc->getName());
    for (std::vector<Gui::SelectionObject>::iterator ft = sel.begin(); ft != sel.end(); ++ft) {
        Gui::ViewProvider* vp = Gui::Application::Instance->getViewProvider(ft->getObject());
        if (vp) {
            vp->onDelete(ft->getSubNames());
        }
    }
    doc->commitTransaction();
}


void ElementView::keyPressEvent(QKeyEvent * event)
{
    switch (event->key())
    {
      case Qt::Key_Z:
        // signal
        onFilterShortcutPressed();
        break;
      default:
        QListWidget::keyPressEvent( event );
        break;
    }
}

void ElementView::mousePressEvent(QMouseEvent* event) {

    if (event->button() == Qt::RightButton) {
        QListWidgetItem* item = itemAt(event->pos());
        ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
        itemData->rightClicked = true;
    }

    QListWidget::mousePressEvent(event);
}

// ----------------------------------------------------------------------------

void MyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.data(Qt::UserRole).canConvert<ElementData*>()) {
        ElementData* itemData = qvariant_cast<ElementData*>(index.data(Qt::UserRole));

        int border = 1;
        int rectBorder = 1;
        int height = option.rect.height();
        int x0 = option.rect.x() + 4;
        int iconsize = height - 2 * border;
        int btny = option.rect.y() + border;

        if (itemData->isLineSelected || itemData->isStartingPointSelected || itemData->isEndPointSelected || itemData->isMidPointSelected) { //option.state & QStyle::State_Selected
            painter->fillRect(option.rect, option.palette.highlight());
            if (!itemData->isLineSelected && itemData->GeometryType != Part::GeomPoint::getClassTypeId()) {
                QRect rect0(x0 + border, btny, iconsize, iconsize);
                painter->fillRect(rect0, option.palette.base());
            }
            if (!itemData->isStartingPointSelected && itemData->GeometryType != Part::GeomCircle::getClassTypeId() && itemData->GeometryType != Part::GeomEllipse::getClassTypeId()) {
                QRect rect1(x0 + height + border, btny, iconsize, iconsize);
                painter->fillRect(rect1, option.palette.base());
            }
            if (!itemData->isEndPointSelected && itemData->GeometryType != Part::GeomCircle::getClassTypeId() && itemData->GeometryType != Part::GeomEllipse::getClassTypeId() && itemData->GeometryType != Part::GeomPoint::getClassTypeId()) {
                QRect rect2(x0 + height * 2 + border, btny, iconsize, iconsize);
                painter->fillRect(rect2, option.palette.base());
            }
            if (!itemData->isMidPointSelected && itemData->GeometryType != Part::GeomLineSegment::getClassTypeId() && itemData->GeometryType != Part::GeomBSplineCurve::getClassTypeId() && itemData->GeometryType != Part::GeomPoint::getClassTypeId()){
                QRect rect3(x0 + height * 3 + border, btny, iconsize, iconsize);
                painter->fillRect(rect3, option.palette.base());
            }
        }

        painter->drawPixmap(x0 + border             , btny, itemData->icon0.pixmap(iconsize, iconsize));
        painter->drawPixmap(x0 + border + height    , btny, itemData->icon1.pixmap(iconsize, iconsize));
        painter->drawPixmap(x0 + border + height * 2, btny, itemData->icon2.pixmap(iconsize, iconsize));
        painter->drawPixmap(x0 + border + height * 3, btny, itemData->icon3.pixmap(iconsize, iconsize));


        /*Buttons
        QStyleOptionButton opt0;
        QStyleOptionButton opt1;
        QStyleOptionButton opt2;
        QStyleOptionButton opt3;
        opt0.state |= QStyle::State_Enabled;
        opt1.state |= QStyle::State_Enabled;
        opt2.state |= QStyle::State_Enabled;
        opt3.state |= QStyle::State_Enabled;

        opt0.rect.setRect(x0 + border, btny, buttonSize, buttonSize);
        opt1.rect.setRect(x0 + height + border, btny, buttonSize, buttonSize);
        opt2.rect.setRect(x0 + height * 2 + border, btny, buttonSize, buttonSize);
        opt3.rect.setRect(x0 + height * 3 + border, btny, buttonSize, buttonSize);

        opt0.icon = itemData.icon0;
        opt1.icon = itemData.icon1;
        opt2.icon = itemData.icon2;
        opt3.icon = itemData.icon3;
        QSize btnSize = QSize(16, 16);
        opt0.iconSize = btnSize; opt1.iconSize = btnSize; opt2.iconSize = btnSize; opt3.iconSize = btnSize;

        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt0, painter, 0);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt1, painter, 0);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt2, painter, 0);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt3, painter, 0);*/

        //Label : 
        painter->drawText(x0 + height * 4 + 3 * border, option.rect.y() + height - 5, itemData->label);

    }
}

bool MyDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() == QEvent::MouseMove) {
        /*if (index != m_lastUnderMouse) {
            if (m_lastUnderMouse.isValid()) {
                model->setData(m_lastUnderMouse, (int)Normal, Qt::UserRole);
                emit needsUpdate(m_lastUnderMouse);
            }
            if (index.isValid() && index.column() == ButtonColumn) {
                model->setData(index, (int)Hovered, Qt::UserRole);
                emit needsUpdate(index);
                m_lastUnderMouse = index;
            }
            else {
                m_lastUnderMouse = QModelIndex();
            }
        }*/
    }
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent* mEvent = (QMouseEvent*)event;
        QPoint point = mEvent->pos();
        int xPos = point.x();
        int border = 1;
        ElementData* itemData = qvariant_cast<ElementData*>(index.data(Qt::UserRole));
        if(xPos < option.rect.x() + 4 + option.rect.height() + border)
            itemData->clickedOn = 0; //0 = edge
        else if (xPos < option.rect.x() + 4 + option.rect.height() * 2 + border)
            itemData->clickedOn = 1; //1 = start
        else if (xPos < option.rect.x() + 4 + option.rect.height() * 3 + border)
            itemData->clickedOn = 2; //2 = end
        else if (xPos < option.rect.x() + 4 + option.rect.height() * 4 + border)
            itemData->clickedOn = 3; //3 = mid
        else
            itemData->clickedOn = 4; //4 = none

    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
// ----------------------------------------------------------------------------

/* TRANSLATOR SketcherGui::TaskSketcherElements */

TaskSketcherElements::TaskSketcherElements(ViewProviderSketch *sketchView)
    : TaskBox(Gui::BitmapFactory().pixmap("document-new"),tr("Elements"),true, nullptr)
    , sketchView(sketchView)
    , ui(new Ui_TaskSketcherElements())
    , focusItemIndex(-1)
    , previouslySelectedItemIndex(-1)
    , isNamingBoxChecked(false)
{
    Sketcher_Element_Arc_Edge = MultIcon("Sketcher_Element_Arc_Edge");
    Sketcher_Element_Arc_EndPoint = MultIcon("Sketcher_Element_Arc_EndPoint");
    Sketcher_Element_Arc_MidPoint = MultIcon("Sketcher_Element_Arc_MidPoint");
    Sketcher_Element_Arc_StartingPoint = MultIcon("Sketcher_Element_Arc_StartingPoint");
    Sketcher_Element_Circle_Edge = MultIcon("Sketcher_Element_Circle_Edge");
    Sketcher_Element_Circle_MidPoint = MultIcon("Sketcher_Element_Circle_MidPoint");
    Sketcher_Element_Line_Edge = MultIcon("Sketcher_Element_Line_Edge");
    Sketcher_Element_Line_EndPoint = MultIcon("Sketcher_Element_Line_EndPoint");
    Sketcher_Element_Line_StartingPoint = MultIcon("Sketcher_Element_Line_StartingPoint");
    Sketcher_Element_Point_StartingPoint = MultIcon("Sketcher_Element_Point_StartingPoint");
    Sketcher_Element_Ellipse_Edge = MultIcon("Sketcher_Element_Ellipse_Edge_2");
    Sketcher_Element_Ellipse_MidPoint = MultIcon("Sketcher_Element_Ellipse_CentrePoint");
    Sketcher_Element_ArcOfEllipse_Edge = MultIcon("Sketcher_Element_Elliptical_Arc_Edge");
    Sketcher_Element_ArcOfEllipse_MidPoint = MultIcon("Sketcher_Element_Elliptical_Arc_Centre_Point");
    Sketcher_Element_ArcOfEllipse_StartingPoint = MultIcon("Sketcher_Element_Elliptical_Arc_Start_Point");
    Sketcher_Element_ArcOfEllipse_EndPoint = MultIcon("Sketcher_Element_Elliptical_Arc_End_Point");
    Sketcher_Element_ArcOfHyperbola_Edge = MultIcon("Sketcher_Element_Hyperbolic_Arc_Edge");
    Sketcher_Element_ArcOfHyperbola_MidPoint = MultIcon("Sketcher_Element_Hyperbolic_Arc_Centre_Point");
    Sketcher_Element_ArcOfHyperbola_StartingPoint = MultIcon("Sketcher_Element_Hyperbolic_Arc_Start_Point");
    Sketcher_Element_ArcOfHyperbola_EndPoint = MultIcon("Sketcher_Element_Hyperbolic_Arc_End_Point");
    Sketcher_Element_ArcOfParabola_Edge = MultIcon("Sketcher_Element_Parabolic_Arc_Edge");
    Sketcher_Element_ArcOfParabola_MidPoint = MultIcon("Sketcher_Element_Parabolic_Arc_Centre_Point");
    Sketcher_Element_ArcOfParabola_StartingPoint = MultIcon("Sketcher_Element_Parabolic_Arc_Start_Point");
    Sketcher_Element_ArcOfParabola_EndPoint = MultIcon("Sketcher_Element_Parabolic_Arc_End_Point");
    Sketcher_Element_BSpline_Edge = MultIcon("Sketcher_Element_BSpline_Edge");
    Sketcher_Element_BSpline_StartingPoint = MultIcon("Sketcher_Element_BSpline_StartPoint");
    Sketcher_Element_BSpline_EndPoint = MultIcon("Sketcher_Element_BSpline_EndPoint");
    none = MultIcon("Sketcher_Element_SelectionTypeInvalid");

    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
#ifdef Q_OS_MAC
    QString cmdKey = QString::fromUtf8("\xe2\x8c\x98"); // U+2318
#else
    // translate the text (it's offered by Qt's translation files)
    // but avoid being picked up by lupdate
    const char* ctrlKey = "Ctrl";
    QString cmdKey = QShortcut::tr(ctrlKey);
#endif
    ui->Explanation->setText(tr("(Z) next valid type"));

    ui->listWidgetElements->setItemDelegate(new MyDelegate);
    ui->listWidgetElements->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->listWidgetElements->setEditTriggers(QListWidget::NoEditTriggers);
    ui->listWidgetElements->setMouseTracking(true);

    createSettingsButtonActions();

    // connecting the needed signals
    QObject::connect(
        ui->listWidgetElements, SIGNAL(itemPressed(QListWidgetItem *)),
        this                     , SLOT  (on_listWidgetElements_itemPressed(QListWidgetItem *))
       );
    QObject::connect(
        ui->listWidgetElements, SIGNAL(itemEntered(QListWidgetItem *)),
        this                     , SLOT  (on_listWidgetElements_itemEntered(QListWidgetItem *))
       );
    QObject::connect(
        ui->listWidgetElements, SIGNAL(onFilterShortcutPressed()),
        this                     , SLOT  (on_listWidgetElements_filterShortcutPressed())
       );
    QObject::connect(
        ui->listMultiFilter, SIGNAL(itemChanged(QListWidgetItem*)),
        this, SLOT(on_listMultiFilter_itemChanged(QListWidgetItem*))
    );
    QObject::connect(
        ui->filterBox, SIGNAL(stateChanged(int)),
        this, SLOT(on_filterBox_stateChanged(int))
    );
    QObject::connect(
        ui->settingsButton, SIGNAL(clicked(bool)),
        this, SLOT(on_settingsButton_clicked(bool))
    );
    QObject::connect(
        qAsConst(ui->settingsButton)->actions()[0], SIGNAL(changed()),
        this, SLOT(on_settings_extendedInformation_changed())
    );

    connectionElementsChanged = sketchView->signalElementsChanged.connect(
        boost::bind(&SketcherGui::TaskSketcherElements::slotElementsChanged, this));

    this->groupLayout()->addWidget(proxy);


    slotElementsChanged();

    // make filter items checkable
    ui->listMultiFilter->blockSignals(true);
    for (int i = 0; i < ui->listMultiFilter->count(); i++) {
        QListWidgetItem* item = ui->listMultiFilter->item(i);

        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

        item->setCheckState(Qt::Checked);
    }
    ui->listMultiFilter->setVisible(false);
    ui->listMultiFilter->blockSignals(false);

    this->installEventFilter(this);
    ui->filterBox->installEventFilter(this);
    ui->listMultiFilter->installEventFilter(this);
}

TaskSketcherElements::~TaskSketcherElements()
{
    connectionElementsChanged.disconnect();
}

/* filter functions --------------------------------------------------- */

void TaskSketcherElements::on_filterBox_stateChanged(int val){ 
    if(ui->filterBox->checkState() == Qt::Checked)
        ui->listMultiFilter->show();
    else
        ui->listMultiFilter->hide();

    slotElementsChanged();
}

bool TaskSketcherElements::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == (QObject*)ui->filterBox && event->type() == QEvent::Enter && ui->filterBox->checkState() == Qt::Checked) {
        ui->listMultiFilter->show();
    }
    else if (obj == (QObject*)ui->listMultiFilter && event->type() == QEvent::Leave) {
        ui->listMultiFilter->hide();
    }
    else if (obj == this && event->type() == QEvent::Leave) {
        ui->listMultiFilter->hide();
    }
    return QWidget::eventFilter(obj, event);
}

void TaskSketcherElements::on_listMultiFilter_itemChanged(QListWidgetItem* item)
{
    ui->listMultiFilter->blockSignals(true);

    if (item == ui->listMultiFilter->item(3)) { //3 is 'All geos'
        for (int i = 4; i < ui->listMultiFilter->count(); i++) {
            ui->listMultiFilter->item(i)->setCheckState(item->checkState());
        }
    }

    ui->listMultiFilter->blockSignals(false);

    updateVisibility();
}

void TaskSketcherElements::setItemVisibility(QListWidgetItem* item)
{
    /* index
    0 => Normal
    1 => Construction
    2 => External
    3 => all geos
    4 => Point
    5 => Line
    6 => Circle
    7 => Ellipse
    8 => Arc
    9 => Arc of Ellipse
    10 => Hyperbola
    11 => Parabola
    12 => bspline
    */

    ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
    bool visibility = true;

    if (ui->filterBox->checkState() == Qt::Unchecked) { item->setHidden(false); return; }

    if (ui->listMultiFilter->item(0)->checkState() == Qt::Unchecked && !itemData->isConstruction) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(1)->checkState() == Qt::Unchecked && itemData->isConstruction) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(2)->checkState() == Qt::Unchecked && itemData->isExternal) { item->setHidden(true); return; }

    if (ui->listMultiFilter->item(4)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomPoint::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(5)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomLineSegment::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(6)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomCircle::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(7)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomEllipse::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(8)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomArcOfCircle::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(9)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomArcOfEllipse::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(10)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomArcOfHyperbola::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(11)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomArcOfParabola::getClassTypeId()) { item->setHidden(true); return; }
    if (ui->listMultiFilter->item(12)->checkState() == Qt::Unchecked && itemData->GeometryType == Part::GeomBSplineCurve::getClassTypeId()) { item->setHidden(true); return; }

    item->setHidden(false);
    return;
}

void TaskSketcherElements::updateVisibility()
{
    for (int i = 0; i < ui->listWidgetElements->count(); i++) {
        setItemVisibility(ui->listWidgetElements->item(i));
    }
} 

/*------------------*/
void TaskSketcherElements::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    std::string temp;
    if (msg.Type == Gui::SelectionChanges::ClrSelection) {
        clearWidget();
    }
    else if (msg.Type == Gui::SelectionChanges::AddSelection ||
             msg.Type == Gui::SelectionChanges::RmvSelection) {
        bool select = (msg.Type == Gui::SelectionChanges::AddSelection);
        // is it this object??
        if (strcmp(msg.pDocName,sketchView->getSketchObject()->getDocument()->getName())==0 &&
            strcmp(msg.pObjectName,sketchView->getSketchObject()->getNameInDocument())== 0) {
            if (msg.pSubName) {
                QString expr = QString::fromLatin1(msg.pSubName);
                std::string shapetype(msg.pSubName);
                // if-else edge vertex
                if (shapetype.size() > 4 && shapetype.substr(0,4) == "Edge") {
                    QRegExp rx(QString::fromLatin1("^Edge(\\d+)$"));
                    int pos = expr.indexOf(rx);
                    if (pos > -1) {
                        bool ok;
                        int ElementId = rx.cap(1).toInt(&ok) - 1;
                        if (ok) {
                            int countItems = ui->listWidgetElements->count();
                            for (int i=0; i < countItems; i++) {
                                QListWidgetItem* item = ui->listWidgetElements->item(i);
                                ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
                                if (itemData->ElementNbr == ElementId) {
                                    itemData->isLineSelected = select;
                                    break;
                                }
                            }
                        }
                    }
                }
                else if (shapetype.size() > 6 && shapetype.substr(0,6) == "Vertex"){
                    QRegExp rx(QString::fromLatin1("^Vertex(\\d+)$"));
                    int pos = expr.indexOf(rx);
                    if (pos > -1) {
                        bool ok;
                        int ElementId = rx.cap(1).toInt(&ok) - 1;
                        if (ok) {
                            // Get the GeoID&Pos
                            int GeoId;
                            Sketcher::PointPos PosId;
                            sketchView->getSketchObject()->getGeoVertexIndex(ElementId,GeoId, PosId);

                            int countItems = ui->listWidgetElements->count();
                            for (int i=0; i < countItems; i++) {
                                QListWidgetItem* item = ui->listWidgetElements->item(i);
                                ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
                                if (itemData->ElementNbr == GeoId) {
                                    switch(PosId)
                                    {
                                    case Sketcher::PointPos::start:
                                        itemData->isStartingPointSelected=select;
                                        break;
                                    case Sketcher::PointPos::end:
                                        itemData->isEndPointSelected=select;
                                        break;
                                    case Sketcher::PointPos::mid:
                                        itemData->isMidPointSelected=select;
                                        break;
                                    default:
                                        break;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                // update the listwidget
                ui->listWidgetElements->blockSignals(true);

                for (int i=0;i<ui->listWidgetElements->count(); i++) {
                    QListWidgetItem* item = ui->listWidgetElements->item(i);
                    ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
                    item->setSelected(itemData->isLineSelected || itemData->isStartingPointSelected || itemData->isEndPointSelected || itemData->isMidPointSelected);
                }

                ui->listWidgetElements->blockSignals(false);

            }
        }
    }
    else if (msg.Type == Gui::SelectionChanges::SetSelection) {
        // do nothing here
    }
}

void TaskSketcherElements::on_listWidgetElements_itemPressed(QListWidgetItem* itf) {
    //We use itemPressed instead of previously used ItemSelectionChanged because if user click on already selected item, ItemSelectionChanged didn't trigger.
    if (itf == nullptr) { return; }

    ElementData* itfData = qvariant_cast<ElementData*>(itf->data(Qt::UserRole));
    bool rightClickOnSelected = itfData->rightClicked && (itfData->isLineSelected || itfData->isStartingPointSelected || itfData->isEndPointSelected || itfData->isMidPointSelected);
    itfData->rightClicked = false;
    if (rightClickOnSelected) { return; } //if user right clicked on a selected item, change nothing.

    ui->listWidgetElements->blockSignals(true);

    bool multipleselection = false;
    bool multipleconsecutiveselection = false;
    if (QApplication::keyboardModifiers() == Qt::ControlModifier)
        multipleselection = true;
    if (QApplication::keyboardModifiers() == Qt::ShiftModifier)
        multipleconsecutiveselection = true;

    if (multipleselection && multipleconsecutiveselection) { // ctrl takes priority over shift functionality
        multipleselection = true;
        multipleconsecutiveselection = false;
    }

    std::vector<std::string> elementSubNames;
    std::string doc_name = sketchView->getSketchObject()->getDocument()->getName();
    std::string obj_name = sketchView->getSketchObject()->getNameInDocument();

    bool block = this->blockSelection(true); // avoid to be notified by itself
    Gui::Selection().clearSelection();

    for (int i = 0; i < ui->listWidgetElements->count(); i++) {
        QListWidgetItem* item = ui->listWidgetElements->item(i);
        ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));

        if (!multipleselection && !multipleconsecutiveselection ) {
            //if not multiple selection, then all are disabled but the one that was just selected
            itemData->isLineSelected = false;
            itemData->isStartingPointSelected = false;
            itemData->isEndPointSelected = false;
            itemData->isMidPointSelected = false;
        }

        if (item == itf) {

            if (itemData->GeometryType == Part::GeomPoint::getClassTypeId()) {
                itemData->isStartingPointSelected = !itemData->isStartingPointSelected;
            }
            else if (itemData->clickedOn == ElementData::ClickedOn::mid
                && (itemData->GeometryType == Part::GeomArcOfCircle::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfEllipse::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfHyperbola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfParabola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomCircle::getClassTypeId()
                    || itemData->GeometryType == Part::GeomEllipse::getClassTypeId())) {
                itemData->isMidPointSelected = !itemData->isMidPointSelected;
            }
            else if (itemData->clickedOn == ElementData::ClickedOn::start &&
                (itemData->GeometryType == Part::GeomArcOfCircle::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfEllipse::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfHyperbola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfParabola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomLineSegment::getClassTypeId()
                    || itemData->GeometryType == Part::GeomBSplineCurve::getClassTypeId())) {
                itemData->isStartingPointSelected = !itemData->isStartingPointSelected;
            }
            else if (itemData->clickedOn == ElementData::ClickedOn::end &&
                (itemData->GeometryType == Part::GeomArcOfCircle::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfEllipse::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfHyperbola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomArcOfParabola::getClassTypeId()
                    || itemData->GeometryType == Part::GeomLineSegment::getClassTypeId()
                    || itemData->GeometryType == Part::GeomBSplineCurve::getClassTypeId())) {
                itemData->isEndPointSelected = !itemData->isEndPointSelected;
            }
            else {
                itemData->isLineSelected = !itemData->isLineSelected;
            }
            itemData->clickedOn == ElementData::ClickedOn::none;
        }
        else if (multipleconsecutiveselection && previouslySelectedItemIndex >= 0 && !rightClickOnSelected &&
            ((i > focusItemIndex && i < previouslySelectedItemIndex) || (i<focusItemIndex && i>previouslySelectedItemIndex))) {
            if (itemData->GeometryType == Part::GeomPoint::getClassTypeId()) {
                itemData->isStartingPointSelected = true;
            }
            else {
                itemData->isLineSelected = true;
            }
        }

        // first update the listwidget. Item is selected if at least one element of the geo is selected.
        item->setSelected(itemData->isLineSelected || itemData->isStartingPointSelected || itemData->isEndPointSelected || itemData->isMidPointSelected);

        // now the scene
        std::stringstream ss;
        int vertex;

        if (itemData->isLineSelected) {
            ss << "Edge" << itemData->ElementNbr + 1;
            elementSubNames.push_back(ss.str());
        }

        if (itemData->isStartingPointSelected) {
            ss.str(std::string());
            vertex = itemData->StartingVertex;
            if (vertex != -1) {
                ss << "Vertex" << vertex + 1;
                elementSubNames.push_back(ss.str());
            }
        }

        if (itemData->isEndPointSelected) {
            ss.str(std::string());
            vertex = itemData->EndVertex;
            if (vertex != -1) {
                ss << "Vertex" << vertex + 1;
                elementSubNames.push_back(ss.str());
            }
        }

        if (itemData->isMidPointSelected) {
            ss.str(std::string());
            vertex = itemData->MidVertex;
            if (vertex != -1) {
                ss << "Vertex" << vertex + 1;
                elementSubNames.push_back(ss.str());
            }
        }
    }

    if (!elementSubNames.empty()) {
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), elementSubNames);
    }

    this->blockSelection(block);
    ui->listWidgetElements->blockSignals(false);

    if (focusItemIndex > -1 && focusItemIndex < ui->listWidgetElements->count())
        previouslySelectedItemIndex = focusItemIndex;

    ui->listWidgetElements->repaint();
}

void TaskSketcherElements::on_listWidgetElements_itemEntered(QListWidgetItem *item)
{
    ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));
    if (!item)
        return;

    Gui::Selection().rmvPreselect();

    ui->listWidgetElements->setFocus();

    std::string doc_name = sketchView->getSketchObject()->getDocument()->getName();
    std::string obj_name = sketchView->getSketchObject()->getNameInDocument();

    /* 0 - Lines
     * 1 - Starting Points
     * 2 - End Points
     * 3 - Middle Points
     */
    std::stringstream ss;


    /*
    int tempitemindex=ui->listWidgetElements->row(item);
    //Edge Auto-Switch functionality
    if (isautoSwitchBoxChecked && tempitemindex!=focusItemIndex){
        ui->listWidgetElements->blockSignals(true);
        if (it->GeometryType==Part::GeomPoint::getClassTypeId()) {
            ui->comboBoxElementFilter->setCurrentIndex(1);
        }
        else {
            ui->comboBoxElementFilter->setCurrentIndex(0);
        }
        ui->listWidgetElements->blockSignals(false);
    }

    int element=ui->comboBoxElementFilter->currentIndex();
    
    focusItemIndex=tempitemindex;*/
    focusItemIndex = ui->listWidgetElements->row(item);

    if (itemData->isStartingPointSelected) {
        int vertex = sketchView->getSketchObject()->getVertexIndexGeoPos(itemData->ElementNbr, Sketcher::PointPos::start);
        if (vertex != -1) {
            ss << "Vertex" << vertex + 1;
            Gui::Selection().setPreselect(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
        }
    }
    else if (itemData->isEndPointSelected) {
        int vertex = sketchView->getSketchObject()->getVertexIndexGeoPos(itemData->ElementNbr, Sketcher::PointPos::end);
        if (vertex != -1) {
            ss << "Vertex" << vertex + 1;
            Gui::Selection().setPreselect(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
        }
    }
    else if (itemData->isMidPointSelected) {
        int vertex = sketchView->getSketchObject()->getVertexIndexGeoPos(itemData->ElementNbr, Sketcher::PointPos::mid);
        if (vertex != -1) {
            ss << "Vertex" << vertex + 1;
            Gui::Selection().setPreselect(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
        }
    }
    else {
        ss << "Edge" << itemData->ElementNbr + 1;
        Gui::Selection().setPreselect(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
    }

}

void TaskSketcherElements::leaveEvent (QEvent * event)
{
    Q_UNUSED(event);
    Gui::Selection().rmvPreselect();
    ui->listWidgetElements->clearFocus();
}

void TaskSketcherElements::slotElementsChanged(void)
{
    assert(sketchView);
    // Build up ListView with the elements
    Sketcher::SketchObject *sketch = sketchView->getSketchObject();
    const std::vector< Part::Geometry * > &vals = sketch->Geometry.getValues();

    ui->listWidgetElements->clear();

    int i=1;
    for(std::vector< Part::Geometry * >::const_iterator it= vals.begin();it!=vals.end();++it,++i){
        Base::Type type = (*it)->getTypeId();
        bool construction = Sketcher::GeometryFacade::getConstruction(*it);


        QListWidgetItem* itemN = new QListWidgetItem;

        ElementData* elementData = new ElementData(i - 1,
            sketchView->getSketchObject()->getVertexIndexGeoPos(i - 1, Sketcher::PointPos::start),
            sketchView->getSketchObject()->getVertexIndexGeoPos(i - 1, Sketcher::PointPos::mid),
            sketchView->getSketchObject()->getVertexIndexGeoPos(i - 1, Sketcher::PointPos::end),
            type, construction, false,

            type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_Edge.getIcon(construction, false) :
            type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_Edge.getIcon(construction, false) :
            type == Part::GeomCircle::getClassTypeId() ? Sketcher_Element_Circle_Edge.getIcon(construction, false) :
            type == Part::GeomEllipse::getClassTypeId() ? Sketcher_Element_Ellipse_Edge.getIcon(construction, false) :
            type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_Edge.getIcon(construction, false) :
            type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_Edge.getIcon(construction, false) :
            type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_Edge.getIcon(construction, false) :
            type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_Edge.getIcon(construction, false) :
            none.getIcon(construction, false),


            type == Part::GeomPoint::getClassTypeId() ? Sketcher_Element_Point_StartingPoint.getIcon(construction, false) :
            type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_StartingPoint.getIcon(construction, false) :
            type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_StartingPoint.getIcon(construction, false) :
            type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_StartingPoint.getIcon(construction, false) :
            type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_StartingPoint.getIcon(construction, false) :
            type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_StartingPoint.getIcon(construction, false) :
            type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_StartingPoint.getIcon(construction, false) :
            none.getIcon(construction, false),


            type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_EndPoint.getIcon(construction, false) :
            type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_EndPoint.getIcon(construction, false) :
            type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_EndPoint.getIcon(construction, false) :
            type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_EndPoint.getIcon(construction, false) :
            type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_EndPoint.getIcon(construction, false) :
            type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_EndPoint.getIcon(construction, false) :
            none.getIcon(construction, false),


            type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_MidPoint.getIcon(construction, false) :
            type == Part::GeomCircle::getClassTypeId() ? Sketcher_Element_Circle_MidPoint.getIcon(construction, false) :
            type == Part::GeomEllipse::getClassTypeId() ? Sketcher_Element_Ellipse_MidPoint.getIcon(construction, false) :
            type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_MidPoint.getIcon(construction, false) :
            type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_MidPoint.getIcon(construction, false) :
            type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_MidPoint.getIcon(construction, false) :
            none.getIcon(construction, false),


            type == Part::GeomPoint::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Point") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Point"))) :
            type == Part::GeomLineSegment::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Line") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Line"))) :
            type == Part::GeomArcOfCircle::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Arc") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Arc"))) :
            type == Part::GeomCircle::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Circle") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Circle"))) :
            type == Part::GeomEllipse::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Ellipse") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Ellipse"))) :
            type == Part::GeomArcOfEllipse::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Elliptical Arc") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Elliptical Arc"))) :
            type == Part::GeomArcOfHyperbola::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Hyperbolic Arc") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Hyperbolic Arc"))) :
            type == Part::GeomArcOfParabola::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("Parabolic Arc") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Parabolic Arc"))) :
            type == Part::GeomBSplineCurve::getClassTypeId() ? (isNamingBoxChecked ?
                (tr("BSpline") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("BSpline"))) :
            (isNamingBoxChecked ?
                (tr("Other") + QString::fromLatin1("(Edge%1#ID%2)").arg(i).arg(i - 1)) +
                (construction ? (QString::fromLatin1("-") + tr("Construction")) : QString::fromLatin1("")) :
                (QString::fromLatin1("%1-").arg(i) + tr("Other"))) 
        );

        QVariant dataInVariant;
        dataInVariant.setValue(elementData);
        itemN->setData(Qt::UserRole, dataInVariant);

        ui->listWidgetElements->addItem(itemN);

        setItemVisibility(itemN);
    }

    const std::vector< Part::Geometry * > &ext_vals = sketchView->getSketchObject()->getExternalGeometry();

    const std::vector<App::DocumentObject*> linkobjs = sketchView->getSketchObject()->ExternalGeometry.getValues();
    const std::vector<std::string> linksubs = sketchView->getSketchObject()->ExternalGeometry.getSubValues();

    int j=1;
    for(std::vector< Part::Geometry * >::const_iterator it= ext_vals.begin();it!=ext_vals.end();++it,++i,++j){
      Base::Type type = (*it)->getTypeId();

        if(j>2) { // we do not want the H and V axes

            QString linkname;

            if(isNamingBoxChecked) {
                if(size_t(j-3) < linkobjs.size() && size_t(j-3) < linksubs.size()) {
                    linkname =  QString::fromLatin1("(ExternalEdge%1#ID%2, ").arg(j-2).arg(-j) +
                                QString::fromUtf8(linkobjs[j-3]->getNameInDocument()) +
                                QString::fromLatin1(".") +
                                QString::fromUtf8(linksubs[j-3].c_str()) +
                                QString::fromLatin1(")");
                }
                else {
                    linkname = QString::fromLatin1("(ExternalEdge%1)").arg(j-2);
                }
            }

            QListWidgetItem* itemN = new QListWidgetItem;

            ElementData* elementData = new ElementData( -j,
                sketchView->getSketchObject()->getVertexIndexGeoPos(-j, Sketcher::PointPos::start),
                sketchView->getSketchObject()->getVertexIndexGeoPos(-j, Sketcher::PointPos::mid),
                sketchView->getSketchObject()->getVertexIndexGeoPos(-j, Sketcher::PointPos::end),
                type, false, true,

                type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_Edge.External :
                type == Part::GeomCircle::getClassTypeId() ? Sketcher_Element_Circle_Edge.External :
                type == Part::GeomEllipse::getClassTypeId() ? Sketcher_Element_Ellipse_Edge.External :
                type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_Edge.External :
                type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_Edge.External :
                type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_Edge.External :
                type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_Edge.External :
                none.External,

                type == Part::GeomPoint::getClassTypeId() ? Sketcher_Element_Point_StartingPoint.External :
                type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_StartingPoint.External :
                type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_StartingPoint.External :
                type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_StartingPoint.External :
                type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_StartingPoint.External :
                type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_StartingPoint.External :
                type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_StartingPoint.External :
                none.External,

                type == Part::GeomLineSegment::getClassTypeId() ? Sketcher_Element_Line_EndPoint.External :
                type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_EndPoint.External :
                type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_EndPoint.External :
                type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_EndPoint.External :
                type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_EndPoint.External :
                type == Part::GeomBSplineCurve::getClassTypeId() ? Sketcher_Element_BSpline_EndPoint.External :
                none.External,


                type == Part::GeomArcOfCircle::getClassTypeId() ? Sketcher_Element_Arc_MidPoint.External :
                type == Part::GeomCircle::getClassTypeId() ? Sketcher_Element_Circle_MidPoint.External :
                type == Part::GeomEllipse::getClassTypeId() ? Sketcher_Element_Ellipse_MidPoint.External :
                type == Part::GeomArcOfEllipse::getClassTypeId() ? Sketcher_Element_ArcOfEllipse_MidPoint.External :
                type == Part::GeomArcOfHyperbola::getClassTypeId() ? Sketcher_Element_ArcOfHyperbola_MidPoint.External :
                type == Part::GeomArcOfParabola::getClassTypeId() ? Sketcher_Element_ArcOfParabola_MidPoint.External :
                none.External,


                type == Part::GeomPoint::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Point") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Point"))) :
                type == Part::GeomLineSegment::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Line") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Line"))) :
                type == Part::GeomArcOfCircle::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Arc") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Arc"))) :
                type == Part::GeomCircle::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Circle") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Circle"))) :
                type == Part::GeomEllipse::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Ellipse") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Ellipse"))) :
                type == Part::GeomArcOfEllipse::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Elliptical Arc") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Elliptical Arc"))) :
                type == Part::GeomArcOfHyperbola::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Hyperbolic Arc") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Hyperbolic Arc"))) :
                type == Part::GeomArcOfParabola::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("Parabolic Arc") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Parabolic Arc"))) :
                type == Part::GeomBSplineCurve::getClassTypeId() ? (isNamingBoxChecked ?
                    (tr("BSpline") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("BSpline"))) :
                (isNamingBoxChecked ?
                    (tr("Other") + linkname) :
                    (QString::fromLatin1("%1-").arg(i - 2) + tr("Other")))
            );

            QVariant dataInVariant;
            dataInVariant.setValue(elementData);
            itemN->setData(Qt::UserRole, dataInVariant);

            ui->listWidgetElements->addItem(itemN);

            setItemVisibility(itemN);

        }
    }
}

void TaskSketcherElements::on_listWidgetElements_filterShortcutPressed()
{
    if (!(focusItemIndex > -1 && focusItemIndex < ui->listWidgetElements->count()))
        return;

    QListWidgetItem* itf = ui->listWidgetElements->item(focusItemIndex);
    ElementData* itfData = qvariant_cast<ElementData*>(itf->data(Qt::UserRole));

    //We switch to next type only if only one element of the geo is selected.
    if (!(itfData->isLineSelected != itfData->isStartingPointSelected != itfData->isEndPointSelected != itfData->isMidPointSelected)) //note != act as XOr
        return;

    previouslySelectedItemIndex = -1; // Shift selection on list widget implementation

    Base::Type type = itfData->GeometryType;

    if (itfData->isLineSelected) {
        itfData->clickedOn = static_cast<int>((type == Part::GeomCircle::getClassTypeId() || type == Part::GeomEllipse::getClassTypeId()) ? ElementData::ClickedOn::mid : ElementData::ClickedOn::start);
    }
    else if (itfData->isStartingPointSelected) {
        itfData->clickedOn = static_cast<int>((type == Part::GeomCircle::getClassTypeId() || type == Part::GeomEllipse::getClassTypeId()) ? ElementData::ClickedOn::mid :
            (type == Part::GeomPoint::getClassTypeId()) ? ElementData::ClickedOn::start : ElementData::ClickedOn::end);
    }
    else if (itfData->isEndPointSelected) {
        itfData->clickedOn = static_cast<int>(type == Part::GeomLineSegment::getClassTypeId() ? ElementData::ClickedOn::edge :
            type == Part::GeomPoint::getClassTypeId() ? ElementData::ClickedOn::start : ElementData::ClickedOn::mid);
    }
    else {
        itfData->clickedOn = static_cast<int>(type == Part::GeomPoint::getClassTypeId() ? ElementData::ClickedOn::start : ElementData::ClickedOn::edge);
    }

    Gui::Selection().rmvPreselect();

    on_listWidgetElements_itemEntered(itf);
    on_listWidgetElements_itemPressed(itf);
}

void TaskSketcherElements::clearWidget()
{
    ui->listWidgetElements->blockSignals(true);
    ui->listWidgetElements->clearSelection ();
    ui->listWidgetElements->blockSignals(false);

    // update widget
    int countItems = ui->listWidgetElements->count();
    for (int i=0; i < countItems; i++) {
      QListWidgetItem* item = ui->listWidgetElements->item(i);
      ElementData* itemData = qvariant_cast<ElementData*>(item->data(Qt::UserRole));

      itemData->isLineSelected=false;
      itemData->isStartingPointSelected=false;
      itemData->isEndPointSelected=false;
      itemData->isMidPointSelected=false;
    }
}

void TaskSketcherElements::changeEvent(QEvent *e)
{
    TaskBox::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(proxy);
    }
}

TaskSketcherElements::MultIcon::MultIcon(const char* name)
{
    int hue, sat, val, alp;
    Normal = Gui::BitmapFactory().iconFromTheme(name);
    QImage imgConstr(Normal.pixmap(Normal.availableSizes()[0]).toImage());
    QImage imgExt(imgConstr);

    for(int ix=0 ; ix<imgConstr.width() ; ix++) {
        for(int iy=0 ; iy<imgConstr.height() ; iy++) {
            QColor clr = QColor::fromRgba(imgConstr.pixel(ix,iy));
            clr.getHsv(&hue, &sat, &val, &alp);
            if (alp > 127 && hue >= 0) {
                if (sat > 127 && (hue > 330 || hue < 30)) {
                    clr.setHsv((hue + 240) % 360, sat, val, alp);
                    imgConstr.setPixel(ix, iy, clr.rgba());
                    clr.setHsv((hue + 300) % 360, sat, val, alp);
                    imgExt.setPixel(ix, iy, clr.rgba());
                }
                else if (sat < 64 && val > 192)
                {
                    clr.setHsv(240, (255-sat), val, alp);
                    imgConstr.setPixel(ix, iy, clr.rgba());
                    clr.setHsv(300, (255-sat), val, alp);
                    imgExt.setPixel(ix, iy, clr.rgba());
                }
            }
        }
    }
    Construction = QIcon(QPixmap::fromImage(imgConstr));
    External = QIcon(QPixmap::fromImage(imgExt));

}

QIcon TaskSketcherElements::MultIcon::getIcon(bool construction, bool external) const
{
    if (construction && external)
        return QIcon();
    if (construction)
        return Construction;
    if (external)
        return External;
    return Normal;
}


/* Settings menu ==================================================*/
void TaskSketcherElements::createSettingsButtonActions()
{
    QAction* action = new QAction(QString::fromLatin1("Extended information"), this);

    action->setCheckable(true);

    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/Elements");
    {
        QSignalBlocker block(this);
        action->setChecked(hGrp->GetBool("Extended Naming", false));
    }

    ui->settingsButton->addAction(action);
}

void TaskSketcherElements::on_settings_extendedInformation_changed()
{
    ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/Elements");
    hGrp->SetBool("Extended Naming", ui->settingsButton->actions()[0]->isChecked());

    isNamingBoxChecked = ui->settingsButton->actions()[0]->isChecked();
    slotElementsChanged();
}

void TaskSketcherElements::on_settingsButton_clicked(bool)
{
    ui->settingsButton->showMenu();
}

#include "moc_TaskSketcherElements.cpp"