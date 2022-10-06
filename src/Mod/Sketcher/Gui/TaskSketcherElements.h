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


#ifndef GUI_TASKVIEW_TaskSketcherElements_H
#define GUI_TASKVIEW_TaskSketcherElements_H

#include <Gui/TaskView/TaskView.h>
#include <Gui/Selection.h>
#include <boost_signals2.hpp>
#include <QListWidget>
#include <QIcon>

namespace App {
class Property;
}

namespace SketcherGui {

class ViewProviderSketch;
class Ui_TaskSketcherElements;

class MyDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private Q_SLOTS:
    //void commitAndCloseEditor();
};

class ElementData
{
public:
    ElementData() = default;
    ~ElementData() = default;
    ElementData(const ElementData&) = default;

    ElementData(int elementnr, int startingVertex, int midVertex, int endVertex,
        Base::Type geometryType, bool construction, bool external, QIcon ic0,QIcon ic1,QIcon ic2,QIcon ic3, QString lab ) : 
         ElementNbr(elementnr)
        , StartingVertex(startingVertex)
        , MidVertex(midVertex)
        , EndVertex(endVertex)
        , isLineSelected(false)
        , isStartingPointSelected(false)
        , isEndPointSelected(false)
        , isMidPointSelected(false)
        , GeometryType(geometryType)
        , isConstruction(construction)
        , isExternal(external)
        , icon0(ic0)
        , icon1(ic1)
        , icon2(ic2)
        , icon3(ic3)
        , label(lab)
        , rightClicked(false)
    {}

    int ElementNbr;
    int StartingVertex;
    int MidVertex;
    int EndVertex;
    bool isLineSelected;
    bool isStartingPointSelected;
    bool isEndPointSelected;
    bool isMidPointSelected;
    Base::Type GeometryType;
    bool isConstruction;
    bool isExternal;

    enum ClickedOn{edge, start, end, mid, none};
    int clickedOn;
    bool rightClicked;

    QIcon icon0;
    QIcon icon1;
    QIcon icon2;
    QIcon icon3;
    QString label;
};

Q_DECLARE_METATYPE(ElementData*);

class ElementView : public QListWidget
{
    Q_OBJECT

public:
    explicit ElementView(QWidget *parent = nullptr);
    ~ElementView();


Q_SIGNALS:
    void onFilterShortcutPressed();

protected:
    void contextMenuEvent (QContextMenuEvent* event);
    void keyPressEvent(QKeyEvent * event); 
    void mousePressEvent(QMouseEvent* event);


protected Q_SLOTS:
    // Constraints
    void doPointCoincidence();
    void doPointOnObjectConstraint();
    void doVerticalDistance();
    void doHorizontalDistance();
    void doParallelConstraint();
    void doPerpendicularConstraint();
    void doTangentConstraint();
    void doEqualConstraint();
    void doSymmetricConstraint();
    void doBlockConstraint();

    void doLockConstraint();
    void doHorizontalConstraint();
    void doVerticalConstraint();
    void doLengthConstraint();
    void doRadiusConstraint();
    void doDiameterConstraint();
    void doRadiamConstraint();
    void doAngleConstraint();

    // Other Commands
    void doToggleConstruction();

    // Acelerators
    void doSelectConstraints();
    void doSelectOrigin();
    void doSelectHAxis();
    void doSelectVAxis();
    void deleteSelectedItems();
};

class TaskSketcherElements : public Gui::TaskView::TaskBox, public Gui::SelectionObserver
{
    Q_OBJECT

    class MultIcon {

    public:
        MultIcon() {};
        MultIcon(const char*);

        QIcon Normal;
        QIcon Construction;
        QIcon External;

        QIcon getIcon(bool construction, bool external) const;
    };

public:
    TaskSketcherElements(ViewProviderSketch *sketchView);
    ~TaskSketcherElements();

    /// Observer message from the Selection
    void onSelectionChanged(const Gui::SelectionChanges& msg);

    bool eventFilter(QObject* obj, QEvent* event);

private:
    void slotElementsChanged(void);
    void updateVisibility();
    void setItemVisibility(QListWidgetItem* item);
    void clearWidget();
    void createSettingsButtonActions();

public Q_SLOTS:
    void on_listWidgetElements_itemPressed(QListWidgetItem* item);
    void on_listWidgetElements_itemEntered(QListWidgetItem *item);
    void on_listWidgetElements_filterShortcutPressed();
    void on_settings_extendedInformation_changed();
    void on_settingsButton_clicked(bool);
    void on_filterBox_stateChanged(int val);
    void on_listMultiFilter_itemChanged(QListWidgetItem* item);

protected:
    void changeEvent(QEvent *e);
    void leaveEvent ( QEvent * event );
    ViewProviderSketch *sketchView;
    typedef boost::signals2::connection Connection;
    Connection connectionElementsChanged;

private:
    QWidget* proxy;
    std::unique_ptr<Ui_TaskSketcherElements> ui;
    int focusItemIndex;
    int previouslySelectedItemIndex;

    bool isNamingBoxChecked;

    MultIcon Sketcher_Element_Arc_Edge;
    MultIcon Sketcher_Element_Arc_EndPoint;
    MultIcon Sketcher_Element_Arc_MidPoint;
    MultIcon Sketcher_Element_Arc_StartingPoint;
    MultIcon Sketcher_Element_Circle_Edge;
    MultIcon Sketcher_Element_Circle_MidPoint;
    MultIcon Sketcher_Element_Line_Edge;
    MultIcon Sketcher_Element_Line_EndPoint;
    MultIcon Sketcher_Element_Line_StartingPoint;
    MultIcon Sketcher_Element_Point_StartingPoint;
    MultIcon Sketcher_Element_Ellipse_Edge;
    MultIcon Sketcher_Element_Ellipse_MidPoint;
    MultIcon Sketcher_Element_ArcOfEllipse_Edge;
    MultIcon Sketcher_Element_ArcOfEllipse_MidPoint;
    MultIcon Sketcher_Element_ArcOfEllipse_StartingPoint;
    MultIcon Sketcher_Element_ArcOfEllipse_EndPoint;
    MultIcon Sketcher_Element_ArcOfHyperbola_Edge;
    MultIcon Sketcher_Element_ArcOfHyperbola_MidPoint;
    MultIcon Sketcher_Element_ArcOfHyperbola_StartingPoint;
    MultIcon Sketcher_Element_ArcOfHyperbola_EndPoint;
    MultIcon Sketcher_Element_ArcOfParabola_Edge;
    MultIcon Sketcher_Element_ArcOfParabola_MidPoint;
    MultIcon Sketcher_Element_ArcOfParabola_StartingPoint;
    MultIcon Sketcher_Element_ArcOfParabola_EndPoint;
    MultIcon Sketcher_Element_BSpline_Edge;
    MultIcon Sketcher_Element_BSpline_StartingPoint;
    MultIcon Sketcher_Element_BSpline_EndPoint;
    MultIcon none;
};

} //namespace SketcherGui

#endif // GUI_TASKVIEW_TASKAPPERANCE_H
