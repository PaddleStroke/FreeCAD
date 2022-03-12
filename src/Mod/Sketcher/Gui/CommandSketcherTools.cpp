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
# include <cfloat>
# include <memory>
# include <QMessageBox>
# include <Precision.hxx>
# include <QApplication>
# include <QMessageBox>

# include <Inventor/SbString.h>
#endif

#include <Base/Console.h>
#include <Base/Tools.h>
#include <App/Application.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/CommandT.h>
#include <Gui/MainWindow.h>
#include <Gui/DlgEditFileIncludePropertyExternal.h>

#include <Gui/Action.h>
#include <Gui/BitmapFactory.h>

#include "ViewProviderSketch.h"
#include "DrawSketchHandler.h"

#include <Mod/Part/App/Geometry.h>
#include <Mod/Sketcher/App/SketchObject.h>
#include <Mod/Sketcher/App/SolverGeometryExtension.h>

#include "ViewProviderSketch.h"
#include "SketchRectangularArrayDialog.h"
#include "Utils.h"

#include <BRepAdaptor_Curve.hxx>
#if OCC_VERSION_HEX < 0x070600
#include <BRepAdaptor_HCurve.hxx>
#endif
#include <Mod/Part/App/BRepOffsetAPI_MakeOffsetFix.h>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI.hxx>


using namespace std;
using namespace SketcherGui;
using namespace Sketcher;

bool isSketcherAcceleratorActive(Gui::Document *doc, bool actsOnSelection)
{
    if (doc) {
        // checks if a Sketch Viewprovider is in Edit and is in no special mode
        if (doc->getInEdit() && doc->getInEdit()->isDerivedFrom(SketcherGui::ViewProviderSketch::getClassTypeId())) {
            auto mode = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit())
                ->getSketchMode();
            if (mode == ViewProviderSketch::STATUS_NONE ||
                mode == ViewProviderSketch::STATUS_SKETCH_UseHandler) {
                if (!actsOnSelection)
                    return true;
                else if (Gui::Selection().countObjectsOfType(Sketcher::SketchObject::getClassTypeId()) > 0)
                    return true;
            }
        }
    }

    return false;
}

void ActivateAcceleratorHandler(Gui::Document *doc, DrawSketchHandler *handler)
{
    std::unique_ptr<DrawSketchHandler> ptr(handler);
    if (doc) {
        if (doc->getInEdit() && doc->getInEdit()->isDerivedFrom(SketcherGui::ViewProviderSketch::getClassTypeId())) {
            SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*> (doc->getInEdit());
            vp->purgeHandler();
            vp->activateHandler(ptr.release());
        }
    }
}

// ================================================================================

// Close Shape Command
DEF_STD_CMD_A(CmdSketcherCloseShape)

CmdSketcherCloseShape::CmdSketcherCloseShape()
    :Command("Sketcher_CloseShape")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Close shape");
    sToolTipText    = QT_TR_NOOP("Produce a closed shape by tying the end point "
                                 "of one element with the next element's starting point");
    sWhatsThis      = "Sketcher_CloseShape";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_CloseShape";
    sAccel          = "Z, W";
    eType           = ForEdit;
}

void CmdSketcherCloseShape::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    // Cancel any in-progress operation
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    SketcherGui::ReleaseHandler(doc);

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select at least two edges from the sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.size() < 2) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select at least two edges from the sketch."));
        return;
    }

    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

    int GeoIdFirst = -1;
    int GeoIdLast = -1;

    // undo command open
    openCommand(QT_TRANSLATE_NOOP("Command", "Add coincident constraint"));
    // go through the selected subelements
    for (size_t i=0; i < (SubNames.size() - 1); i++) {
        // only handle edges
        if (SubNames[i].size() > 4 && SubNames[i].substr(0,4) == "Edge" &&
            SubNames[i+1].size() > 4 && SubNames[i+1].substr(0,4) == "Edge") {

            int GeoId1 = std::atoi(SubNames[i].substr(4,4000).c_str()) - 1;
            int GeoId2 = std::atoi(SubNames[i+1].substr(4,4000).c_str()) - 1;

            if (GeoIdFirst == -1)
                GeoIdFirst = GeoId1;

            GeoIdLast = GeoId2;

            const Part::Geometry *geo1 = Obj->getGeometry(GeoId1);
            const Part::Geometry *geo2 = Obj->getGeometry(GeoId2);
            if ((geo1->getTypeId() != Part::GeomLineSegment::getClassTypeId() &&
                geo1->getTypeId() != Part::GeomArcOfCircle::getClassTypeId()) ||
                (geo2->getTypeId() != Part::GeomLineSegment::getClassTypeId() &&
                geo2->getTypeId() != Part::GeomArcOfCircle::getClassTypeId())) {
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Impossible constraint"),
                      QObject::tr("One selected edge is not connectable"));
                abortCommand();
                return;
            }

            // Check for the special case of closing a shape with two lines to avoid overlap
            if (SubNames.size() == 2 &&
                geo1->getTypeId() == Part::GeomLineSegment::getClassTypeId() &&
                geo2->getTypeId() == Part::GeomLineSegment::getClassTypeId() ) {
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
                    QObject::tr("Closing a shape formed by exactly two lines makes no sense."));
                abortCommand();
                return;
            }

            Gui::cmdAppObjectArgs(selection[0].getObject(),
                                  "addConstraint(Sketcher.Constraint('Coincident', %d, %d, %d, %d)) ",
                                  GeoId1, static_cast<int>(Sketcher::PointPos::end), GeoId2, static_cast<int>(Sketcher::PointPos::start));
        }
    }

    // Close Last Edge with First Edge
    Gui::cmdAppObjectArgs(selection[0].getObject(),
                          "addConstraint(Sketcher.Constraint('Coincident', %d, %d, %d, %d)) ",
                          GeoIdLast, static_cast<int>(Sketcher::PointPos::end), GeoIdFirst, static_cast<int>(Sketcher::PointPos::start));

    // finish the transaction and update, and clear the selection (convenience)
    commitCommand();
    tryAutoRecompute(Obj);
    getSelection().clearSelection();
}

bool CmdSketcherCloseShape::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

// Connect Edges Command
DEF_STD_CMD_A(CmdSketcherConnect)

CmdSketcherConnect::CmdSketcherConnect()
    :Command("Sketcher_ConnectLines")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Connect edges");
    sToolTipText    = QT_TR_NOOP("Tie the end point of the element with next element's starting point");
    sWhatsThis      = "Sketcher_ConnectLines";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_ConnectLines";
    sAccel          = "Z, J";
    eType           = ForEdit;
}

void CmdSketcherConnect::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    // Cancel any in-progress operation
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    SketcherGui::ReleaseHandler(doc);

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select at least two edges from the sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.size() < 2) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select at least two edges from the sketch."));
        return;
    }
    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

    // undo command open
    openCommand(QT_TRANSLATE_NOOP("Command", "Add coincident constraint"));

    // go through the selected subelements
    for (unsigned int i=0; i<(SubNames.size()-1); i++ ) {
        // only handle edges
        if (SubNames[i].size() > 4 && SubNames[i].substr(0,4) == "Edge" &&
            SubNames[i+1].size() > 4 && SubNames[i+1].substr(0,4) == "Edge") {

            int GeoId1 = std::atoi(SubNames[i].substr(4,4000).c_str()) - 1;
            int GeoId2 = std::atoi(SubNames[i+1].substr(4,4000).c_str()) - 1;

            const Part::Geometry *geo1 = Obj->getGeometry(GeoId1);
            const Part::Geometry *geo2 = Obj->getGeometry(GeoId2);
            if ((geo1->getTypeId() != Part::GeomLineSegment::getClassTypeId() &&
                geo1->getTypeId() != Part::GeomArcOfCircle::getClassTypeId()) ||
                (geo2->getTypeId() != Part::GeomLineSegment::getClassTypeId() &&
                geo2->getTypeId() != Part::GeomArcOfCircle::getClassTypeId())) {
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Impossible constraint"),
                      QObject::tr("One selected edge is not connectable"));
                abortCommand();
                return;
            }

            Gui::cmdAppObjectArgs(selection[0].getObject(),"addConstraint(Sketcher.Constraint('Coincident',%d,%d,%d,%d)) ",
                GeoId1,static_cast<int>(Sketcher::PointPos::end),GeoId2,static_cast<int>(Sketcher::PointPos::start));
        }
    }

    // finish the transaction and update, and clear the selection (convenience)
    commitCommand();
    tryAutoRecompute(Obj);
    getSelection().clearSelection();
}

bool CmdSketcherConnect::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

// Select Constraints of selected elements
DEF_STD_CMD_A(CmdSketcherSelectConstraints)

CmdSketcherSelectConstraints::CmdSketcherSelectConstraints()
    :Command("Sketcher_SelectConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select associated constraints");
    sToolTipText    = QT_TR_NOOP("Select the constraints associated with the selected geometrical elements");
    sWhatsThis      = "Sketcher_SelectConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectConstraints";
    sAccel          = "Z, K";
    eType           = ForEdit;
}

void CmdSketcherSelectConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // Cancel any in-progress operation
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    SketcherGui::ReleaseHandler(doc);

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();

    getSelection().clearSelection();

    std::vector<std::string> constraintSubNames;
    // go through the selected subelements
    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle edges
        if (it->size() > 4 && it->substr(0,4) == "Edge") {
            int GeoId = std::atoi(it->substr(4,4000).c_str()) - 1;

            // push all the constraints
            int i = 0;
            for (std::vector< Sketcher::Constraint * >::const_iterator it= vals.begin();
                 it != vals.end(); ++it,++i)
            {
                if ((*it)->First == GeoId || (*it)->Second == GeoId || (*it)->Third == GeoId) {
                    constraintSubNames.push_back(Sketcher::PropertyConstraintList::getConstraintName(i));
                }
            }
        }
    }

    if(!constraintSubNames.empty())
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), constraintSubNames);

}

bool CmdSketcherSelectConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

// Select Origin
DEF_STD_CMD_A(CmdSketcherSelectOrigin)

CmdSketcherSelectOrigin::CmdSketcherSelectOrigin()
    :Command("Sketcher_SelectOrigin")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select origin");
    sToolTipText    = QT_TR_NOOP("Select the local origin point of the sketch");
    sWhatsThis      = "Sketcher_SelectOrigin";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectOrigin";
    sAccel          = "Z, O";
    eType           = ForEdit;
}

void CmdSketcherSelectOrigin::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();
//    ViewProviderSketch * vp = static_cast<ViewProviderSketch *>(Gui::Application::Instance->getViewProvider(docobj));
//    Sketcher::SketchObject* Obj = vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();
    std::stringstream ss;

    ss << "RootPoint";

    if(Gui::Selection().isSelected(doc_name.c_str(), obj_name.c_str(), ss.str().c_str()))
        Gui::Selection().rmvSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
    else
        Gui::Selection().addSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
}

bool CmdSketcherSelectOrigin::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

// Select Vertical Axis
DEF_STD_CMD_A(CmdSketcherSelectVerticalAxis)

CmdSketcherSelectVerticalAxis::CmdSketcherSelectVerticalAxis()
    :Command("Sketcher_SelectVerticalAxis")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select vertical axis");
    sToolTipText    = QT_TR_NOOP("Select the local vertical axis of the sketch");
    sWhatsThis      = "Sketcher_SelectVerticalAxis";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectVerticalAxis";
    sAccel          = "Z, V";
    eType           = ForEdit;
}

void CmdSketcherSelectVerticalAxis::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();
    std::stringstream ss;

    ss << "V_Axis";

    if(Gui::Selection().isSelected(doc_name.c_str(), obj_name.c_str(), ss.str().c_str()))
      Gui::Selection().rmvSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
    else
      Gui::Selection().addSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
}

bool CmdSketcherSelectVerticalAxis::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

// Select Horizontal Axis
DEF_STD_CMD_A(CmdSketcherSelectHorizontalAxis)

CmdSketcherSelectHorizontalAxis::CmdSketcherSelectHorizontalAxis()
    :Command("Sketcher_SelectHorizontalAxis")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select horizontal axis");
    sToolTipText    = QT_TR_NOOP("Select the local horizontal axis of the sketch");
    sWhatsThis      = "Sketcher_SelectHorizontalAxis";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectHorizontalAxis";
    sAccel          = "Z, H";
    eType           = ForEdit;
}

void CmdSketcherSelectHorizontalAxis::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();
    std::stringstream ss;

    ss << "H_Axis";

    if(Gui::Selection().isSelected(doc_name.c_str(), obj_name.c_str(), ss.str().c_str()))
      Gui::Selection().rmvSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
    else
      Gui::Selection().addSelection(doc_name.c_str(), obj_name.c_str(), ss.str().c_str());
}

bool CmdSketcherSelectHorizontalAxis::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectRedundantConstraints)

CmdSketcherSelectRedundantConstraints::CmdSketcherSelectRedundantConstraints()
    :Command("Sketcher_SelectRedundantConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select redundant constraints");
    sToolTipText    = QT_TR_NOOP("Select redundant constraints");
    sWhatsThis      = "Sketcher_SelectRedundantConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectRedundantConstraints";
    sAccel          = "Z, P, R";
    eType           = ForEdit;
}

void CmdSketcherSelectRedundantConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();

    // get the needed lists and objects
    const std::vector< int > &solverredundant = vp->getSketchObject()->getLastRedundant();
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    getSelection().clearSelection();

    // push the constraints
    std::vector<std::string> constraintSubNames;

    int i = 0;
    for (std::vector< Sketcher::Constraint * >::const_iterator it= vals.begin();it != vals.end(); ++it,++i) {
        for(std::vector< int >::const_iterator itc= solverredundant.begin();itc != solverredundant.end(); ++itc) {
            if ((*itc) - 1 == i) {
                constraintSubNames.push_back(Sketcher::PropertyConstraintList::getConstraintName(i));
                break;
            }
        }
    }

    if(!constraintSubNames.empty())
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), constraintSubNames);
}

bool CmdSketcherSelectRedundantConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectMalformedConstraints)

CmdSketcherSelectMalformedConstraints::CmdSketcherSelectMalformedConstraints()
    :Command("Sketcher_SelectMalformedConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select malformed constraints");
    sToolTipText    = QT_TR_NOOP("Select malformed constraints");
    sWhatsThis      = "Sketcher_SelectMalformedConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectMalformedConstraints";
    sAccel          = "Z, P, M";
    eType           = ForEdit;
}

void CmdSketcherSelectMalformedConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();

    // get the needed lists and objects
    const std::vector< int > &solvermalformed = vp->getSketchObject()->getLastMalformedConstraints();
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    getSelection().clearSelection();

    // push the constraints
    std::vector<std::string> constraintSubNames;
    int i = 0;
    for (std::vector< Sketcher::Constraint * >::const_iterator it= vals.begin();it != vals.end(); ++it,++i) {
        for(std::vector< int >::const_iterator itc= solvermalformed.begin();itc != solvermalformed.end(); ++itc) {
            if ((*itc) - 1 == i) {
                constraintSubNames.push_back(Sketcher::PropertyConstraintList::getConstraintName(i));
                break;
            }
        }
    }

    if(!constraintSubNames.empty())
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), constraintSubNames);
}

bool CmdSketcherSelectMalformedConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectPartiallyRedundantConstraints)

CmdSketcherSelectPartiallyRedundantConstraints::CmdSketcherSelectPartiallyRedundantConstraints()
    :Command("Sketcher_SelectPartiallyRedundantConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select partially redundant constraints");
    sToolTipText    = QT_TR_NOOP("Select partially redundant constraints");
    sWhatsThis      = "Sketcher_SelectPartiallyRedundantConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectPartiallyRedundantConstraints";
    sAccel          = "Z, P, P";
    eType           = ForEdit;
}

void CmdSketcherSelectPartiallyRedundantConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();

    // get the needed lists and objects
    const std::vector< int > &solverpartiallyredundant = vp->getSketchObject()->getLastPartiallyRedundant();
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    getSelection().clearSelection();

    // push the constraints
    std::vector<std::string> constraintSubNames;
    int i = 0;
    for (std::vector< Sketcher::Constraint * >::const_iterator it= vals.begin();it != vals.end(); ++it,++i) {
        for(std::vector< int >::const_iterator itc= solverpartiallyredundant.begin();itc != solverpartiallyredundant.end(); ++itc) {
            if ((*itc) - 1 == i) {
                constraintSubNames.push_back(Sketcher::PropertyConstraintList::getConstraintName(i));
                break;
            }
        }
    }

    if(!constraintSubNames.empty())
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), constraintSubNames);
}

bool CmdSketcherSelectPartiallyRedundantConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectConflictingConstraints)

CmdSketcherSelectConflictingConstraints::CmdSketcherSelectConflictingConstraints()
    :Command("Sketcher_SelectConflictingConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select conflicting constraints");
    sToolTipText    = QT_TR_NOOP("Select conflicting constraints");
    sWhatsThis      = "Sketcher_SelectConflictingConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectConflictingConstraints";
    sAccel          = "Z, P, C";
    eType           = ForEdit;
}

void CmdSketcherSelectConflictingConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();
    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();

    // get the needed lists and objects
    const std::vector< int > &solverconflicting = vp->getSketchObject()->getLastConflicting();
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    getSelection().clearSelection();

    // push the constraints
    std::vector<std::string> constraintSubNames;
    int i = 0;
    for (std::vector< Sketcher::Constraint * >::const_iterator it= vals.begin();it != vals.end(); ++it,++i) {
        for (std::vector< int >::const_iterator itc= solverconflicting.begin();itc != solverconflicting.end(); ++itc) {
            if ((*itc) - 1 == i) {
                constraintSubNames.push_back(Sketcher::PropertyConstraintList::getConstraintName(i));
                break;
            }
        }
    }

    if(!constraintSubNames.empty())
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), constraintSubNames);
}

bool CmdSketcherSelectConflictingConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectElementsAssociatedWithConstraints)

CmdSketcherSelectElementsAssociatedWithConstraints::CmdSketcherSelectElementsAssociatedWithConstraints()
    :Command("Sketcher_SelectElementsAssociatedWithConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select associated geometry");
    sToolTipText    = QT_TR_NOOP("Select the geometrical elements associated with the selected constraints");
    sWhatsThis      = "Sketcher_SelectElementsAssociatedWithConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectElementsAssociatedWithConstraints";
    sAccel          = "Z, E";
    eType           = ForEdit;
}

void CmdSketcherSelectElementsAssociatedWithConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    std::vector<Gui::SelectionObject> selection = Gui::Selection().getSelectionEx();
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    const std::vector< Sketcher::Constraint * > &vals = Obj->Constraints.getValues();

    getSelection().clearSelection();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();
    std::stringstream ss;

    std::vector<std::string> elementSubNames;
    // go through the selected subelements
    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle constraints
        if (it->size() > 10 && it->substr(0,10) == "Constraint") {
            int ConstrId = Sketcher::PropertyConstraintList::getIndexFromConstraintName(*it);

            if(ConstrId < static_cast<int>(vals.size())){
                if(vals[ConstrId]->First!=GeoEnum::GeoUndef){
                    ss.str(std::string());

                    switch(vals[ConstrId]->FirstPos)
                    {
                        case Sketcher::PointPos::none:
                            ss << "Edge" << vals[ConstrId]->First + 1;
                            break;
                        case Sketcher::PointPos::start:
                        case Sketcher::PointPos::end:
                        case Sketcher::PointPos::mid:
                            int vertex = Obj->getVertexIndexGeoPos(vals[ConstrId]->First,vals[ConstrId]->FirstPos);
                            if(vertex>-1)
                                ss << "Vertex" <<  vertex + 1;
                            break;
                    }
                    elementSubNames.push_back(ss.str());
                }

                if(vals[ConstrId]->Second!=GeoEnum::GeoUndef){
                    ss.str(std::string());

                    switch(vals[ConstrId]->SecondPos)
                    {
                        case Sketcher::PointPos::none:
                            ss << "Edge" << vals[ConstrId]->Second + 1;
                            break;
                        case Sketcher::PointPos::start:
                        case Sketcher::PointPos::end:
                        case Sketcher::PointPos::mid:
                            int vertex = Obj->getVertexIndexGeoPos(vals[ConstrId]->Second,vals[ConstrId]->SecondPos);
                            if(vertex>-1)
                                ss << "Vertex" << vertex + 1;
                            break;
                    }

                    elementSubNames.push_back(ss.str());
                }

                if(vals[ConstrId]->Third!=GeoEnum::GeoUndef){
                    ss.str(std::string());

                    switch(vals[ConstrId]->ThirdPos)
                    {
                        case Sketcher::PointPos::none:
                            ss << "Edge" << vals[ConstrId]->Third + 1;
                            break;
                        case Sketcher::PointPos::start:
                        case Sketcher::PointPos::end:
                        case Sketcher::PointPos::mid:
                            int vertex = Obj->getVertexIndexGeoPos(vals[ConstrId]->Third,vals[ConstrId]->ThirdPos);
                            if(vertex>-1)
                                ss << "Vertex" <<  vertex + 1;
                            break;
                    }

                    elementSubNames.push_back(ss.str());
                }
            }
        }
    }

    if (elementSubNames.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("No constraint selected"),
                             QObject::tr("At least one constraint must be selected"));
    }
    else {
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), elementSubNames);
    }

}

bool CmdSketcherSelectElementsAssociatedWithConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSelectElementsWithDoFs)

CmdSketcherSelectElementsWithDoFs::CmdSketcherSelectElementsWithDoFs()
:Command("Sketcher_SelectElementsWithDoFs")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Select unconstrained DoF");
    sToolTipText    = QT_TR_NOOP("Select geometrical elements where the solver still detects unconstrained degrees of freedom.");
    sWhatsThis      = "Sketcher_SelectElementsWithDoFs";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_SelectElementsWithDoFs";
    sAccel          = "Z, F";
    eType           = ForEdit;
}

void CmdSketcherSelectElementsWithDoFs::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    getSelection().clearSelection();
    Gui::Document * doc= getActiveGuiDocument();
    ReleaseHandler(doc);
    SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
    Sketcher::SketchObject* Obj= vp->getSketchObject();

    std::string doc_name = Obj->getDocument()->getName();
    std::string obj_name = Obj->getNameInDocument();
    std::stringstream ss;

    auto geos = Obj->getInternalGeometry();

    std::vector<std::string> elementSubNames;

    auto testselectvertex = [&Obj, &ss, &elementSubNames](int geoId, PointPos pos) {
        ss.str(std::string());

        int vertex = Obj->getVertexIndexGeoPos(geoId, pos);
        if (vertex > -1) {
            ss << "Vertex" <<  vertex + 1;

            elementSubNames.push_back(ss.str());
        }
    };

    auto testselectedge = [&ss, &elementSubNames](int geoId) {
        ss.str(std::string());

        ss << "Edge" <<  geoId + 1;
        elementSubNames.push_back(ss.str());
    };

    int geoid = 0;

    for (auto geo : geos) {
        if(geo) {
            if(geo->hasExtension(Sketcher::SolverGeometryExtension::getClassTypeId())) {

                auto solvext = std::static_pointer_cast<const Sketcher::SolverGeometryExtension>(
                                    geo->getExtension(Sketcher::SolverGeometryExtension::getClassTypeId()).lock());

                if (solvext->getGeometry() == Sketcher::SolverGeometryExtension::NotFullyConstraint) {
                    // Coded for consistency with getGeometryWithDependentParameters, read the comments
                    // on that function
                    if (solvext->getEdge() == SolverGeometryExtension::Dependent)
                        testselectedge(geoid);
                    if (solvext->getStart() == SolverGeometryExtension::Dependent)
                        testselectvertex(geoid, Sketcher::PointPos::start);
                    if (solvext->getEnd() == SolverGeometryExtension::Dependent)
                        testselectvertex(geoid, Sketcher::PointPos::end);
                    if (solvext->getMid() == SolverGeometryExtension::Dependent)
                        testselectvertex(geoid, Sketcher::PointPos::mid);
                }
            }
        }

        geoid++;
    }

    if (!elementSubNames.empty()) {
        Gui::Selection().addSelections(doc_name.c_str(), obj_name.c_str(), elementSubNames);
    }

}

bool CmdSketcherSelectElementsWithDoFs::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherRestoreInternalAlignmentGeometry)

CmdSketcherRestoreInternalAlignmentGeometry::CmdSketcherRestoreInternalAlignmentGeometry()
    :Command("Sketcher_RestoreInternalAlignmentGeometry")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Show/hide internal geometry");
    sToolTipText    = QT_TR_NOOP("Show all internal geometry or hide unused internal geometry");
    sWhatsThis      = "Sketcher_RestoreInternalAlignmentGeometry";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_Element_Ellipse_All";
    sAccel          = "Z, I";
    eType           = ForEdit;
}

void CmdSketcherRestoreInternalAlignmentGeometry::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    // Cancel any in-progress operation
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    SketcherGui::ReleaseHandler(doc);

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

    getSelection().clearSelection();

    // go through the selected subelements
    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle edges
        if ((it->size() > 4 && it->substr(0,4) == "Edge") ||
            (it->size() > 12 && it->substr(0,12) == "ExternalEdge")) {
            int GeoId;
            if (it->substr(0,4) == "Edge")
               GeoId = std::atoi(it->substr(4,4000).c_str()) - 1;
            else
               GeoId = -std::atoi(it->substr(12,4000).c_str()) - 2;

            const Part::Geometry *geo = Obj->getGeometry(GeoId);
            // Only for supported types
            if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId() ||
                geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId() ||
                geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId() ||
                geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId() ||
                geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {

                int currentgeoid = Obj->getHighestCurveIndex();

                try {
                    Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Exposing Internal Geometry"));
                    Gui::cmdAppObjectArgs(Obj, "exposeInternalGeometry(%d)", GeoId);

                    int aftergeoid = Obj->getHighestCurveIndex();

                    if(aftergeoid == currentgeoid) { // if we did not expose anything, deleteunused
                        Gui::cmdAppObjectArgs(Obj, "deleteUnusedInternalGeometry(%d)", GeoId);
                    }
                }
                catch (const Base::Exception& e) {
                    Base::Console().Error("%s\n", e.what());
                    Gui::Command::abortCommand();

                    tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(Obj));

                    return;
                }

                Gui::Command::commitCommand();
                tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(Obj));
            }
        }
    }
}

bool CmdSketcherRestoreInternalAlignmentGeometry::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherSymmetry)

CmdSketcherSymmetry::CmdSketcherSymmetry()
    :Command("Sketcher_Symmetry")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Symmetry");
    sToolTipText    = QT_TR_NOOP("Creates symmetric geometry with respect to the last selected line or point");
    sWhatsThis      = "Sketcher_Symmetry";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_Symmetry";
    sAccel          = "Z, S";
    eType           = ForEdit;
}

void CmdSketcherSymmetry::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    // Cancel any in-progress operation
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    SketcherGui::ReleaseHandler(doc);

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.empty()) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());
    getSelection().clearSelection();

    int LastGeoId = 0;
    Sketcher::PointPos LastPointPos = Sketcher::PointPos::none;
    const Part::Geometry *LastGeo;
    typedef enum { invalid = -1, line = 0, point = 1 } GeoType;

    GeoType lastgeotype = invalid;

    // create python command with list of elements
    std::stringstream stream;
    int geoids = 0;

    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle non-external edges
        if ((it->size() > 4 && it->substr(0,4) == "Edge") ||
            (it->size() > 12 && it->substr(0,12) == "ExternalEdge")) {

            if (it->substr(0,4) == "Edge") {
                LastGeoId = std::atoi(it->substr(4,4000).c_str()) - 1;
                LastPointPos = Sketcher::PointPos::none;
            }
            else {
                LastGeoId = -std::atoi(it->substr(12,4000).c_str()) - 2;
                LastPointPos = Sketcher::PointPos::none;
            }

            // reference can be external or non-external
            LastGeo = Obj->getGeometry(LastGeoId);
            // Only for supported types
            if (LastGeo->getTypeId() == Part::GeomLineSegment::getClassTypeId())
                lastgeotype = line;
            else
                lastgeotype = invalid;

            // lines to make symmetric (only non-external)
            if (LastGeoId >= 0) {
                geoids++;
                stream << LastGeoId << ",";
            }
        }
        else if (it->size() > 6 && it->substr(0,6) == "Vertex") {
            // only if it is a GeomPoint
            int VtId = std::atoi(it->substr(6,4000).c_str()) - 1;
            int GeoId;
            Sketcher::PointPos PosId;
            Obj->getGeoVertexIndex(VtId, GeoId, PosId);

            if (Obj->getGeometry(GeoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                LastGeoId = GeoId;
                LastPointPos = Sketcher::PointPos::start;
                lastgeotype = point;

                // points to make symmetric
                if (LastGeoId >= 0) {
                    geoids++;
                    stream << LastGeoId << ",";
                }
            }
        }
    }

    bool lastvertexoraxis = false;
    // check if last selected element is a Vertex, not being a GeomPoint
    if (SubNames.rbegin()->size() > 6 && SubNames.rbegin()->substr(0,6) == "Vertex") {
        int VtId = std::atoi(SubNames.rbegin()->substr(6,4000).c_str()) - 1;
        int GeoId;
        Sketcher::PointPos PosId;
        Obj->getGeoVertexIndex(VtId, GeoId, PosId);
        if (Obj->getGeometry(GeoId)->getTypeId() != Part::GeomPoint::getClassTypeId()) {
            LastGeoId = GeoId;
            LastPointPos = PosId;
            lastgeotype = point;
            lastvertexoraxis = true;
        }
    }
    // check if last selected element is horizontal axis
    else if (SubNames.rbegin()->size() == 6 && SubNames.rbegin()->substr(0,6) == "H_Axis") {
        LastGeoId = Sketcher::GeoEnum::HAxis;
        LastPointPos = Sketcher::PointPos::none;
        lastgeotype = line;
        lastvertexoraxis = true;
    }
    // check if last selected element is vertical axis
    else if (SubNames.rbegin()->size() == 6 && SubNames.rbegin()->substr(0,6) == "V_Axis") {
        LastGeoId = Sketcher::GeoEnum::VAxis;
        LastPointPos = Sketcher::PointPos::none;
        lastgeotype = line;
        lastvertexoraxis = true;
    }
    // check if last selected element is the root point
    else if (SubNames.rbegin()->size() == 9 && SubNames.rbegin()->substr(0,9) == "RootPoint") {
        LastGeoId = Sketcher::GeoEnum::RtPnt;
        LastPointPos = Sketcher::PointPos::start;
        lastgeotype = point;
        lastvertexoraxis = true;
    }

    if (geoids == 0 || (geoids == 1 && LastGeoId >= 0 && !lastvertexoraxis)) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("A symmetric construction requires "
                        "at least two geometric elements, "
                        "the last geometric element being the reference "
                        "for the symmetry construction."));
        return;
    }

    if (lastgeotype == invalid) {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Wrong selection"),
            QObject::tr("The last element must be a point "
                        "or a line serving as reference "
                        "for the symmetry construction."));
        return;
    }

    std::string geoIdList = stream.str();

    // missing cases:
    // 1- Last element is an edge, and is V or H axis
    // 2- Last element is a point GeomPoint
    // 3- Last element is a point (Vertex)

    if (LastGeoId >= 0 && !lastvertexoraxis) {
        // if LastGeoId was added remove the last element
        int index = geoIdList.rfind(',');
        index = geoIdList.rfind(',', index-1);
        geoIdList.resize(index);
    }
    else {
        int index = geoIdList.rfind(',');
        geoIdList.resize(index);
    }

    geoIdList.insert(0, 1, '[');
    geoIdList.append(1, ']');

    Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Create symmetric geometry"));

    try{
        Gui::cmdAppObjectArgs(Obj,
                              "addSymmetric(%s, %d, %d)",
                              geoIdList.c_str(), LastGeoId, static_cast<int>(LastPointPos));
        Gui::Command::commitCommand();
    }
    catch (const Base::Exception& e) {
        Base::Console().Error("%s\n", e.what());
        Gui::Command::abortCommand();
    }
    tryAutoRecomputeIfNotSolve(Obj);
}

bool CmdSketcherSymmetry::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

class SketcherCopy : public Gui::Command {
public:
    enum Op {
        Copy,
        Clone,
        Move
    };
    SketcherCopy(const char* name);
    void activate(SketcherCopy::Op op);
    virtual void activate() = 0;
};

// TODO: replace XPM cursor with SVG file
static const char *cursor_createcopy[]={
    "32 32 3 1",
    "+ c white",
    "# c red",
    ". c None",
    "................................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    "................................",
    ".+++++...+++++..................",
    "................................",
    ".......+........................",
    ".......+..............###.......",
    ".......+..............###.......",
    ".......+..............###.......",
    ".......+..............###.......",
    "......................###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###..............###.......",
    ".....###........................",
    ".....###........................",
    ".....###........................",
    ".....###........................",
    "................................",
    "................................",
    "................................",
    "................................"};

class DrawSketchHandlerCopy: public DrawSketchHandler
{
public:
    DrawSketchHandlerCopy(string geoidlist, int origingeoid,
                          Sketcher::PointPos originpos, int nelements,
                          SketcherCopy::Op op)
    : Mode(STATUS_SEEK_First)
    , snapMode(SnapMode::Free)
    , geoIdList(geoidlist)
    , Origin()
    , OriginGeoId(origingeoid)
    , OriginPos(originpos)
    , nElements(nelements)
    , Op(op)
    , EditCurve(2)
    {
    }

    virtual ~DrawSketchHandlerCopy(){}
    /// mode table
    enum SelectMode {
        STATUS_SEEK_First,      /**< enum value ----. */
        STATUS_End
    };

    enum class SnapMode {
        Free,
        Snap5Degree
    };

    virtual void activated(ViewProviderSketch *sketchgui)
    {
        setCursor(QPixmap(cursor_createcopy), 7, 7);
        Origin = static_cast<Sketcher::SketchObject *>(sketchgui->getObject())->getPoint(OriginGeoId, OriginPos);
        EditCurve[0] = Base::Vector2d(Origin.x, Origin.y);
    }

    virtual void mouseMove(Base::Vector2d onSketchPos)
    {
        if (Mode == STATUS_SEEK_First) {

             if(QApplication::keyboardModifiers() == Qt::ControlModifier)
                    snapMode = SnapMode::Snap5Degree;
                else
                    snapMode = SnapMode::Free;

            float length = (onSketchPos - EditCurve[0]).Length();
            float angle = (onSketchPos - EditCurve[0]).Angle();

            Base::Vector2d endpoint = onSketchPos;

            if (snapMode == SnapMode::Snap5Degree) {
                angle = round(angle / (M_PI/36)) * M_PI/36;
                endpoint = EditCurve[0] + length * Base::Vector2d(cos(angle),sin(angle));
            }

            SbString text;
            text.sprintf(" (%.1f, %.1fdeg)", length, angle * 180 / M_PI);
            setPositionText(endpoint, text);

            EditCurve[1] = endpoint;
            drawEdit(EditCurve);
            if (seekAutoConstraint(sugConstr1, endpoint, Base::Vector2d(0.0, 0.0), AutoConstraint::VERTEX)) {
                renderSuggestConstraintsCursor(sugConstr1);
                return;
            }
        }
        applyCursor();
    }

    virtual bool pressButton(Base::Vector2d)
    {
        if (Mode == STATUS_SEEK_First) {
            drawEdit(EditCurve);
            Mode = STATUS_End;
        }
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos)
    {
        Q_UNUSED(onSketchPos);
        if (Mode == STATUS_End)
        {
            Base::Vector2d vector = EditCurve[1] - EditCurve[0];
            unsetCursor();
            resetPositionText();

            int currentgeoid = static_cast<Sketcher::SketchObject *>(sketchgui->getObject())->getHighestCurveIndex();
            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Copy/clone/move geometry"));

            try{
                if (Op != SketcherCopy::Move) {
                    Gui::cmdAppObjectArgs(sketchgui->getObject(),
                                          "addCopy(%s, App.Vector(%f, %f, 0), %s)",
                                          geoIdList.c_str(), vector.x, vector.y,
                                          (Op == SketcherCopy::Clone ? "True" : "False"));
                }
                else {
                    Gui::cmdAppObjectArgs(sketchgui->getObject(),
                                          "addMove(%s, App.Vector(%f, %f, 0))",
                                          geoIdList.c_str(), vector.x, vector.y);
                }
                Gui::Command::commitCommand();
            }
            catch (const Base::Exception& e) {
                Base::Console().Error("%s\n", e.what());
                Gui::Command::abortCommand();
            }

            if (Op != SketcherCopy::Move) {
                // add auto constraints for the destination copy
                if (sugConstr1.size() > 0) {
                    createAutoConstraints(sugConstr1, currentgeoid+nElements, OriginPos);
                    sugConstr1.clear();
                }
            }
            else {
                if (sugConstr1.size() > 0) {
                    createAutoConstraints(sugConstr1, OriginGeoId, OriginPos);
                    sugConstr1.clear();
                }
            }

            tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(sketchgui->getObject()));
            EditCurve.clear();
            drawEdit(EditCurve);

            // no code after this line, Handler gets deleted in ViewProvider
            sketchgui->purgeHandler();
        }
        return true;
    }
protected:
    SelectMode Mode;
    SnapMode snapMode;
    string geoIdList;
    Base::Vector3d Origin;
    int OriginGeoId;
    Sketcher::PointPos OriginPos;
    int nElements;
    SketcherCopy::Op Op;
    std::vector<Base::Vector2d> EditCurve;
    std::vector<AutoConstraint> sugConstr1;
};

/*---- SketcherCopy definition ----*/
SketcherCopy::SketcherCopy(const char* name): Command(name)
{}

void SketcherCopy::activate(SketcherCopy::Op op)
{
    // get the selection
    std::vector<Gui::SelectionObject> selection = getSelection().getSelectionEx();

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.empty()) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());
    getSelection().clearSelection();

    int LastGeoId = 0;
    Sketcher::PointPos LastPointPos = Sketcher::PointPos::none;
    const Part::Geometry *LastGeo = 0;

    // create python command with list of elements
    std::stringstream stream;
    int geoids = 0;
    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle non-external edges
        if (it->size() > 4 && it->substr(0,4) == "Edge") {
            LastGeoId = std::atoi(it->substr(4,4000).c_str()) - 1;
            LastPointPos = Sketcher::PointPos::none;
            LastGeo = Obj->getGeometry(LastGeoId);
            // lines to copy
            if (LastGeoId >= 0) {
                geoids++;
                stream << LastGeoId << ",";
            }
        }
        else if (it->size() > 6 && it->substr(0,6) == "Vertex") {
            // only if it is a GeomPoint
            int VtId = std::atoi(it->substr(6,4000).c_str()) - 1;
            int GeoId;
            Sketcher::PointPos PosId;
            Obj->getGeoVertexIndex(VtId, GeoId, PosId);
            if (Obj->getGeometry(GeoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                LastGeoId = GeoId;
                LastPointPos = Sketcher::PointPos::start;
                // points to copy
                if (LastGeoId >= 0) {
                    geoids++;
                    stream << LastGeoId << ",";
                }
            }
        }
    }

    // check if last selected element is a Vertex, not being a GeomPoint
    if (SubNames.rbegin()->size() > 6 && SubNames.rbegin()->substr(0,6) == "Vertex") {
        int VtId = std::atoi(SubNames.rbegin()->substr(6,4000).c_str()) - 1;
        int GeoId;
        Sketcher::PointPos PosId;
        Obj->getGeoVertexIndex(VtId, GeoId, PosId);
        if (Obj->getGeometry(GeoId)->getTypeId() != Part::GeomPoint::getClassTypeId()) {
            LastGeoId = GeoId;
            LastPointPos = PosId;
        }
    }

    if (geoids < 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("A copy requires at least one selected non-external geometric element"));
        return;
    }

    std::string geoIdList = stream.str();

    // remove the last added comma and brackets to make the python list
    int index = geoIdList.rfind(',');
    geoIdList.resize(index);
    geoIdList.insert(0, 1, '[');
    geoIdList.append(1, ']');

    // if the last element is not a point serving as a reference for the copy process
    // then make the start point of the last element the copy reference (if it exists, if not the center point)
    if (LastPointPos == Sketcher::PointPos::none) {
        if (LastGeo->getTypeId() == Part::GeomCircle::getClassTypeId() ||
            LastGeo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
            LastPointPos = Sketcher::PointPos::mid;
        }
        else {
            LastPointPos = Sketcher::PointPos::start;
        }
    }

    // Ask the user if they want to clone or to simple copy
/*
    int ret = QMessageBox::question(Gui::getMainWindow(), QObject::tr("Dimensional/Geometric constraints"),
                                    QObject::tr("Do you want to clone the object, i.e. substitute dimensional constraints by geometric constraints?"),
                                    QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
    // use an equality constraint
    if (ret == QMessageBox::Yes) {
        clone = true;
    }
    else if (ret == QMessageBox::Cancel) {
    // do nothing
    return;
    }
*/

    ActivateAcceleratorHandler(getActiveGuiDocument(),
                               new DrawSketchHandlerCopy(geoIdList, LastGeoId, LastPointPos, geoids, op));
}


class CmdSketcherCopy : public SketcherCopy
{
public:
    CmdSketcherCopy();
    virtual ~CmdSketcherCopy(){}
    virtual const char* className() const
    { return "CmdSketcherCopy"; }
    virtual void activate();
protected:
    virtual void activated(int iMsg);
    virtual bool isActive(void);
};

CmdSketcherCopy::CmdSketcherCopy()
    :SketcherCopy("Sketcher_Copy")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Copy");
    sToolTipText    = QT_TR_NOOP("Creates a simple copy of the geometry taking as reference the last selected point");
    sWhatsThis      = "Sketcher_Copy";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_Copy";
    sAccel          = "Z, C";
    eType           = ForEdit;
}

void CmdSketcherCopy::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    SketcherCopy::activate(SketcherCopy::Copy);
}


void CmdSketcherCopy::activate()
{
    SketcherCopy::activate(SketcherCopy::Copy);
}

bool CmdSketcherCopy::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

class CmdSketcherClone : public SketcherCopy
{
public:
    CmdSketcherClone();
    virtual ~CmdSketcherClone(){}
    virtual const char* className() const
    { return "CmdSketcherClone"; }
    virtual void activate();
protected:
    virtual void activated(int iMsg);
    virtual bool isActive(void);
};

CmdSketcherClone::CmdSketcherClone()
    :SketcherCopy("Sketcher_Clone")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Clone");
    sToolTipText    = QT_TR_NOOP("Creates a clone of the geometry taking as reference the last selected point");
    sWhatsThis      = "Sketcher_Clone";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_Clone";
    sAccel          = "Z, L";
    eType           = ForEdit;
}

void CmdSketcherClone::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    SketcherCopy::activate(SketcherCopy::Clone);
}

void CmdSketcherClone::activate()
{
    SketcherCopy::activate(SketcherCopy::Clone);
}

bool CmdSketcherClone::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

class CmdSketcherMove : public SketcherCopy
{
public:
    CmdSketcherMove();
    virtual ~CmdSketcherMove(){}
    virtual const char* className() const
    { return "CmdSketcherMove"; }
    virtual void activate();
protected:
    virtual void activated(int iMsg);
    virtual bool isActive(void);
};

CmdSketcherMove::CmdSketcherMove()
    :SketcherCopy("Sketcher_Move")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Move");
    sToolTipText    = QT_TR_NOOP("Moves the geometry taking as reference the last selected point");
    sWhatsThis      = "Sketcher_Move";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_Move";
    sAccel          = "Z, M";
    eType           = ForEdit;
}

void CmdSketcherMove::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    SketcherCopy::activate(SketcherCopy::Move);
}

void CmdSketcherMove::activate()
{
    SketcherCopy::activate(SketcherCopy::Move);
}

bool CmdSketcherMove::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// ================================================================================

DEF_STD_CMD_ACL(CmdSketcherCompCopy)

CmdSketcherCompCopy::CmdSketcherCompCopy()
    : Command("Sketcher_CompCopy")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Copy");
    sToolTipText    = QT_TR_NOOP("Creates a clone of the geometry taking as reference the last selected point");
    sWhatsThis      = "Sketcher_CompCopy";
    sStatusTip      = sToolTipText;
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherCompCopy::activated(int iMsg)
{
    if (iMsg<0 || iMsg>2)
        return;

    // Since the default icon is reset when enabling/disabling the command we have
    // to explicitly set the icon of the used command.
    Gui::ActionGroup* pcAction = qobject_cast<Gui::ActionGroup*>(_pcAction);
    QList<QAction*> a = pcAction->actions();

    assert(iMsg < a.size());
    pcAction->setIcon(a[iMsg]->icon());

    if (iMsg == 0){
        CmdSketcherClone sc;
        sc.activate();
        pcAction->setShortcut(QString::fromLatin1(this->getAccel()));
    }
    else if (iMsg == 1) {
        CmdSketcherCopy sc;
        sc.activate();
        pcAction->setShortcut(QString::fromLatin1(this->getAccel()));
    }
    else if (iMsg == 2) {
        CmdSketcherMove sc;
        sc.activate();
        pcAction->setShortcut(QString::fromLatin1(""));
    }
}

Gui::Action * CmdSketcherCompCopy::createAction(void)
{
    Gui::ActionGroup* pcAction = new Gui::ActionGroup(this, Gui::getMainWindow());
    pcAction->setDropDownMenu(true);
    applyCommandData(this->className(), pcAction);

    QAction* clone = pcAction->addAction(QString());
    clone->setIcon(Gui::BitmapFactory().iconFromTheme("Sketcher_Clone"));
    QAction* copy = pcAction->addAction(QString());
    copy->setIcon(Gui::BitmapFactory().iconFromTheme("Sketcher_Copy"));
    QAction* move = pcAction->addAction(QString());
    move->setIcon(Gui::BitmapFactory().iconFromTheme("Sketcher_Move"));

    _pcAction = pcAction;
    languageChange();

    pcAction->setIcon(clone->icon());
    int defaultId = 0;
    pcAction->setProperty("defaultAction", QVariant(defaultId));

    pcAction->setShortcut(QString::fromLatin1(getAccel()));

    return pcAction;
}

void CmdSketcherCompCopy::languageChange()
{
    Command::languageChange();

    if (!_pcAction)
        return;
    Gui::ActionGroup* pcAction = qobject_cast<Gui::ActionGroup*>(_pcAction);
    QList<QAction*> a = pcAction->actions();

    QAction* clone = a[0];
    clone->setText(QApplication::translate("Sketcher_CompCopy","Clone"));
    clone->setToolTip(QApplication::translate("Sketcher_Clone","Creates a clone of the geometry taking as reference the last selected point"));
    clone->setStatusTip(QApplication::translate("Sketcher_Clone","Creates a clone of the geometry taking as reference the last selected point"));
    QAction* copy = a[1];
    copy->setText(QApplication::translate("Sketcher_CompCopy","Copy"));
    copy->setToolTip(QApplication::translate("Sketcher_Copy","Creates a simple copy of the geometry taking as reference the last selected point"));
    copy->setStatusTip(QApplication::translate("Sketcher_Copy","Creates a simple copy of the geometry taking as reference the last selected point"));
    QAction* move = a[2];
    move->setText(QApplication::translate("Sketcher_CompCopy","Move"));
    move->setToolTip(QApplication::translate("Sketcher_Move","Moves the geometry taking as reference the last selected point"));
    move->setStatusTip(QApplication::translate("Sketcher_Move","Moves the geometry taking as reference the last selected point"));
}

bool CmdSketcherCompCopy::isActive(void)
{
    return isSketcherAcceleratorActive( getActiveGuiDocument(), true );
}

// ================================================================================

// TODO: replace XPM cursor with SVG file
/* XPM */
static const char *cursor_createrectangulararray[]={
    "32 32 3 1",
    "+ c white",
    "# c red",
    ". c None",
    "................................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    ".......+........................",
    "................................",
    ".+++++...+++++..................",
    ".......................###......",
    ".......+...............###......",
    ".......+...............###......",
    ".......+...............###......",
    ".......+......###......###......",
    ".......+......###......###......",
    "..............###......###......",
    "..............###......###......",
    ".....###......###......###......",
    ".....###......###......###......",
    ".....###......###......###......",
    ".....###......###......###......",
    ".....###......###......###......",
    ".....###......###......###......",
    ".....###......###...............",
    ".....###......###...............",
    ".....###......###...............",
    ".....###......###...............",
    ".....###........................",
    ".....###........................",
    ".....###........................",
    ".....###........................",
    "................................",
    "................................"};

class DrawSketchHandlerRectangularArray: public DrawSketchHandler
{
public:
    DrawSketchHandlerRectangularArray(string geoidlist, int origingeoid,
                                      Sketcher::PointPos originpos, int nelements, bool clone,
                                      int rows, int cols, bool constraintSeparation,
                                      bool equalVerticalHorizontalSpacing)
        : Mode(STATUS_SEEK_First)
        , snapMode(SnapMode::Free)
        , geoIdList(geoidlist)
        , OriginGeoId(origingeoid)
        , OriginPos(originpos)
        , nElements(nelements)
        , Clone(clone)
        , Rows(rows)
        , Cols(cols)
        , ConstraintSeparation(constraintSeparation)
        , EqualVerticalHorizontalSpacing(equalVerticalHorizontalSpacing)
        , EditCurve(2)
    {
    }

    virtual ~DrawSketchHandlerRectangularArray(){}
    /// mode table
    enum SelectMode {
        STATUS_SEEK_First,      /**< enum value ----. */
        STATUS_End
    };

    enum class SnapMode {
        Free,
        Snap5Degree
    };

    virtual void activated(ViewProviderSketch *sketchgui)
    {
        setCursor(QPixmap(cursor_createrectangulararray), 7, 7);
        Origin = static_cast<Sketcher::SketchObject *>(sketchgui->getObject())->getPoint(OriginGeoId, OriginPos);
        EditCurve[0] = Base::Vector2d(Origin.x, Origin.y);
    }

    virtual void mouseMove(Base::Vector2d onSketchPos)
    {
        if (Mode==STATUS_SEEK_First) {

            if(QApplication::keyboardModifiers() == Qt::ControlModifier)
                    snapMode = SnapMode::Snap5Degree;
                else
                    snapMode = SnapMode::Free;

            float length = (onSketchPos - EditCurve[0]).Length();
            float angle = (onSketchPos - EditCurve[0]).Angle();

            Base::Vector2d endpoint = onSketchPos;

            if (snapMode == SnapMode::Snap5Degree) {
                angle = round(angle / (M_PI/36)) * M_PI/36;
                endpoint = EditCurve[0] + length * Base::Vector2d(cos(angle),sin(angle));
            }

            SbString text;
            text.sprintf(" (%.1f, %.1fdeg)", length, angle * 180 / M_PI);
            setPositionText(endpoint, text);

            EditCurve[1] = endpoint;
            drawEdit(EditCurve);
            if (seekAutoConstraint(sugConstr1, endpoint, Base::Vector2d(0.0, 0.0), AutoConstraint::VERTEX))
            {
                renderSuggestConstraintsCursor(sugConstr1);
                return;
            }

        }
        applyCursor();
    }

    virtual bool pressButton(Base::Vector2d)
    {
        if (Mode == STATUS_SEEK_First) {
            drawEdit(EditCurve);
            Mode = STATUS_End;
        }
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos)
    {
        Q_UNUSED(onSketchPos);
        if (Mode == STATUS_End) {
            Base::Vector2d vector = EditCurve[1] - EditCurve[0];
            unsetCursor();
            resetPositionText();

            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Create copy of geometry"));

            try {
                Gui::cmdAppObjectArgs(sketchgui->getObject(),
                                      "addRectangularArray(%s, App.Vector(%f, %f, 0), %s, %d, %d, %s, %f)",
                                      geoIdList.c_str(), vector.x, vector.y,
                                      (Clone ? "True" : "False"),
                                      Cols, Rows,
                                      (ConstraintSeparation ? "True" : "False"),
                                      (EqualVerticalHorizontalSpacing ? 1.0 : 0.5));
                Gui::Command::commitCommand();
            }
            catch (const Base::Exception& e) {
                Base::Console().Error("%s\n", e.what());
                Gui::Command::abortCommand();
            }

            // add auto constraints for the destination copy
            if (sugConstr1.size() > 0) {
                createAutoConstraints(sugConstr1, OriginGeoId+nElements, OriginPos);
                sugConstr1.clear();
            }
            tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(sketchgui->getObject()));

            EditCurve.clear();
            drawEdit(EditCurve);

            // no code after this line, Handler is deleted in ViewProvider
            sketchgui->purgeHandler();
        }
        return true;
    }
protected:
    SelectMode Mode;
    SnapMode snapMode;
    string geoIdList;
    Base::Vector3d Origin;
    int OriginGeoId;
    Sketcher::PointPos OriginPos;
    int nElements;
    bool Clone;
    int Rows;
    int Cols;
    bool ConstraintSeparation;
    bool EqualVerticalHorizontalSpacing;
    std::vector<Base::Vector2d> EditCurve;
    std::vector<AutoConstraint> sugConstr1;
};

DEF_STD_CMD_A(CmdSketcherRectangularArray)

CmdSketcherRectangularArray::CmdSketcherRectangularArray()
    :Command("Sketcher_RectangularArray")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Rectangular array");
    sToolTipText    = QT_TR_NOOP("Creates a rectangular array pattern of the geometry taking as reference the last selected point");
    sWhatsThis      = "Sketcher_RectangularArray";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_RectangularArray";
    sAccel          = "Z, A";
    eType           = ForEdit;
}

void CmdSketcherRectangularArray::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.empty()) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

    getSelection().clearSelection();

    int LastGeoId = 0;
    Sketcher::PointPos LastPointPos = Sketcher::PointPos::none;
    const Part::Geometry *LastGeo = 0;

    // create python command with list of elements
    std::stringstream stream;
    int geoids = 0;

    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle non-external edges
        if (it->size() > 4 && it->substr(0,4) == "Edge") {
            LastGeoId = std::atoi(it->substr(4,4000).c_str()) - 1;
            LastPointPos = Sketcher::PointPos::none;
            LastGeo = Obj->getGeometry(LastGeoId);

            // lines to copy
            if (LastGeoId >= 0) {
                geoids++;
                stream << LastGeoId << ",";
            }
        }
        else if (it->size() > 6 && it->substr(0,6) == "Vertex") {
            // only if it is a GeomPoint
            int VtId = std::atoi(it->substr(6,4000).c_str()) - 1;
            int GeoId;
            Sketcher::PointPos PosId;
            Obj->getGeoVertexIndex(VtId, GeoId, PosId);
            if (Obj->getGeometry(GeoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                LastGeoId = GeoId;
                LastPointPos = Sketcher::PointPos::start;
                // points to copy
                if (LastGeoId >= 0) {
                    geoids++;
                    stream << LastGeoId << ",";
                }
            }
        }
    }

    // check if last selected element is a Vertex, not being a GeomPoint
    if (SubNames.rbegin()->size() > 6 && SubNames.rbegin()->substr(0,6) == "Vertex") {
        int VtId = std::atoi(SubNames.rbegin()->substr(6,4000).c_str()) - 1;
        int GeoId;
        Sketcher::PointPos PosId;
        Obj->getGeoVertexIndex(VtId, GeoId, PosId);
        if (Obj->getGeometry(GeoId)->getTypeId() != Part::GeomPoint::getClassTypeId()) {
            LastGeoId = GeoId;
            LastPointPos = PosId;
        }
    }

    if (geoids < 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("A copy requires at least one selected non-external geometric element"));
        return;
    }

    std::string geoIdList = stream.str();

    // remove the last added comma and brackets to make the python list
    int index = geoIdList.rfind(',');
    geoIdList.resize(index);
    geoIdList.insert(0, 1, '[');
    geoIdList.append(1, ']');

    // if the last element is not a point serving as a reference for the copy process
    // then make the start point of the last element the copy reference (if it exists, if not the center point)
    if (LastPointPos == Sketcher::PointPos::none) {
        if (LastGeo->getTypeId() == Part::GeomCircle::getClassTypeId() ||
            LastGeo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
            LastPointPos = Sketcher::PointPos::mid;
        }
        else {
            LastPointPos = Sketcher::PointPos::start;
        }
    }

    // Pop-up asking for values
    SketchRectangularArrayDialog slad;

    if (slad.exec() == QDialog::Accepted) {
        ActivateAcceleratorHandler(getActiveGuiDocument(),
            new DrawSketchHandlerRectangularArray(geoIdList, LastGeoId, LastPointPos, geoids, slad.Clone,
                                                  slad.Rows, slad.Cols, slad.ConstraintSeparation,
                                                  slad.EqualVerticalHorizontalSpacing));
    }
}

bool CmdSketcherRectangularArray::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

// Rotate / circular pattern tool =======================================================

class DrawSketchHandlerRotate : public DrawSketchHandler
{
public:
    DrawSketchHandlerRotate(std::vector<int> listOfGeoIds)
        : Mode(STATUS_SEEK_First)
        , EditCurve(3)
        , numberOfCopies(0)
        , deleteOriginal(0)
        , needUpdateGeos(1)
        , snapMode(SnapMode::Free)
        , listOfGeoIds(listOfGeoIds) {}
    virtual ~DrawSketchHandlerRotate() {}

    enum SelectMode {
        STATUS_SEEK_First,
        STATUS_SEEK_Second,
        STATUS_SEEK_Third,       /**< enum value ----. */
        STATUS_End
    };

    enum class SnapMode {
        Free,
        Snap5Degree
    };

    virtual void activated(ViewProviderSketch*)
    {
        toolSettings->widget->setSettings(17);
        Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Rotate"));
        toolSettings->widget->setLabel(QApplication::translate("Rotate_1", "Select the center of the rotation."), 6);
        firstCurveCreated = getHighestCurveIndex() + 1;


        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/General");
        previewEnabled = hGrp->GetBool("RotateEnablePreview", true);

        // Constrain icon size in px
        qreal pixelRatio = devicePixelRatio();
        const unsigned long defaultCrosshairColor = 0xFFFFFF;
        unsigned long color = getCrosshairColor();
        auto colorMapping = std::map<unsigned long, unsigned long>();
        colorMapping[defaultCrosshairColor] = color;

        qreal fullIconWidth = 32 * pixelRatio;
        qreal iconWidth = 16 * pixelRatio;
        QPixmap cursorPixmap = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Crosshair", QSizeF(fullIconWidth, fullIconWidth), colorMapping),
            icon = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Rotate", QSizeF(iconWidth, iconWidth));
        QPainter cursorPainter;
        cursorPainter.begin(&cursorPixmap);
        cursorPainter.drawPixmap(16 * pixelRatio, 16 * pixelRatio, icon);
        cursorPainter.end();
        int hotX = 8;
        int hotY = 8;
        cursorPixmap.setDevicePixelRatio(pixelRatio);
        // only X11 needs hot point coordinates to be scaled
        if (qGuiApp->platformName() == QLatin1String("xcb")) {
            hotX *= pixelRatio;
            hotY *= pixelRatio;
        }
        setCursor(cursorPixmap, hotX, hotY, false);
    }
    virtual void deactivated(ViewProviderSketch*)
    {
        //delete created constrains if the tool is exited before validating by left clicking somewhere
        Gui::Command::abortCommand();
        sketchgui->getSketchObject()->solve(true);
        sketchgui->draw(false, false); // Redraw
    }

    virtual void mouseMove(Base::Vector2d onSketchPos)
    {
        if (QApplication::keyboardModifiers() == Qt::ControlModifier)
            snapMode = SnapMode::Snap5Degree;
        else
            snapMode = SnapMode::Free;

        if (Mode == STATUS_SEEK_First) {
            setPositionText(onSketchPos);
            if (snapMode == SnapMode::Snap5Degree && getSnapPoint(centerPoint)) {
                setPositionText(centerPoint);
            }

            if (toolSettings->widget->isSettingSet[0] && toolSettings->widget->isSettingSet[1]) {
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
            }
        }
        else if (Mode == STATUS_SEEK_Second) {
            toolSettings->widget->setLabel(QApplication::translate("Rotate_2", "Select a first point that will define the rotation angle with the next point."), 6);
            length = (onSketchPos - centerPoint).Length();
            startAngle = (onSketchPos - centerPoint).Angle();

            Base::Vector2d endpoint = onSketchPos;

            if (snapMode == SnapMode::Snap5Degree) {
                if (getSnapPoint(endpoint)) {
                    startAngle = (endpoint - centerPoint).Angle();
                }
                else {
                    startAngle = round(startAngle / (M_PI / 36)) * M_PI / 36;
                    endpoint = centerPoint + length * Base::Vector2d(cos(startAngle), sin(startAngle));
                }
            }

            SbString text;
            text.sprintf(" (%.1f, %.1fdeg)", length, startAngle * 180 / M_PI);
            setPositionText(endpoint, text);

            EditCurve[0] = endpoint;
            EditCurve[1] = centerPoint;
            EditCurve[2] = endpoint;
            drawEdit(EditCurve);

            if (toolSettings->widget->isSettingSet[2] == 1) {
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
            }
        }
        else if (Mode == STATUS_SEEK_Third) {
            toolSettings->widget->setLabel(QApplication::translate("Rotate_2", "Select the second point that will determine the rotation angle."), 6);
            endAngle = (onSketchPos - centerPoint).Angle();
            Base::Vector2d endpoint = onSketchPos;

            if (toolSettings->widget->isSettingSet[2]) {
                totalAngle = toolSettings->widget->toolParameters[2];
                endpoint = centerPoint + length * Base::Vector2d(cos(totalAngle + startAngle), sin(totalAngle + startAngle));
            }
            else {
                if (snapMode == SnapMode::Snap5Degree) {
                    if (getSnapPoint(endpoint)) {
                        endAngle = (endpoint - centerPoint).Angle();
                    }
                    else {
                        endAngle = round(endAngle / (M_PI / 36)) * M_PI / 36;
                        endpoint = centerPoint + length * Base::Vector2d(cos(endAngle), sin(endAngle));
                    }
                }
                else {
                    endpoint = centerPoint + length * Base::Vector2d(cos(endAngle), sin(endAngle));
                }
                double angle1 = atan2(endpoint.y - centerPoint.y,
                    endpoint.x - centerPoint.x) - startAngle;
                double angle2 = angle1 + (angle1 < 0. ? 2 : -2) * M_PI;
                totalAngle = abs(angle1 - totalAngle) < abs(angle2 - totalAngle) ? angle1 : angle2;
            }

            if (toolSettings->widget->isSettingSet[3]) {
                numberOfCopies = floor(abs(toolSettings->widget->toolParameters[3]));
            }

            //generate the copies
            if (previewEnabled) {
                generateRotatedGeos(0);
                sketchgui->draw(false, false); // Redraw
            }

            SbString text;
            text.sprintf(" (%d copies, %.1fdeg)", numberOfCopies, totalAngle * 180 / M_PI);
            setPositionText(endpoint, text);

            EditCurve[2] = endpoint;
            drawEdit(EditCurve);

            if (toolSettings->widget->isSettingSet[2] + toolSettings->widget->isSettingSet[3] == 2) {
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
            }
        }
        applyCursor();
    }

    virtual bool pressButton(Base::Vector2d onSketchPos)
    {
        if (Mode == STATUS_SEEK_First) {
            if (!(snapMode == SnapMode::Snap5Degree && getSnapPoint(centerPoint))) {
                //note: if getSnapPoint returns true, then centerPoint is modified already
                centerPoint = onSketchPos;
                if (toolSettings->widget->isSettingSet[0] == 1) {
                    centerPoint.x = toolSettings->widget->toolParameters[0];
                }
                if (toolSettings->widget->isSettingSet[1] == 1) {
                    centerPoint.y = toolSettings->widget->toolParameters[1];
                }
            }

            EditCurve[0] = centerPoint;
            toolSettings->widget->setParameterActive(0, 0);
            toolSettings->widget->setParameterActive(0, 1);
            toolSettings->widget->setParameterActive(1, 2);
            toolSettings->widget->setParameterActive(1, 3);
            toolSettings->widget->setParameterFocus(2);
            Mode = STATUS_SEEK_Second;
        }
        else if (Mode == STATUS_SEEK_Second) {
            Mode = STATUS_SEEK_Third;
        }
        else if (Mode == STATUS_SEEK_Third) {
            Mode = STATUS_End;
        }
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos)
    {
        Q_UNUSED(onSketchPos);
        if (Mode == STATUS_End) {
            generateRotatedGeos(1);

            Gui::Command::commitCommand();

            EditCurve.clear();
            drawEdit(EditCurve);

            sketchgui->getSketchObject()->solve(true);
            sketchgui->draw(false, false); // Redraw

            sketchgui->purgeHandler();
        }
        return true;
    }
protected:
    SelectMode Mode;
    SnapMode snapMode;
    std::vector<int> listOfGeoIds;
    Base::Vector2d centerPoint;
    std::vector<Base::Vector2d> EditCurve;

    bool deleteOriginal, previewEnabled, needUpdateGeos;
    double length, startAngle, endAngle, totalAngle, individualAngle;
    int numberOfCopies, prevNumberOfCopies, firstCurveCreated;

    void generateRotatedGeos(bool onReleaseButton) {
        int numberOfCopiesToMake = numberOfCopies;
        if (numberOfCopies == 0) {
            numberOfCopiesToMake = 1;
            deleteOriginal = 1;
        }
        else {
            deleteOriginal = 0;
        }

        if (prevNumberOfCopies != numberOfCopies) {
            needUpdateGeos = 1;
        }

        individualAngle = totalAngle / numberOfCopiesToMake;

        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();


        restartCommand(QT_TRANSLATE_NOOP("Command", "Rotate"));
        //Creates geos
        std::stringstream stream;
        stream << "geoList = []\n";
        stream << "constrGeoList = []\n";
        for (size_t i = 1; i <= numberOfCopiesToMake; i++) {
            for (size_t j = 0; j < listOfGeoIds.size(); j++) {
                const Part::Geometry* geo = Obj->getGeometry(listOfGeoIds[j]);
                if (GeometryFacade::getConstruction(geo)) {
                    stream << "constrGeoList.";
                }
                else {
                    stream << "geoList.";
                }
                if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                    const Part::GeomCircle* circle = static_cast<const Part::GeomCircle*>(geo);
                    Base::Vector3d rotatedCenter = getRotatedPoint(circle->getCenter(), centerPoint, individualAngle * i);
                    stream << "append(Part.Circle(App.Vector(" << rotatedCenter.x << "," << rotatedCenter.y << ",0),App.Vector(0,0,1)," << circle->getRadius() << "))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.Circle(App.Vector(%f,%f,0),App.Vector(0,0,1),%f),%s)",
                        rotatedCenter.x, rotatedCenter.y, circle->getRadius(),
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                    const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
                    Base::Vector3d rotatedCenter = getRotatedPoint(arcOfCircle->getCenter(), centerPoint, individualAngle * i);
                    double arcStartAngle, arcEndAngle;
                    arcOfCircle->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                    stream << "append(Part.ArcOfCircle(Part.Circle(App.Vector(" << rotatedCenter.x << "," << rotatedCenter.y << ",0),App.Vector(0,0,1)," << arcOfCircle->getRadius() << "),"
                        << arcStartAngle + individualAngle * i << "," << arcEndAngle + individualAngle * i << "))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.ArcOfCircle(Part.Circle(App.Vector(%f,%f,0),App.Vector(0,0,1),%f),%f,%f),%s)",
                        rotatedCenter.x, rotatedCenter.y, arcOfCircle->getRadius(),
                        arcStartAngle + individualAngle * i, arcEndAngle + individualAngle * i,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                    const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
                    Base::Vector3d rotatedStartPoint = getRotatedPoint(line->getStartPoint(), centerPoint, individualAngle * i);
                    Base::Vector3d rotatedEndPoint = getRotatedPoint(line->getEndPoint(), centerPoint, individualAngle * i);
                    stream << "append(Part.LineSegment(App.Vector(" << rotatedStartPoint.x << "," << rotatedStartPoint.y << ",0),App.Vector(" << rotatedEndPoint.x << "," << rotatedEndPoint.y << ",0)))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.LineSegment(App.Vector(%f,%f,0),App.Vector(%f,%f,0)),%s)",
                        rotatedStartPoint.x, rotatedStartPoint.y, rotatedEndPoint.x, rotatedEndPoint.y,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                    const Part::GeomEllipse* ellipse = static_cast<const Part::GeomEllipse*>(geo);
                    Base::Vector3d rotatedCenterPoint = getRotatedPoint(ellipse->getCenter(), centerPoint, individualAngle * i);
                    Base::Vector3d ellipseAxis = ellipse->getMajorAxisDir();
                    Base::Vector3d periapsis = ellipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * ellipse->getMajorRadius();
                    periapsis = getRotatedPoint(periapsis, centerPoint, individualAngle * i);
                    Base::Vector3d ellipseMinorAxis;
                    ellipseMinorAxis.x = -ellipseAxis.y;
                    ellipseMinorAxis.y = ellipseAxis.x;
                    Base::Vector3d positiveB = ellipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * ellipse->getMinorRadius();
                    positiveB = getRotatedPoint(positiveB, centerPoint, individualAngle * i);
                    stream << "append(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y << ",0),App.Vector(" << rotatedCenterPoint.x << "," << rotatedCenterPoint.y << ",0)))\n";
                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.Ellipse(App.Vector(%f,%f,0),App.Vector(%f,%f,0),App.Vector(%f,%f,0)),%s)",
                        periapsis.x, periapsis.y,
                        positiveB.x, positiveB.y,
                        rotatedCenterPoint.x, rotatedCenterPoint.y,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                    const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
                    Base::Vector3d rotatedCenterPoint = getRotatedPoint(arcOfEllipse->getCenter(), centerPoint, individualAngle * i);
                    Base::Vector3d ellipseAxis = arcOfEllipse->getMajorAxisDir();
                    Base::Vector3d periapsis = arcOfEllipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * arcOfEllipse->getMajorRadius();
                    periapsis = getRotatedPoint(periapsis, centerPoint, individualAngle * i);
                    Base::Vector3d ellipseMinorAxis;
                    ellipseMinorAxis.x = -ellipseAxis.y;
                    ellipseMinorAxis.y = ellipseAxis.x;
                    Base::Vector3d positiveB = arcOfEllipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * arcOfEllipse->getMinorRadius();
                    positiveB = getRotatedPoint(positiveB, centerPoint, individualAngle * i);
                    double arcStartAngle, arcEndAngle;
                    arcOfEllipse->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                    stream << "append(Part.ArcOfEllipse(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y
                        << ",0),App.Vector(" << rotatedCenterPoint.x << "," << rotatedCenterPoint.y << ",0)),"
                        << arcStartAngle + individualAngle * i << "," << arcEndAngle + individualAngle * i << "))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.ArcOfEllipse"
                        "(Part.Ellipse(App.Vector(%f,%f,0),App.Vector(%f,%f,0),App.Vector(%f,%f,0)),%f,%f),%s)",
                        periapsis.x, periapsis.y,
                        positiveB.x, positiveB.y,
                        rotatedCenterPoint.x, rotatedCenterPoint.y,
                        arcStartAngle + individualAngle * i, arcEndAngle + individualAngle * i,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
                    const Part::GeomArcOfHyperbola* arcOfHyperbola = static_cast<const Part::GeomArcOfHyperbola*>(geo);
                    Base::Vector3d rotatedCenterPoint = getRotatedPoint(arcOfHyperbola->getCenter(), centerPoint, individualAngle * i);
                    Base::Vector3d ellipseAxis = arcOfHyperbola->getMajorAxisDir();
                    Base::Vector3d periapsis = arcOfHyperbola->getCenter() + (ellipseAxis / ellipseAxis.Length()) * arcOfHyperbola->getMajorRadius();
                    periapsis = getRotatedPoint(periapsis, centerPoint, individualAngle * i);
                    Base::Vector3d ellipseMinorAxis;
                    ellipseMinorAxis.x = -ellipseAxis.y;
                    ellipseMinorAxis.y = ellipseAxis.x;
                    Base::Vector3d positiveB = arcOfHyperbola->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * arcOfHyperbola->getMinorRadius();
                    positiveB = getRotatedPoint(positiveB, centerPoint, individualAngle * i);
                    double arcStartAngle, arcEndAngle;
                    arcOfHyperbola->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                    stream << "append(Part.ArcOfHyperbola(Part.Hyperbola(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y
                        << ",0),App.Vector(" << rotatedCenterPoint.x << "," << rotatedCenterPoint.y << ",0)),"
                        << arcStartAngle << "," << arcEndAngle << "))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.ArcOfHyperbola"
                        "(Part.Hyperbola(App.Vector(%f,%f,0),App.Vector(%f,%f,0),App.Vector(%f,%f,0)),%f,%f),%s)",
                        periapsis.x, periapsis.y,
                        positiveB.x, positiveB.y,
                        rotatedCenterPoint.x, rotatedCenterPoint.y,
                        arcStartAngle, arcEndAngle,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                    const Part::GeomArcOfParabola* arcOfParabola = static_cast<const Part::GeomArcOfParabola*>(geo);
                    Base::Vector3d rotatedFocusPoint = getRotatedPoint(arcOfParabola->getFocus(), centerPoint, individualAngle * i);
                    Base::Vector3d rotatedCenterPoint = getRotatedPoint(arcOfParabola->getCenter(), centerPoint, individualAngle * i);
                    double arcStartAngle, arcEndAngle;
                    arcOfParabola->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                    stream << "append(Part.ArcOfParabola(Part.Parabola(App.Vector(" << rotatedFocusPoint.x << "," << rotatedFocusPoint.y << ",0),App.Vector(" << rotatedCenterPoint.x << "," << rotatedCenterPoint.y
                        << ",0),App.Vector(0,0,1)),"
                        << arcStartAngle << "," << arcEndAngle << "))\n";

                    /*Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.ArcOfParabola"
                        "(Part.Parabola(App.Vector(%f,%f,0),App.Vector(%f,%f,0),App.Vector(0,0,1)),%f,%f),%s)",
                        rotatedFocusPoint.x, rotatedFocusPoint.y,
                        rotatedCenterPoint.x, rotatedCenterPoint.y,
                        arcStartAngle, arcEndAngle,
                        GeometryFacade::getConstruction(geo) ? "True" : "False");*/
                }
                else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
                    /*//try 1 : Doesn't work since I added these circles. But before it worked only with numberOfCopies = 0.
                    const Part::GeomBSplineCurve* bSpline = static_cast<const Part::GeomBSplineCurve*>(geo);
                    std::vector<Base::Vector3d> bSplineCtrlPoints = bSpline->getPoles();
                    std::vector<double> bSplineWeights = bSpline->getWeights();
                    std::stringstream stream;
                    int FirstPoleGeoId = getHighestCurveIndex()+1;
                    for (size_t k = 0; k < bSplineCtrlPoints.size(); k++) {
                        Base::Vector3d rotatedControlPoint = getRotatedPoint(bSplineCtrlPoints[k], centerPoint, individualAngle * i);
                        stream << "App.Vector(" << rotatedControlPoint.x << "," << rotatedControlPoint.y << "),";
                        //Add pole
                        Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.Circle(App.Vector(%f,%f,0),App.Vector(0,0,1),10),True)",
                            rotatedControlPoint.x, rotatedControlPoint.y);
                        Gui::cmdAppObjectArgs(sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Weight',%d,%f)) ",
                            getHighestCurveIndex(), bSplineWeights[k]);
                    }
                    std::string controlpoints = stream.str();
                    // remove last comma and add brackets
                    int index = controlpoints.rfind(',');
                    controlpoints.resize(index);
                    controlpoints.insert(0, 1, '[');
                    controlpoints.append(1, ']');
                    Base::Console().Warning("%s\n", controlpoints.c_str());
                    Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.BSplineCurve(%s,None,None,%s,3,None,False),%s)",
                        controlpoints.c_str(),
                        bSpline->isPeriodic() ? "True" : "False",
                        GeometryFacade::getConstruction(geo) ? "True" : "False");
                    Base::Console().Warning("hello 3\n");
                    // Constraint pole circles to B-spline.
                    std::stringstream cstream;
                    cstream << "conList = []\n";
                    for (size_t k = 0; k < bSplineCtrlPoints.size(); k++) {
                        Base::Console().Warning("hello 4\n");
                        cstream << "conList.append(Sketcher.Constraint('InternalAlignment:Sketcher::BSplineControlPoint'," << FirstPoleGeoId + k
                            << "," << static_cast<int>(Sketcher::PointPos::mid) << "," << getHighestCurveIndex() << "," << k << "))\n";
                    }
                    cstream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
                    cstream << "del conList\n";
                    Gui::Command::doCommand(Gui::Command::Doc, cstream.str().c_str());
                    // for showing the knots on creation
                    Gui::cmdAppObjectArgs(sketchgui->getObject(), "exposeInternalGeometry(%d)", getHighestCurveIndex());*/

                    /*//try 2 : Works with only numberOfCopies = 0...
                    Part::GeomBSplineCurve* geobsp = static_cast<Part::GeomBSplineCurve*>(geo->copy());

                    std::vector<Base::Vector3d> poles = geobsp->getPoles();

                    for (std::vector<Base::Vector3d>::iterator jt = poles.begin(); jt != poles.end(); ++jt) {

                        (*jt) = getRotatedPoint((*jt), centerPoint, individualAngle * i);
                    }

                    geobsp->setPoles(poles);
                    sketchgui->getSketchObject()->addGeometry(geobsp, GeometryFacade::getConstruction(geo));*/
                }
            }
        }
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(geoList,False)\n";
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(constrGeoList,True)\n";
        stream << "del geoList\n";
        stream << "del constrGeoList\n";
        Gui::Command::doCommand(Gui::Command::Doc, stream.str().c_str());
        /*if (needUpdateGeos || onReleaseButton) {
        * Idea is to move geos rather than delete and recreate to reduce crashes. But it's not sure it's better and it's not working.
            needUpdateGeos = 0;
            prevNumberOfCopies = numberOfCopies;
            sketchgui->draw(false, false); // Redraw
            sketchgui->getSketchObject()->solve(true);

            int lastCurve = getHighestCurveIndex();
            for (int i = firstCurveCreated; i <= lastCurve; i++) {
                const Part::Geometry* geo = Obj->getGeometry(i);
                if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                    Obj->initTemporaryMove(i, PointPos::mid, false);
                }
            }
        }
        else {
            rotateGeos();
            sketchgui->draw(false, false); // Redraw
        }*/

        //Create constraints
        if (onReleaseButton) {
            //stream << "conList = []\n"; //not sure this way would be better
            const std::vector< Sketcher::Constraint* >& vals = Obj->Constraints.getValues();
            std::vector< Constraint* > newconstrVals(vals);
            std::vector<int> geoIdsWhoAlreadyHasEqual = {}; //avoid applying equal several times if cloning distanceX and distanceY of the same part.

            std::vector< Sketcher::Constraint* >::const_iterator itEnd = vals.end(); //we need vals.end before adding any constraints
            for (std::vector< Sketcher::Constraint* >::const_iterator it = vals.begin(); it != itEnd; ++it) {
                int firstIndex = indexInVec(listOfGeoIds, (*it)->First);
                int secondIndex = indexInVec(listOfGeoIds, (*it)->Second);
                int thirdIndex = indexInVec(listOfGeoIds, (*it)->Third);

                if (((*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Perpendicular)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex >= 0) {
                    for (size_t i = 0; i < numberOfCopiesToMake; i++) {
                        Constraint* constNew = (*it)->copy();
                        constNew->First = firstCurveCreated + firstIndex + listOfGeoIds.size() * i;
                        constNew->Second = firstCurveCreated + secondIndex + listOfGeoIds.size() * i;
                        constNew->Third = firstCurveCreated + thirdIndex + listOfGeoIds.size() * i;
                        newconstrVals.push_back(constNew);
                    }
                }
                else if (((*it)->Type == Sketcher::Coincident
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Perpendicular
                    || (*it)->Type == Sketcher::Parallel
                    || (*it)->Type == Sketcher::Equal
                    || (*it)->Type == Sketcher::PointOnObject)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex == GeoEnum::GeoUndef) {
                    for (size_t i = 0; i < numberOfCopiesToMake; i++) {
                        Constraint* constNew = (*it)->copy();
                        constNew->First = firstCurveCreated + firstIndex + listOfGeoIds.size() * i;
                        constNew->Second = firstCurveCreated + secondIndex + listOfGeoIds.size() * i;
                        newconstrVals.push_back(constNew);
                    }
                }
                else if (((*it)->Type == Sketcher::Radius
                    || (*it)->Type == Sketcher::Diameter)
                    && firstIndex >= 0) {
                    for (size_t i = 0; i < numberOfCopiesToMake; i++) {
                        if (deleteOriginal || !toolSettings->widget->isCheckBoxChecked(2)) {
                            Constraint* constNew = (*it)->copy();
                            constNew->First = firstCurveCreated + firstIndex + listOfGeoIds.size() * i;
                            newconstrVals.push_back(constNew);
                        }
                        else {
                            Constraint* constNew = (*it)->copy();
                            constNew->Type = Sketcher::Equal;// first is already (*it)->First
                            constNew->isDriving = true;
                            constNew->Second = firstCurveCreated + firstIndex + listOfGeoIds.size() * i;
                            newconstrVals.push_back(constNew);
                        }
                    }
                }
                else if (((*it)->Type == Sketcher::Distance
                    || (*it)->Type == Sketcher::DistanceX
                    || (*it)->Type == Sketcher::DistanceY)
                    && firstIndex >= 0 && secondIndex >= 0) { //only line length because we can't apply equality between points.
                    for (size_t i = 0; i < numberOfCopiesToMake; i++) {
                        if ((deleteOriginal || !toolSettings->widget->isCheckBoxChecked(2)) && (*it)->Type == Sketcher::Distance) {
                            Constraint* constNew = (*it)->copy();
                            constNew->First = firstCurveCreated + firstIndex + listOfGeoIds.size() * i;
                            constNew->Second = firstCurveCreated + secondIndex + listOfGeoIds.size() * i;
                            newconstrVals.push_back(constNew);
                        }
                        else if ((*it)->First == (*it)->Second && indexInVec(geoIdsWhoAlreadyHasEqual, (*it)->First) != -1) {
                            Constraint* constNew = (*it)->copy();
                            constNew->Type = Sketcher::Equal;// first is already (*it)->First
                            constNew->isDriving = true;
                            constNew->Second = firstCurveCreated + secondIndex + listOfGeoIds.size() * i;
                            newconstrVals.push_back(constNew);
                        }
                    }
                    if (toolSettings->widget->isCheckBoxChecked(2) && (*it)->First == (*it)->Second) {
                        geoIdsWhoAlreadyHasEqual.push_back((*it)->First);
                    }
                }
            }
            if (newconstrVals.size() > vals.size())
                Obj->Constraints.setValues(std::move(newconstrVals));
            //stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
            //stream << "del conList\n";
        }


        if (deleteOriginal) {
            std::stringstream stream;
            for (size_t j = 0; j < listOfGeoIds.size() - 1; j++) {
                stream << listOfGeoIds[j] << ",";
            }
            stream << listOfGeoIds[listOfGeoIds.size() - 1];
            try {
                Gui::cmdAppObjectArgs(sketchgui->getObject(), "delGeometries([%s])", stream.str().c_str());
            }
            catch (const Base::Exception& e) {
                Base::Console().Error("%s\n", e.what());
            }
        }
    }

    void rotateGeos() {
        //currently painfully slow with movePoint() Also it crashes if we don't sketchgui->getSketchObject()->solve(true); in needUpdateGeos first .
        //also a problem to calculate individualAngle * i as i is not iterator to numberofcopies.
        int lastCurve = getHighestCurveIndex();
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        for (int i = firstCurveCreated; i <= lastCurve; i++) {
            const Part::Geometry* geo = Obj->getGeometry(i);
            if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {

                Base::Vector3d rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::mid), centerPoint, individualAngle * i);
                //Obj->movePoint(i, PointPos::mid, rotatedPoint);
                Obj->moveTemporaryPoint(i, PointPos::mid, rotatedPoint, false);

                //Part::GeomCircle* circle = static_cast<Part::GeomCircle*>(geo);
                //circle->setCenter(getRotatedPoint(circle->getCenter(), centerPoint, individualAngle * i));
            }
            else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                Base::Vector3d rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::mid), centerPoint, individualAngle * i);
                Obj->movePoint(i, PointPos::mid, rotatedPoint);
                rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::start), centerPoint, individualAngle * i);
                Obj->movePoint(i, PointPos::start, rotatedPoint);
                rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::end), centerPoint, individualAngle * i);
                Obj->movePoint(i, PointPos::end, rotatedPoint);
                /*Part::GeomArcOfCircle* arcOfCircle = static_cast<Part::GeomArcOfCircle*>(geo);
                arcOfCircle->setCenter(getRotatedPoint(arcOfCircle->getCenter(), centerPoint, individualAngle * i));
                double arcStartAngle, arcEndAngle;
                //arcOfCircle->getRange(arcStartAngle, arcEndAngle, true);
                //arcOfCircle->setRange(arcStartAngle + individualAngle * i, arcEndAngle + individualAngle * i, true);*/
            }
            else if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                Base::Vector3d rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::start), centerPoint, individualAngle * i);
                Obj->movePoint(i, PointPos::start, rotatedPoint);
                rotatedPoint = getRotatedPoint(Obj->getPoint(i, PointPos::end), centerPoint, individualAngle * i);
                Obj->movePoint(i, PointPos::end, rotatedPoint);
                /*Part::GeomLineSegment* line = static_cast<Part::GeomLineSegment*>(geo);
                Base::Vector3d rotatedStartPoint = getRotatedPoint(line->getStartPoint(), centerPoint, individualAngle * i);
                Base::Vector3d rotatedEndPoint = getRotatedPoint(line->getEndPoint(), centerPoint, individualAngle * i);
                line->setPoints(rotatedStartPoint, rotatedEndPoint);*/
            }
            else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                /*Part::GeomEllipse* ellipse = static_cast<Part::GeomEllipse*>(geo);
                ellipse->setCenter(getRotatedPoint(ellipse->getCenter(), centerPoint, individualAngle * i));
                ellipse->setMajorAxisDir(getRotatedPoint(ellipse->getMajorAxisDir(), centerPoint, individualAngle * i));*/
            }
            else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                /*Part::GeomArcOfEllipse* arcOfEllipse = static_cast<Part::GeomArcOfEllipse*>(geo);
                arcOfEllipse->setCenter(getRotatedPoint(arcOfEllipse->getCenter(), centerPoint, individualAngle * i));
                arcOfEllipse->setMajorAxisDir(getRotatedPoint(arcOfEllipse->getMajorAxisDir(), centerPoint, individualAngle * i));
                double arcStartAngle, arcEndAngle;
                arcOfEllipse->getRange(arcStartAngle, arcEndAngle, true);
                //arcOfEllipse->setRange(arcStartAngle + individualAngle * i, arcEndAngle + individualAngle * i, true);*/

            }
            else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
                /*Part::GeomArcOfHyperbola* arcOfHyperbola = static_cast<Part::GeomArcOfHyperbola*>(geo);
                arcOfHyperbola->setCenter(getRotatedPoint(arcOfHyperbola->getCenter(), centerPoint, individualAngle * i));
                arcOfHyperbola->setMajorAxisDir(getRotatedPoint(arcOfHyperbola->getMajorAxisDir(), centerPoint, individualAngle * i));*/
            }
            else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                /*Part::GeomArcOfParabola* arcOfParabola = static_cast<Part::GeomArcOfParabola*>(geo);
                arcOfParabola->setCenter(getRotatedPoint(arcOfParabola->getCenter(), centerPoint, individualAngle * i));
                //arcOfParabola->setFocus(getRotatedPoint(arcOfParabola->getFocus(), centerPoint, individualAngle * i));*/
            }
            else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
            }
            Base::Console().Warning("hello 6\n");
        }
        Base::Console().Warning("hello 7\n");
    }

    bool getSnapPoint(Base::Vector2d& snapPoint) {
        int pointGeoId = GeoEnum::GeoUndef;
        Sketcher::PointPos pointPosId = Sketcher::PointPos::none;
        int VtId = getPreselectPoint();
        int CrsId = getPreselectCross();
        if (CrsId == 0) {
            pointGeoId = Sketcher::GeoEnum::RtPnt;
            pointPosId = Sketcher::PointPos::start;
        }
        else if (VtId >= 0) {
            sketchgui->getSketchObject()->getGeoVertexIndex(VtId, pointGeoId, pointPosId);
        }
        if (pointGeoId != GeoEnum::GeoUndef && pointGeoId < firstCurveCreated) { 
            //don't want to snap to the point of a geometry which is being previewed!
            auto sk = static_cast<Sketcher::SketchObject*>(sketchgui->getObject());
            snapPoint.x = sk->getPoint(pointGeoId, pointPosId).x;
            snapPoint.y = sk->getPoint(pointGeoId, pointPosId).y;
            return true;
        }
        return false;
    }

    int indexInVec(std::vector<int> vec, int elem)
    {
        if (elem == GeoEnum::GeoUndef){
            return GeoEnum::GeoUndef;
        }
        for (size_t i = 0; i < vec.size(); i++)
        {
            if (vec[i] == elem)
            {
                return i;
            }
        }
        return -1;
    }

    Base::Vector3d getRotatedPoint(Base::Vector3d pointToRotate, Base::Vector2d centerPoint, double angle) {
        Base::Vector2d pointToRotate2D;
        pointToRotate2D.x = pointToRotate.x;
        pointToRotate2D.y = pointToRotate.y;

        double initialAngle = (pointToRotate2D - centerPoint).Angle();
        double lengthToCenter = (pointToRotate2D - centerPoint).Length();

        pointToRotate2D = centerPoint + lengthToCenter * Base::Vector2d(cos(angle + initialAngle), sin(angle + initialAngle));


        pointToRotate.x = pointToRotate2D.x;
        pointToRotate.y = pointToRotate2D.y;

        return pointToRotate;
    }

    void restartCommand(const char* cstrName) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        Gui::Command::abortCommand();
        Obj->solve(true);
        sketchgui->draw(false, false); // Redraw
        Gui::Command::openCommand(cstrName);
    }
};

DEF_STD_CMD_A(CmdSketcherRotate)

CmdSketcherRotate::CmdSketcherRotate()
    : Command("Sketcher_Rotate")
{
    sAppModule = "Sketcher";
    sGroup = "Sketcher";
    sMenuText = QT_TR_NOOP("Rotate geometries");
    sToolTipText = QT_TR_NOOP("Rotate selected geometries n times, enable creation of circular patterns.");
    sWhatsThis = "Sketcher_Rotate";
    sStatusTip = sToolTipText;
    sPixmap = "Sketcher_Rotate";
    sAccel = "B";
    eType = ForEdit;
}

void CmdSketcherRotate::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    std::vector<int> listOfGeoIds = {};

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
            QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string>& SubNames = selection[0].getSubNames();
    if (!SubNames.empty()) {
        Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

        for (std::vector<std::string>::const_iterator it = SubNames.begin(); it != SubNames.end(); ++it) {
            // only handle non-external edges
            if (it->size() > 4 && it->substr(0, 4) == "Edge") {
                int geoId = std::atoi(it->substr(4, 4000).c_str()) - 1;
                if (geoId >= 0) {
                    listOfGeoIds.push_back(geoId);
                }
            }
            else if (it->size() > 6 && it->substr(0, 6) == "Vertex") {
                // only if it is a GeomPoint
                int VtId = std::atoi(it->substr(6, 4000).c_str()) - 1;
                int geoId;
                Sketcher::PointPos PosId;
                Obj->getGeoVertexIndex(VtId, geoId, PosId);
                if (Obj->getGeometry(geoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                    if (geoId >= 0) {
                        listOfGeoIds.push_back(geoId);
                    }
                }
            }
        }
    }

    getSelection().clearSelection();

    ActivateAcceleratorHandler(getActiveGuiDocument(), new DrawSketchHandlerRotate(listOfGeoIds));
}

bool CmdSketcherRotate::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// Scale tool =====================================================================

class DrawSketchHandlerScale : public DrawSketchHandler
{
public:
    DrawSketchHandlerScale(std::vector<int> listOfGeoIds)
        : Mode(STATUS_SEEK_First)
        , EditCurve(2)
        , numberOfCopies(0)
        , deleteOriginal(0)
        , needUpdateGeos(1)
        , snapMode(SnapMode::Free)
        , listOfGeoIds(listOfGeoIds) {}
    virtual ~DrawSketchHandlerScale() {}

    enum SelectMode {
        STATUS_SEEK_First,
        STATUS_SEEK_Second,
        STATUS_End
    };

    enum class SnapMode {
        Free,
        Snap
    };

    virtual void activated(ViewProviderSketch*)
    {
        toolSettings->widget->setSettings(18);
        Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Scale"));
        firstCurveCreated = getHighestCurveIndex() + 1;


        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/General");
        previewEnabled = hGrp->GetBool("ScaleEnablePreview", true);

        // Constrain icon size in px
        qreal pixelRatio = devicePixelRatio();
        const unsigned long defaultCrosshairColor = 0xFFFFFF;
        unsigned long color = getCrosshairColor();
        auto colorMapping = std::map<unsigned long, unsigned long>();
        colorMapping[defaultCrosshairColor] = color;

        qreal fullIconWidth = 32 * pixelRatio;
        qreal iconWidth = 16 * pixelRatio;
        QPixmap cursorPixmap = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Crosshair", QSizeF(fullIconWidth, fullIconWidth), colorMapping),
            icon = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Scale", QSizeF(iconWidth, iconWidth));
        QPainter cursorPainter;
        cursorPainter.begin(&cursorPixmap);
        cursorPainter.drawPixmap(16 * pixelRatio, 16 * pixelRatio, icon);
        cursorPainter.end();
        int hotX = 8;
        int hotY = 8;
        cursorPixmap.setDevicePixelRatio(pixelRatio);
        // only X11 needs hot point coordinates to be scaled
        if (qGuiApp->platformName() == QLatin1String("xcb")) {
            hotX *= pixelRatio;
            hotY *= pixelRatio;
        }
        setCursor(cursorPixmap, hotX, hotY, false);
    }
    virtual void deactivated(ViewProviderSketch*)
    {
        //delete created constrains if the tool is exited before validating by left clicking somewhere
        Gui::Command::abortCommand();
        sketchgui->getSketchObject()->solve(true);
        sketchgui->draw(false, false); // Redraw
    }

    virtual void mouseMove(Base::Vector2d onSketchPos)
    {
        if (QApplication::keyboardModifiers() == Qt::ControlModifier)
            snapMode = SnapMode::Snap;
        else
            snapMode = SnapMode::Free;

        if (Mode == STATUS_SEEK_First) {
            setPositionText(onSketchPos);
            if (snapMode == SnapMode::Snap && getSnapPoint(referencePoint)) {
                setPositionText(referencePoint);
            }

            if (toolSettings->widget->isSettingSet[0] && toolSettings->widget->isSettingSet[1]) {
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
            }
        }
        else if (Mode == STATUS_SEEK_Second) {
            toolSettings->widget->setLabel(QApplication::translate("Scale_2", "Select a point where distance from this point to reference point represent the scale factor."), 6);
            length = (onSketchPos - referencePoint).Length();

            Base::Vector2d endpoint = onSketchPos;

            if (snapMode == SnapMode::Snap) {
                if (getSnapPoint(endpoint)) {
                    length = (endpoint - referencePoint).Length();
                }
            }

            if (toolSettings->widget->isSettingSet[2]) {
                scaleFactor = toolSettings->widget->toolParameters[2];
            }
            else {
                scaleFactor = length / sketchgui->GridSize.getValue();
            }

            SbString text;
            text.sprintf(" (%.1f)", length);
            setPositionText(endpoint, text);

            //generate the copies
            if (previewEnabled) {
                generateScaledGeos(0);
                sketchgui->draw(false, false); // Redraw
            }

            EditCurve[0] = endpoint;
            EditCurve[1] = referencePoint;
            drawEdit(EditCurve);

            if (toolSettings->widget->isSettingSet[2]) {
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
            }
        }
        applyCursor();
    }

    virtual bool pressButton(Base::Vector2d onSketchPos)
    {
        if (Mode == STATUS_SEEK_First) {
            if (!(snapMode == SnapMode::Snap && getSnapPoint(referencePoint))) {
                //note: if getSnapPoint returns true, then centerPoint is modified already
                referencePoint = onSketchPos;
                if (toolSettings->widget->isSettingSet[0]) {
                    referencePoint.x = toolSettings->widget->toolParameters[0];
                }
                if (toolSettings->widget->isSettingSet[1]) {
                    referencePoint.y = toolSettings->widget->toolParameters[1];
                }
            }

            EditCurve[0] = referencePoint;
            toolSettings->widget->setParameterActive(0, 0);
            toolSettings->widget->setParameterActive(0, 1);
            toolSettings->widget->setParameterActive(1, 2);
            toolSettings->widget->setParameterActive(1, 3);
            toolSettings->widget->setParameterFocus(2);
            Mode = STATUS_SEEK_Second;
        }
        else if (Mode == STATUS_SEEK_Second) {
            Mode = STATUS_End;
        }
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos)
    {
        Q_UNUSED(onSketchPos);
        if (Mode == STATUS_End) {
            generateScaledGeos(1);

            Gui::Command::commitCommand();

            EditCurve.clear();
            drawEdit(EditCurve);

            sketchgui->getSketchObject()->solve(true);
            sketchgui->draw(false, false); // Redraw

            sketchgui->purgeHandler();
        }
        return true;
    }
protected:
    SelectMode Mode;
    SnapMode snapMode;
    std::vector<int> listOfGeoIds;
    Base::Vector2d referencePoint;
    std::vector<Base::Vector2d> EditCurve;

    bool deleteOriginal, previewEnabled, needUpdateGeos;
    double length, scaleFactor;
    int numberOfCopies, prevNumberOfCopies, firstCurveCreated;

    void generateScaledGeos(bool onReleaseButton) {
        if (toolSettings->widget->isCheckBoxChecked(2)) {
            deleteOriginal = 0;
            numberOfCopies = 2;
        }
        else {
            deleteOriginal = 1;
            numberOfCopies = 1;
        }

        if (prevNumberOfCopies != numberOfCopies) {
            needUpdateGeos = 1;
            prevNumberOfCopies = numberOfCopies;
        }

        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();

        restartCommand(QT_TRANSLATE_NOOP("Command", "Scale"));
        //Creates geos
        std::stringstream stream;
        stream << "geoList = []\n";
        stream << "constrGeoList = []\n";
        for (size_t j = 0; j < listOfGeoIds.size(); j++) {
            const Part::Geometry* geo = Obj->getGeometry(listOfGeoIds[j]);
            if (GeometryFacade::getConstruction(geo)) {
                stream << "constrGeoList.";
            }
            else {
                stream << "geoList.";
            }
            if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                const Part::GeomCircle* circle = static_cast<const Part::GeomCircle*>(geo);
                Base::Vector3d scaledCenter = getScaledPoint(circle->getCenter(), referencePoint, scaleFactor);
                stream << "append(Part.Circle(App.Vector(" << scaledCenter.x << "," << scaledCenter.y << ",0),App.Vector(0,0,1)," << circle->getRadius() * scaleFactor << "))\n";
            }
            else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
                Base::Vector3d scaledCenter = getScaledPoint(arcOfCircle->getCenter(), referencePoint, scaleFactor);
                double arcStartAngle, arcEndAngle;
                arcOfCircle->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                stream << "append(Part.ArcOfCircle(Part.Circle(App.Vector(" << scaledCenter.x << "," << scaledCenter.y << ",0),App.Vector(0,0,1)," << arcOfCircle->getRadius() * scaleFactor << "),"
                    << arcStartAngle << "," << arcEndAngle << "))\n";
            }
            else if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
                Base::Vector3d scaledStartPoint = getScaledPoint(line->getStartPoint(), referencePoint, scaleFactor);
                Base::Vector3d scaledEndPoint = getScaledPoint(line->getEndPoint(), referencePoint, scaleFactor);
                stream << "append(Part.LineSegment(App.Vector(" << scaledStartPoint.x << "," << scaledStartPoint.y << ",0),App.Vector(" << scaledEndPoint.x << "," << scaledEndPoint.y << ",0)))\n";
            }
            else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                const Part::GeomEllipse* ellipse = static_cast<const Part::GeomEllipse*>(geo);
                Base::Vector3d scaledCenterPoint = getScaledPoint(ellipse->getCenter(), referencePoint, scaleFactor);
                Base::Vector3d ellipseAxis = ellipse->getMajorAxisDir();
                Base::Vector3d periapsis = ellipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * ellipse->getMajorRadius();
                periapsis = getScaledPoint(periapsis, referencePoint, scaleFactor);
                Base::Vector3d ellipseMinorAxis;
                ellipseMinorAxis.x = -ellipseAxis.y;
                ellipseMinorAxis.y = ellipseAxis.x;
                Base::Vector3d positiveB = ellipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * ellipse->getMinorRadius();
                positiveB = getScaledPoint(positiveB, referencePoint, scaleFactor);
                stream << "append(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y << ",0),App.Vector(" << scaledCenterPoint.x << "," << scaledCenterPoint.y << ",0)))\n";
            }
            else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
                Base::Vector3d scaledCenterPoint = getScaledPoint(arcOfEllipse->getCenter(), referencePoint, scaleFactor);
                Base::Vector3d ellipseAxis = arcOfEllipse->getMajorAxisDir();
                Base::Vector3d periapsis = arcOfEllipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * arcOfEllipse->getMajorRadius();
                periapsis = getScaledPoint(periapsis, referencePoint, scaleFactor);
                Base::Vector3d ellipseMinorAxis;
                ellipseMinorAxis.x = -ellipseAxis.y;
                ellipseMinorAxis.y = ellipseAxis.x;
                Base::Vector3d positiveB = arcOfEllipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * arcOfEllipse->getMinorRadius();
                positiveB = getScaledPoint(positiveB, referencePoint, scaleFactor);
                double arcStartAngle, arcEndAngle;
                arcOfEllipse->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                stream << "append(Part.ArcOfEllipse(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y
                    << ",0),App.Vector(" << scaledCenterPoint.x << "," << scaledCenterPoint.y << ",0)),"
                    << arcStartAngle  << "," << arcEndAngle << "))\n";
            }
            else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
                const Part::GeomArcOfHyperbola* arcOfHyperbola = static_cast<const Part::GeomArcOfHyperbola*>(geo);
                Base::Vector3d scaledCenterPoint = getScaledPoint(arcOfHyperbola->getCenter(), referencePoint, scaleFactor);
                Base::Vector3d ellipseAxis = arcOfHyperbola->getMajorAxisDir();
                Base::Vector3d periapsis = arcOfHyperbola->getCenter() + (ellipseAxis / ellipseAxis.Length()) * arcOfHyperbola->getMajorRadius();
                periapsis = getScaledPoint(periapsis, referencePoint, scaleFactor);
                Base::Vector3d ellipseMinorAxis;
                ellipseMinorAxis.x = -ellipseAxis.y;
                ellipseMinorAxis.y = ellipseAxis.x;
                Base::Vector3d positiveB = arcOfHyperbola->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * arcOfHyperbola->getMinorRadius();
                positiveB = getScaledPoint(positiveB, referencePoint, scaleFactor);
                double arcStartAngle, arcEndAngle;
                arcOfHyperbola->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                stream << "append(Part.ArcOfHyperbola(Part.Hyperbola(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y
                    << ",0),App.Vector(" << scaledCenterPoint.x << "," << scaledCenterPoint.y << ",0)),"
                    << arcStartAngle << "," << arcEndAngle << "))\n";
            }
            else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                const Part::GeomArcOfParabola* arcOfParabola = static_cast<const Part::GeomArcOfParabola*>(geo);
                Base::Vector3d scaledFocusPoint = getScaledPoint(arcOfParabola->getFocus(), referencePoint, scaleFactor);
                Base::Vector3d scaledCenterPoint = getScaledPoint(arcOfParabola->getCenter(), referencePoint, scaleFactor);
                double arcStartAngle, arcEndAngle;
                arcOfParabola->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                stream << "append(Part.ArcOfParabola(Part.Parabola(App.Vector(" << scaledFocusPoint.x << "," << scaledFocusPoint.y << ",0),App.Vector(" << scaledCenterPoint.x << "," << scaledCenterPoint.y
                    << ",0),App.Vector(0,0,1)),"
                    << arcStartAngle << "," << arcEndAngle << "))\n";
            }
            else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
                /*//try 1 : Doesn't work since I added these circles. But before it worked only with numberOfCopies = 0.
                const Part::GeomBSplineCurve* bSpline = static_cast<const Part::GeomBSplineCurve*>(geo);
                std::vector<Base::Vector3d> bSplineCtrlPoints = bSpline->getPoles();
                std::vector<double> bSplineWeights = bSpline->getWeights();
                std::stringstream stream;
                int FirstPoleGeoId = getHighestCurveIndex()+1;
                for (size_t k = 0; k < bSplineCtrlPoints.size(); k++) {
                    Base::Vector3d scaledControlPoint = getScaledPoint(bSplineCtrlPoints[k], centerPoint, individualAngle * i);
                    stream << "App.Vector(" << scaledControlPoint.x << "," << scaledControlPoint.y << "),";
                    //Add pole
                    Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.Circle(App.Vector(%f,%f,0),App.Vector(0,0,1),10),True)",
                        scaledControlPoint.x, scaledControlPoint.y);
                    Gui::cmdAppObjectArgs(sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Weight',%d,%f)) ",
                        getHighestCurveIndex(), bSplineWeights[k]);
                }
                std::string controlpoints = stream.str();
                // remove last comma and add brackets
                int index = controlpoints.rfind(',');
                controlpoints.resize(index);
                controlpoints.insert(0, 1, '[');
                controlpoints.append(1, ']');
                Base::Console().Warning("%s\n", controlpoints.c_str());
                Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.BSplineCurve(%s,None,None,%s,3,None,False),%s)",
                    controlpoints.c_str(),
                    bSpline->isPeriodic() ? "True" : "False",
                    GeometryFacade::getConstruction(geo) ? "True" : "False");
                Base::Console().Warning("hello 3\n");
                // Constraint pole circles to B-spline.
                std::stringstream cstream;
                cstream << "conList = []\n";
                for (size_t k = 0; k < bSplineCtrlPoints.size(); k++) {
                    Base::Console().Warning("hello 4\n");
                    cstream << "conList.append(Sketcher.Constraint('InternalAlignment:Sketcher::BSplineControlPoint'," << FirstPoleGeoId + k
                        << "," << static_cast<int>(Sketcher::PointPos::mid) << "," << getHighestCurveIndex() << "," << k << "))\n";
                }
                cstream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
                cstream << "del conList\n";
                Gui::Command::doCommand(Gui::Command::Doc, cstream.str().c_str());
                // for showing the knots on creation
                Gui::cmdAppObjectArgs(sketchgui->getObject(), "exposeInternalGeometry(%d)", getHighestCurveIndex());*/

                /*//try 2 : Works with only numberOfCopies = 0...
                Part::GeomBSplineCurve* geobsp = static_cast<Part::GeomBSplineCurve*>(geo->copy());

                std::vector<Base::Vector3d> poles = geobsp->getPoles();

                for (std::vector<Base::Vector3d>::iterator jt = poles.begin(); jt != poles.end(); ++jt) {

                    (*jt) = getScaledPoint((*jt), centerPoint, individualAngle * i);
                }

                geobsp->setPoles(poles);
                sketchgui->getSketchObject()->addGeometry(geobsp, GeometryFacade::getConstruction(geo));*/
            }
        }
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(geoList,False)\n";
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(constrGeoList,True)\n";
        stream << "del geoList\n";
        stream << "del constrGeoList\n";
        Gui::Command::doCommand(Gui::Command::Doc, stream.str().c_str());
        /*if (needUpdateGeos || onReleaseButton) {
        * Idea is to move geos rather than delete and recreate to reduce crashes. But it's not sure it's better and it's not working.
            needUpdateGeos = 0;
            prevNumberOfCopies = numberOfCopies;
            sketchgui->draw(false, false); // Redraw
            sketchgui->getSketchObject()->solve(true);

            int lastCurve = getHighestCurveIndex();
            for (int i = firstCurveCreated; i <= lastCurve; i++) {
                const Part::Geometry* geo = Obj->getGeometry(i);
                if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                    Obj->initTemporaryMove(i, PointPos::mid, false);
                }
            }
        }
        else {
            scaleGeos();
            sketchgui->draw(false, false); // Redraw
        }*/

        //Create constraints
        if (onReleaseButton) {
            //stream << "conList = []\n"; //not sure this way would be better
            const std::vector< Sketcher::Constraint* >& vals = Obj->Constraints.getValues();
            std::vector< Constraint* > newconstrVals(vals);
            std::vector<int> geoIdsWhoAlreadyHasEqual = {}; //avoid applying equal several times if cloning distanceX and distanceY of the same part.

            std::vector< Sketcher::Constraint* >::const_iterator itEnd = vals.end(); //we need vals.end before adding any constraints
            for (std::vector< Sketcher::Constraint* >::const_iterator it = vals.begin(); it != itEnd; ++it) {
                int firstIndex = indexInVec(listOfGeoIds, (*it)->First);
                int secondIndex = indexInVec(listOfGeoIds, (*it)->Second);
                int thirdIndex = indexInVec(listOfGeoIds, (*it)->Third);

                if (((*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Perpendicular)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    constNew->Third = firstCurveCreated + thirdIndex;
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Coincident
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Perpendicular
                    || (*it)->Type == Sketcher::Parallel
                    || (*it)->Type == Sketcher::Equal
                    || (*it)->Type == Sketcher::PointOnObject)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex == GeoEnum::GeoUndef) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Radius
                    || (*it)->Type == Sketcher::Diameter)
                    && firstIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->setValue(constNew->getValue() * scaleFactor);
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Distance
                    || (*it)->Type == Sketcher::DistanceX
                    || (*it)->Type == Sketcher::DistanceY)
                    && firstIndex >= 0 && secondIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    constNew->setValue(constNew->getValue()* scaleFactor);
                    newconstrVals.push_back(constNew);
                }
            }
            if (newconstrVals.size() > vals.size())
                Obj->Constraints.setValues(std::move(newconstrVals));
            //stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
            //stream << "del conList\n";
        }

        if (deleteOriginal) {
            std::stringstream stream;
            for (size_t j = 0; j < listOfGeoIds.size() - 1; j++) {
                stream << listOfGeoIds[j] << ",";
            }
            stream << listOfGeoIds[listOfGeoIds.size() - 1];
            try {
                Gui::cmdAppObjectArgs(sketchgui->getObject(), "delGeometries([%s])", stream.str().c_str());
            }
            catch (const Base::Exception& e) {
                Base::Console().Error("%s\n", e.what());
            }
        }
    }

    bool getSnapPoint(Base::Vector2d& snapPoint) {
        int pointGeoId = GeoEnum::GeoUndef;
        Sketcher::PointPos pointPosId = Sketcher::PointPos::none;
        int VtId = getPreselectPoint();
        int CrsId = getPreselectCross();
        if (CrsId == 0) {
            pointGeoId = Sketcher::GeoEnum::RtPnt;
            pointPosId = Sketcher::PointPos::start;
        }
        else if (VtId >= 0) {
            sketchgui->getSketchObject()->getGeoVertexIndex(VtId, pointGeoId, pointPosId);
        }
        if (pointGeoId != GeoEnum::GeoUndef && pointGeoId < firstCurveCreated) {
            //don't want to snap to the point of a geometry which is being previewed!
            auto sk = static_cast<Sketcher::SketchObject*>(sketchgui->getObject());
            snapPoint.x = sk->getPoint(pointGeoId, pointPosId).x;
            snapPoint.y = sk->getPoint(pointGeoId, pointPosId).y;
            return true;
        }
        return false;
    }

    int indexInVec(std::vector<int> vec, int elem)
    {
        if (elem == GeoEnum::GeoUndef) {
            return GeoEnum::GeoUndef;
        }
        for (size_t i = 0; i < vec.size(); i++)
        {
            if (vec[i] == elem)
            {
                return i;
            }
        }
        return -1;
    }

    Base::Vector3d getScaledPoint(Base::Vector3d pointToScale, Base::Vector2d referencePoint, double scaleFactor) {
        Base::Vector2d pointToScale2D;
        pointToScale2D.x = pointToScale.x;
        pointToScale2D.y = pointToScale.y;
        pointToScale2D = (pointToScale2D - referencePoint) * scaleFactor + referencePoint;

        pointToScale.x = pointToScale2D.x;
        pointToScale.y = pointToScale2D.y;

        return pointToScale;
    }

    void restartCommand(const char* cstrName) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        Gui::Command::abortCommand();
        Obj->solve(true);
        sketchgui->draw(false, false); // Redraw
        Gui::Command::openCommand(cstrName);
    }
};

DEF_STD_CMD_A(CmdSketcherScale)

CmdSketcherScale::CmdSketcherScale()
    : Command("Sketcher_Scale")
{
    sAppModule = "Sketcher";
    sGroup = "Sketcher";
    sMenuText = QT_TR_NOOP("Scale geometries");
    sToolTipText = QT_TR_NOOP("Scale selected geometries.");
    sWhatsThis = "Sketcher_Scale";
    sStatusTip = sToolTipText;
    sPixmap = "Sketcher_Scale";
    sAccel = "S";
    eType = ForEdit;
}

void CmdSketcherScale::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    std::vector<int> listOfGeoIds = {};

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
            QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string>& SubNames = selection[0].getSubNames();
    if (!SubNames.empty()) {
        Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

        for (std::vector<std::string>::const_iterator it = SubNames.begin(); it != SubNames.end(); ++it) {
            // only handle non-external edges
            if (it->size() > 4 && it->substr(0, 4) == "Edge") {
                int geoId = std::atoi(it->substr(4, 4000).c_str()) - 1;
                if (geoId >= 0) {
                    listOfGeoIds.push_back(geoId);
                }
            }
            else if (it->size() > 6 && it->substr(0, 6) == "Vertex") {
                // only if it is a GeomPoint
                int VtId = std::atoi(it->substr(6, 4000).c_str()) - 1;
                int geoId;
                Sketcher::PointPos PosId;
                Obj->getGeoVertexIndex(VtId, geoId, PosId);
                if (Obj->getGeometry(geoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                    if (geoId >= 0) {
                        listOfGeoIds.push_back(geoId);
                    }
                }
            }
        }
    }

    getSelection().clearSelection();

    ActivateAcceleratorHandler(getActiveGuiDocument(), new DrawSketchHandlerScale(listOfGeoIds));
}

bool CmdSketcherScale::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// Offset tool =====================================================================

class DrawSketchHandlerOffset : public DrawSketchHandler
{
public:
    DrawSketchHandlerOffset(std::vector<int> listOfGeoIds)
        : Mode(STATUS_SEEK_First)
        , joinMode(JoinMode::Arc)
        , deleteOriginal(0)
        , offsetLength(1)
        , prevOffsetLength(1)
        , offsetCurveUsed(0)
        , offsetDirection(1)
        , continuousCurveOfCurvedUsed(0)
        , snapMode(SnapMode::Free)
        , listOfGeoIds(listOfGeoIds) {}
    virtual ~DrawSketchHandlerOffset() {}

    enum SelectMode {
        STATUS_SEEK_First,
        STATUS_End
    }; 

    enum class SnapMode {
        Free,
        Snap
    };

    enum class JoinMode {
        Arc,
        Tangent,
        Intersection
    };

    enum class ModeEnums {
        Skin,
        Pipe,
        RectoVerso
    };

    virtual void activated(ViewProviderSketch*)
    {
        toolSettings->widget->setSettings(19);
        Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Offset"));
        firstCurveCreated = getHighestCurveIndex() + 1;

        generatevCC();
        generateSourceWires();

        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher/General");
        previewEnabled = hGrp->GetBool("OffsetEnablePreview", true);

        // Constrain icon size in px
        qreal pixelRatio = devicePixelRatio();
        const unsigned long defaultCrosshairColor = 0xFFFFFF;
        unsigned long color = getCrosshairColor();
        auto colorMapping = std::map<unsigned long, unsigned long>();
        colorMapping[defaultCrosshairColor] = color;

        qreal fullIconWidth = 32 * pixelRatio;
        qreal iconWidth = 16 * pixelRatio;
        QPixmap cursorPixmap = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Crosshair", QSizeF(fullIconWidth, fullIconWidth), colorMapping),
            icon = Gui::BitmapFactory().pixmapFromSvg("Sketcher_Offset", QSizeF(iconWidth, iconWidth));
        QPainter cursorPainter;
        cursorPainter.begin(&cursorPixmap);
        cursorPainter.drawPixmap(16 * pixelRatio, 16 * pixelRatio, icon);
        cursorPainter.end();
        int hotX = 8;
        int hotY = 8;
        cursorPixmap.setDevicePixelRatio(pixelRatio);
        // only X11 needs hot point coordinates to be offsetd
        if (qGuiApp->platformName() == QLatin1String("xcb")) {
            hotX *= pixelRatio;
            hotY *= pixelRatio;
        }
        setCursor(cursorPixmap, hotX, hotY, false);
    }
    virtual void deactivated(ViewProviderSketch*)
    {
        Gui::Command::abortCommand();
        sketchgui->getSketchObject()->solve(true);
        sketchgui->draw(false, false); // Redraw
    }

    virtual void mouseMove(Base::Vector2d onSketchPos)
    {
        if (QApplication::keyboardModifiers() == Qt::ControlModifier)
            snapMode = SnapMode::Snap;
        else
            snapMode = SnapMode::Free;

        if (Mode == STATUS_SEEK_First) {
            endpoint = onSketchPos;
            if (snapMode == SnapMode::Snap) {
                getSnapPoint(endpoint);
            }

            if (toolSettings->widget->isSettingSet[0]) {
                offsetLength = toolSettings->widget->toolParameters[0];
                pressButton(onSketchPos);
                releaseButton(onSketchPos);
                return;
            }
            else {
                findOffsetLength();
            }

            if (toolSettings->widget->isCheckBoxChecked(3)) {
                Base::Console().Warning("arc join \n");
                joinMode = JoinMode::Arc;
            }
            else
                joinMode = JoinMode::Intersection;

            SbString text;
            text.sprintf(" (%.1f)", offsetLength);
            setPositionText(endpoint, text);

            //generate the copies
            if (previewEnabled && offsetLength != 0 && offsetLength != prevOffsetLength) {
                prevOffsetLength = offsetLength;
                findOffsetDirections();
                makeOffset(static_cast<int>(joinMode), false, false);
                sketchgui->draw(false, false); // Redraw
                //Base::Console().Warning("Hello7\n");
            }
        }
        applyCursor();
    }

    virtual bool pressButton(Base::Vector2d onSketchPos)
    {
        if (Mode == STATUS_SEEK_First) {
            Mode = STATUS_End;
        }
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos)
    {
        Q_UNUSED(onSketchPos);
        if (Mode == STATUS_End) {
            if (offsetLength != 0) {
                if (toolSettings->widget->isSettingSet[0]) {
                    offsetLength = toolSettings->widget->toolParameters[0];
                }
                else {
                    findOffsetLength();
                }

                if (toolSettings->widget->isCheckBoxChecked(3))
                    joinMode = JoinMode::Arc;
                else
                    joinMode = JoinMode::Intersection;

                findOffsetDirections();
                makeOffset(static_cast<int>(joinMode), false, true);

                Gui::Command::commitCommand();
            }
            else {
                Gui::Command::abortCommand();
            }
            sketchgui->getSketchObject()->solve(true);
            sketchgui->draw(false, false); // Redraw

            sketchgui->purgeHandler();
        }
        return true;
    }
protected:
    class ContinuousCurveElement
    {
    public:
        ContinuousCurveElement(int geoId)
            : geoId(geoId)
            , separatingVectorSign(0)
            , separatingVectorToPrevSign(0)
            , offsetDirectionSign(1)
            , curveLost(false)
            , angleToNext(0.), angleToPrev(0.), radius(1.)
            , centerPoint(Base::Vector2d(0., 0.))
            , offsetPeriapsis(Base::Vector2d(0., 0.))
            , offsetPositiveB(Base::Vector2d(0., 0.))
            , separatingVector(Base::Vector2d(0., 0.))
            , separatingVectorToPrev(Base::Vector2d(0., 0.))
            , pointToNext(Base::Vector2d(0., 0.))
            , pointToPrevious(Base::Vector2d(0., 0.))
            , offsetPointToNext(Base::Vector2d(5., 5.))
            , offsetPointToPrevious(Base::Vector2d(10., 10.))
            , pointPosToNext(Sketcher::PointPos::none)
            , pointPosToPrev(Sketcher::PointPos::none) {}

        int geoId;
        int separatingVectorSign,separatingVectorToPrevSign;
        int offsetDirectionSign;
        bool curveLost;
        double angleToNext, angleToPrev, radius;
        Base::Vector2d offsetPeriapsis, offsetPositiveB;
        Base::Vector2d centerPoint;
        Base::Vector2d separatingVector, separatingVectorToPrev;
        Base::Vector2d pointToNext, pointToPrevious;
        Base::Vector2d offsetPointToNext, offsetPointToPrevious;
        Sketcher::PointPos pointPosToNext, pointPosToPrev;
        const Part::Geometry* geo;

        void printCce() {
            Base::Console().Warning("geoId: %d\n", geoId);
            Base::Console().Warning("separatingVectorSign: %d\n", separatingVectorSign);
            Base::Console().Warning("separatingVectorToPrevSign: %d\n", separatingVectorToPrevSign);
            Base::Console().Warning("offsetDirectionSign: %d\n", offsetDirectionSign);
            Base::Console().Warning("separatingVector: %f %f\n", separatingVector.x, separatingVector.y);
            Base::Console().Warning("separatingVectorToPrev: %f %f\n", separatingVectorToPrev.x, separatingVectorToPrev.y);
            Base::Console().Warning("pointToNext: %f %f\n", pointToNext.x, pointToNext.y);
            Base::Console().Warning("pointToPrevious: %f %f\n", pointToPrevious.x, pointToPrevious.y);
            Base::Console().Warning("pointPosToNext: %d\n", pointPosToNext);
            Base::Console().Warning("pointPosToPrev: %d\n", pointPosToPrev);
        }
    };

    class CoincidencePointPos
    {
    public:
        Sketcher::PointPos FirstGeoPos;
        Sketcher::PointPos SecondGeoPos;
        Sketcher::PointPos SecondCoincidenceFirstGeoPos;
        Sketcher::PointPos SecondCoincidenceSecondGeoPos;
    };

    SelectMode Mode;
    SnapMode snapMode;
    JoinMode joinMode;
    std::vector<int> listOfGeoIds;
    std::vector<std::vector<ContinuousCurveElement>> vCC;
    std::vector<bool> isCurveiClosed;
    Base::Vector2d projectedPoint, endpoint;
    Sketcher::SketchObject* offsetObj;
    std::vector<TopoDS_Wire> sourceWires;

    bool deleteOriginal, previewEnabled;
    double offsetLength, prevOffsetLength;
    int firstCurveCreated, offsetCurveUsed, continuousCurveOfCurvedUsed, offsetDirection;

    //OCC engine
    void makeOffset(short joinType, bool allowOpenResult, bool onReleaseButton){
        //make offset shape using BRepOffsetAPI_MakeOffset
        TopoDS_Shape offsetShape;
        if (offsetLength > Precision::Confusion()) {
            Part::BRepOffsetAPI_MakeOffsetFix mkOffset(GeomAbs_JoinType(joinType), allowOpenResult);
            for (TopoDS_Wire& w : sourceWires) {
                mkOffset.AddWire(w);
            }
            try {
#if defined(__GNUC__) && defined (FC_OS_LINUX)
                Base::SignalException se;
#endif
                mkOffset.Perform(offsetLength * offsetDirection);
            }
            catch (Standard_Failure&) {
                throw;
            }
            catch (...) {
                throw Base::CADKernelError("BRepOffsetAPI_MakeOffset has crashed! (Unknown exception caught)");
            }
            offsetShape = mkOffset.Shape();

            if (offsetShape.IsNull())
                throw Base::CADKernelError("makeOffset2D: result of offsetting is null!");

            //Copying shape to fix strange orientation behavior, OCC7.0.0. See bug #2699
            // http://www.freecadweb.org/tracker/view.php?id=2699
            offsetShape = BRepBuilderAPI_Copy(offsetShape).Shape();
        }


        //turn wires/edges of shape into Geometries.
        std::vector<Part::Geometry*> geometriesToAdd;
        TopExp_Explorer expl(offsetShape, TopAbs_EDGE);
        int i = 0;
        for (; expl.More(); expl.Next()) {

            const TopoDS_Edge& edge = TopoDS::Edge(expl.Current());
            BRepAdaptor_Curve curve(edge);
            if (curve.GetType() == GeomAbs_Line) {
                double first = curve.FirstParameter();
                if (fabs(first) > 1E99) {
                    first = -10000;
                }

                double last = curve.LastParameter();
                if (fabs(last) > 1E99) {
                    last = +10000;
                }

                gp_Pnt P1 = curve.Value(first);
                gp_Pnt P2 = curve.Value(last);

                Base::Vector3d p1(P1.X(), P1.Y(), P1.Z());
                Base::Vector3d p2(P2.X(), P2.Y(), P2.Z());
                Part::GeomLineSegment* line = new Part::GeomLineSegment();
                line->setPoints(p1, p2);
                GeometryFacade::setConstruction(line, false);
                geometriesToAdd.push_back(line);
            }
            else if (curve.GetType() == GeomAbs_Circle) {
                gp_Circ circle = curve.Circle();
                gp_Pnt cnt = circle.Location();
                gp_Pnt beg = curve.Value(curve.FirstParameter());
                gp_Pnt end = curve.Value(curve.LastParameter());

                if (beg.SquareDistance(end) < Precision::Confusion()) {
                    Part::GeomCircle* gCircle = new Part::GeomCircle();
                    gCircle->setRadius(circle.Radius());
                    gCircle->setCenter(Base::Vector3d(cnt.X(), cnt.Y(), cnt.Z()));

                    GeometryFacade::setConstruction(gCircle, false);
                    geometriesToAdd.push_back(gCircle);
                }
                else {
                    Part::GeomArcOfCircle* gArc = new Part::GeomArcOfCircle();
                    Handle(Geom_Curve) hCircle = new Geom_Circle(circle);
                    Handle(Geom_TrimmedCurve) tCurve = new Geom_TrimmedCurve(hCircle, curve.FirstParameter(),
                        curve.LastParameter());
                    gArc->setHandle(tCurve);
                    GeometryFacade::setConstruction(gArc, false);
                    geometriesToAdd.push_back(gArc);
                }
            }
            else if (curve.GetType() == GeomAbs_Ellipse) {

                gp_Elips elipsOrig = curve.Ellipse();
                gp_Pnt origCenter = elipsOrig.Location();

                gp_Dir origAxisMajorDir = elipsOrig.XAxis().Direction();
                gp_Vec origAxisMajor = elipsOrig.MajorRadius() * gp_Vec(origAxisMajorDir);
                gp_Dir origAxisMinorDir = elipsOrig.YAxis().Direction();
                gp_Vec origAxisMinor = elipsOrig.MinorRadius() * gp_Vec(origAxisMinorDir);

                Handle(Geom_Ellipse) curve = new Geom_Ellipse(elipsOrig);
                Part::GeomEllipse* ellipse = new Part::GeomEllipse();
                ellipse->setHandle(curve);
                GeometryFacade::setConstruction(ellipse, false);

                geometriesToAdd.push_back(ellipse);
            }
            i++;
        }

        //Creates geos
        restartCommand(QT_TRANSLATE_NOOP("Command", "Offset"));
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        Obj->addGeometry(std::move(geometriesToAdd));

        if (onReleaseButton) {

            //Create constraints
            std::stringstream stream;
            stream << "conList = []\n";
            for (int i = firstCurveCreated; i < getHighestCurveIndex(); i++) {
                for (int j = i + 1; j < getHighestCurveIndex() + 1; j++) {
                    //here we check for coincidence on all geometries. It's far from ideal. We should check only the geometries that were inside a wire next to each other.
                    Base::Vector2d firstStartPoint, firstEndPoint, secondStartPoint, secondEndPoint;
                    if (getFirstSecondPoints(i, firstStartPoint, firstEndPoint) && getFirstSecondPoints(j, secondStartPoint, secondEndPoint)) {
                        if ((firstStartPoint - secondStartPoint).Length() < Precision::Confusion()) {
                            stream << "conList.append(Sketcher.Constraint('Coincident'," << i << ",1, " << j << ",1))\n";
                        }
                        else if ((firstStartPoint - secondEndPoint).Length() < Precision::Confusion()) {
                            stream << "conList.append(Sketcher.Constraint('Coincident'," << i << ",1, " << j << ",2))\n";
                        }
                        else if ((firstEndPoint - secondStartPoint).Length() < Precision::Confusion()) {
                            stream << "conList.append(Sketcher.Constraint('Coincident'," << i << ",2, " << j << ",1))\n";
                        }
                        else if ((firstEndPoint - secondEndPoint).Length() < Precision::Confusion()) {
                            stream << "conList.append(Sketcher.Constraint('Coincident'," << i << ",2, " << j << ",2))\n";
                        }
                    }
                }
            }
            stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
            stream << "del conList\n";
            Gui::Command::doCommand(Gui::Command::Doc, stream.str().c_str());

            //Delete original geometries if necessary
            if (!toolSettings->widget->isCheckBoxChecked(2)) {
                std::stringstream stream;
                for (size_t j = 0; j < listOfGeoIds.size() - 1; j++) {
                    stream << listOfGeoIds[j] << ",";
                }
                stream << listOfGeoIds[listOfGeoIds.size() - 1];
                try {
                    Gui::cmdAppObjectArgs(sketchgui->getObject(), "delGeometries([%s])", stream.str().c_str());
                }
                catch (const Base::Exception& e) {
                    Base::Console().Error("%s\n", e.what());
                }
            }
        }
    }

    bool getFirstSecondPoints(int geoId, Base::Vector2d& startPoint, Base::Vector2d& endPoint) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        const Part::Geometry* geo = Obj->getGeometry(geoId);

        if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
            startPoint = vec3dTo2d(line->getStartPoint());
            endPoint = vec3dTo2d(line->getEndPoint());
            return true;
        }
        else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(geo);
            startPoint = vec3dTo2d(arcOfCircle->getStartPoint());
            endPoint = vec3dTo2d(arcOfCircle->getEndPoint());
            return true;
        }
        else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
            const Part::GeomBSplineCurve* bSpline = static_cast<const Part::GeomBSplineCurve*>(geo);
            startPoint = vec3dTo2d(bSpline->getStartPoint());
            endPoint = vec3dTo2d(bSpline->getEndPoint());
            return true;
        }
        return false;
    }

    //custom engine
    void generatevCC() {
        //This function separates all the selected geometries into continuous curves.
        //vCC is a vector of geometries vectors. Each geometry vector being continuous geoID ordered.
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        for (size_t i = 0; i < listOfGeoIds.size(); i++) {
            std::vector<ContinuousCurveElement> vecOfGeoIds;
            ContinuousCurveElement cce(listOfGeoIds[i]);
            cce.geo = Obj->getGeometry(listOfGeoIds[i]);
            if (cce.geo->getTypeId() == Part::GeomCircle::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                vecOfGeoIds.push_back(cce);
                vCC.push_back(vecOfGeoIds);
            }
            else if (cce.geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()
                || cce.geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
                bool inserted = 0;
                int insertedIn = -1;
                for (size_t j = 0; j < vCC.size(); j++) {
                    for (size_t k = 0; k < vCC[j].size(); k++) {
                        CoincidencePointPos pointPosOfCoincidence = checkForCoincidence(listOfGeoIds[i], vCC[j][k].geoId);
                        if (pointPosOfCoincidence.FirstGeoPos != Sketcher::PointPos::none) {
                            if (inserted && insertedIn != j) {
                                //if it's already inserted in another continuous curve then we need to merge both curves together.
                                vCC[insertedIn].insert(vCC[insertedIn].end(), vCC[j].begin(), vCC[j].end());
                                vCC.erase(vCC.begin() + j);
                            }
                            else {
                                //we need to get the curves in the correct order.
                                if (k == vCC[j].size() - 1) {
                                    cce.pointPosToPrev = pointPosOfCoincidence.FirstGeoPos;
                                    cce.pointPosToNext = (cce.pointPosToPrev == Sketcher::PointPos::start) ? Sketcher::PointPos::end : Sketcher::PointPos::start;
                                    vCC[j][k].pointPosToNext = pointPosOfCoincidence.SecondGeoPos;
                                    vCC[j][k].pointPosToPrev = (pointPosOfCoincidence.SecondGeoPos == Sketcher::PointPos::start) ? Sketcher::PointPos::end : Sketcher::PointPos::start;
                                    vCC[j].push_back(cce);
                                }
                                else {
                                    cce.pointPosToNext = pointPosOfCoincidence.FirstGeoPos;
                                    cce.pointPosToPrev = (cce.pointPosToNext == Sketcher::PointPos::start) ? Sketcher::PointPos::end : Sketcher::PointPos::start;
                                    vCC[j][k].pointPosToPrev = pointPosOfCoincidence.SecondGeoPos;
                                    vCC[j].insert(vCC[j].begin() + k, cce);
                                }
                                insertedIn = j;
                                inserted = 1;
                            }
                            break;
                        }
                    }
                }
                if (!inserted) {
                    vecOfGeoIds.push_back(cce);
                    vCC.push_back(vecOfGeoIds);
                }
            }
        }

        for (size_t i = 0; i < vCC.size(); i++) {
            //Check if curve is closed
            bool isCurveClosed = false;
            if (vCC[i].size() > 2) {
                CoincidencePointPos cpp = checkForCoincidence(vCC[i][0].geoId, vCC[i][vCC[i].size() - 1].geoId);
                if (cpp.FirstGeoPos != Sketcher::PointPos::none)
                    isCurveClosed = true;
            }
            else if (vCC[i].size() == 2) {
                //if only 2 elements, we need to check that they don't close end to end.
                CoincidencePointPos cpp = checkForCoincidence(vCC[i][0].geoId, vCC[i][vCC[i].size() - 1].geoId);
                if (cpp.FirstGeoPos != Sketcher::PointPos::none) {
                    if (cpp.SecondCoincidenceFirstGeoPos != Sketcher::PointPos::none) {
                        isCurveClosed = true;
                    }
                }
            }
            isCurveiClosed.push_back(isCurveClosed);
            if (isCurveClosed)
                Base::Console().Warning("Curve CLOSED\n");

            //Calculate the vectors that separate each segment of the continuous curve 'areas of influence'.
            if (vCC[i].size() > 1) {
                for (size_t j = 0; j < vCC[i].size(); j++) {
                    Base::Vector2d vector1 = Base::Vector2d(0., 0.);
                    Base::Vector2d vector2 = Base::Vector2d(0., 0.);
                    Base::Vector2d referencePointForSign;
                    size_t jplus1;
                    if (j + 1 < vCC[i].size())
                        jplus1 = j + 1;
                    else
                        jplus1 = 0;

                    if (j + 1 < vCC[i].size() || isCurveClosed) {
                        int signForVecDir1 = getSignForVecDirection(vCC[i][j].geo, vCC[i][j].pointPosToNext);
                        if (vCC[i][j].geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(vCC[i][j].geo);
                            if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                                vCC[i][j].pointToNext = vec3dTo2d(line->getStartPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(line->getEndPoint());
                                vector1 = vec3dTo2d(line->getEndPoint() - line->getStartPoint());
                                referencePointForSign = vec3dTo2d(line->getEndPoint());
                            }
                            else if (vCC[i][j].pointPosToNext == Sketcher::PointPos::end) {
                                vCC[i][j].pointToNext = vec3dTo2d(line->getEndPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(line->getStartPoint());
                                vector1 = vec3dTo2d(line->getStartPoint() - line->getEndPoint());
                                referencePointForSign = vec3dTo2d(line->getStartPoint());
                            }
                        }
                        else if (vCC[i][j].geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(vCC[i][j].geo);
                            vCC[i][j].centerPoint = vec3dTo2d(arcOfCircle->getCenter());
                            if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                                vCC[i][j].pointToNext = vec3dTo2d(arcOfCircle->getStartPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(arcOfCircle->getEndPoint());
                                vector1.x = -signForVecDir1 * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).y;
                                vector1.y = signForVecDir1 * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getStartPoint()) + vector1;
                            }
                            else if (vCC[i][j].pointPosToNext == Sketcher::PointPos::end) {
                                vCC[i][j].pointToNext = vec3dTo2d(arcOfCircle->getEndPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(arcOfCircle->getStartPoint());
                                vector1.x = -signForVecDir1 * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).y;
                                vector1.y = signForVecDir1 * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getEndPoint()) + vector1;
                            }
                        }

                        int signForVecDir2 = getSignForVecDirection(vCC[i][jplus1].geo, vCC[i][jplus1].pointPosToPrev);
                        if (vCC[i][jplus1].geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(vCC[i][jplus1].geo);
                            if (vCC[i][jplus1].pointPosToPrev == Sketcher::PointPos::start) {
                                vector2 = vec3dTo2d(line->getEndPoint() - line->getStartPoint());
                            }
                            else if (vCC[i][jplus1].pointPosToPrev == Sketcher::PointPos::end) {
                                vector2 = vec3dTo2d(line->getStartPoint() - line->getEndPoint());
                            }
                        }
                        else if (vCC[i][jplus1].geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
                            || vCC[i][jplus1].geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
                            || vCC[i][jplus1].geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
                            || vCC[i][jplus1].geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(vCC[i][jplus1].geo);
                            if (vCC[i][jplus1].pointPosToPrev == Sketcher::PointPos::start) {
                                vector2.x = -signForVecDir2 * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).y;
                                vector2.y = signForVecDir2 * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).x;
                            }
                            else if (vCC[i][jplus1].pointPosToPrev == Sketcher::PointPos::end) {
                                vector2.x = -signForVecDir2 * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).y;
                                vector2.y = signForVecDir2 * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).x;
                            }
                        }

                        //Get the bisecting vector.
                        if (vector1.Length() != 0 && vector2.Length() != 0) {
                            vCC[i][j].separatingVector = vector1 / vector1.Length() + vector2 / vector2.Length();
                            vCC[i][jplus1].separatingVectorToPrev = vCC[i][j].separatingVector;
                            vCC[i][j].separatingVectorSign = getPointSideOfVector(referencePointForSign, vCC[i][j].separatingVector, vCC[i][j].pointToNext);
                            vCC[i][jplus1].separatingVectorToPrevSign = -vCC[i][j].separatingVectorSign;
                        }
                    }
                    else {
                        //Case of open curve last geoId. We need to get n+1 separating vectors instead of n. The n+1 is the left of the 0.
                        int signForVecDir1 = getSignForVecDirection(vCC[i][j].geo, vCC[i][j].pointPosToNext);
                        if (vCC[i][j].geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(vCC[i][j].geo);
                            if (vCC[i][j].pointPosToPrev == Sketcher::PointPos::start) {
                                vCC[i][j].pointToNext = vec3dTo2d(line->getEndPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(line->getStartPoint());
                                vector1.x = -(line->getStartPoint() - line->getEndPoint()).y;
                                vector1.y = (line->getStartPoint() - line->getEndPoint()).x;
                                referencePointForSign = vec3dTo2d(line->getStartPoint());
                            }
                            else if (vCC[i][j].pointPosToPrev == Sketcher::PointPos::end) {
                                vCC[i][j].pointToNext = vec3dTo2d(line->getStartPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(line->getEndPoint());
                                vector1.x = -(line->getEndPoint() - line->getStartPoint()).y;
                                vector1.y = (line->getEndPoint() - line->getStartPoint()).x;
                                referencePointForSign = vec3dTo2d(line->getEndPoint());
                            }
                        }
                        else if (vCC[i][j].geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
                            || vCC[i][j].geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(vCC[i][j].geo);
                            vCC[i][j].centerPoint = vec3dTo2d(arcOfCircle->getCenter());
                            if (vCC[i][j].pointPosToPrev == Sketcher::PointPos::start) {
                                vCC[i][j].pointToNext = vec3dTo2d(arcOfCircle->getEndPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(arcOfCircle->getStartPoint());
                                vector1.x = (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).x;
                                vector1.y = (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).y;
                                Base::Vector2d vector3;
                                vector3.x = -signForVecDir1 * vector1.y;
                                vector3.y = signForVecDir1 * vector1.x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getEndPoint()) + vector3;
                            }
                            else if (vCC[i][j].pointPosToPrev == Sketcher::PointPos::end) {
                                vCC[i][j].pointToNext = vec3dTo2d(arcOfCircle->getStartPoint());
                                vCC[i][j].pointToPrevious = vec3dTo2d(arcOfCircle->getEndPoint());
                                vector1.x = (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).x;
                                vector1.y = (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).y;
                                Base::Vector2d vector3;
                                vector3.x = -signForVecDir1 * vector1.y;
                                vector3.y = signForVecDir1 * vector1.x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getStartPoint()) + vector3;
                            }
                        }
                        if (vector1.Length() != 0) {
                            vCC[i][j].separatingVector = vector1 / vector1.Length();
                            vCC[i][j].separatingVectorSign = getPointSideOfVector(referencePointForSign, vCC[i][j].separatingVector, vCC[i][j].pointToNext);
                        }

                        int signForVecDir2 = getSignForVecDirection(vCC[i][0].geo, vCC[i][0].pointPosToPrev);
                        if (vCC[i][0].geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(vCC[i][0].geo);
                            if (vCC[i][0].pointPosToNext == Sketcher::PointPos::start) {
                                vector2.x = -(line->getStartPoint() - line->getEndPoint()).y;
                                vector2.y = (line->getStartPoint() - line->getEndPoint()).x;
                                referencePointForSign = vec3dTo2d(line->getStartPoint());
                            }
                            else if (vCC[i][0].pointPosToNext == Sketcher::PointPos::end) {
                                vector2.x = -(line->getEndPoint() - line->getStartPoint()).y;
                                vector2.y = (line->getEndPoint() - line->getStartPoint()).x;
                                referencePointForSign = vec3dTo2d(line->getEndPoint());
                            }
                        }
                        else if (vCC[i][0].geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
                            || vCC[i][0].geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
                            || vCC[i][0].geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
                            || vCC[i][0].geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(vCC[i][0].geo);
                            if (vCC[i][0].pointPosToNext == Sketcher::PointPos::start) {
                                vector2 = vec3dTo2d(arcOfCircle->getCenter() - arcOfCircle->getEndPoint());
                                Base::Vector2d vector3;
                                vector3.x = -signForVecDir1 * vector1.y;
                                vector3.y = signForVecDir1 * vector1.x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getEndPoint()) + vector3;
                            }
                            else if (vCC[i][0].pointPosToNext == Sketcher::PointPos::end) {
                                vector2 = vec3dTo2d(arcOfCircle->getCenter() - arcOfCircle->getStartPoint());
                                Base::Vector2d vector3;
                                vector3.x = -signForVecDir1 * vector1.y;
                                vector3.y = signForVecDir1 * vector1.x;
                                referencePointForSign = vec3dTo2d(arcOfCircle->getStartPoint()) + vector3;
                            }
                        }
                        if (vector2.Length() != 0) {
                            vCC[i][0].separatingVectorToPrev = vector2 / vector2.Length();
                            vCC[i][0].separatingVectorToPrevSign = getPointSideOfVector(referencePointForSign, vCC[i][0].separatingVectorToPrev, vCC[i][0].pointToPrevious);
                            //Base::Console().Warning("i - j: %d - %d\n", i, 0);
                            //vCC[i][0].printCce();
                        }
                    }
                    //Base::Console().Warning("i - j: %d - %d\n", i,j);
                    //vCC[i][j].printCce();
                }
            }
        }
    }

    void generateSourceWires() {
        for (size_t i = 0; i < vCC.size(); i++) {
            BRepBuilderAPI_MakeWire mkWire;
            for (size_t j = 0; j < vCC[i].size(); j++) {
                mkWire.Add(TopoDS::Edge(vCC[i][j].geo->toShape()));

            }
            sourceWires.push_back(mkWire.Wire());
        }
    }

    void findOffsetLength() {
        int newOffsetCurveUsed = listOfGeoIds[0];
        int newContinuousCurveOfCurvedUsed = 0;
        double newOffsetLength = 1000000000000;
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();

        for (size_t i = 0; i < vCC.size(); i++) {
            int curveUsed = listOfGeoIds[0];
            double distanceToContinuousCurve = 1000000000000;
            for (size_t j = 0; j < vCC[i].size(); j++) {
                //First we see if we are in area of Influence for that geoID. If only one element then we are automatically.
                bool inInfluenceArea = false;
                double distanceToCurve = listOfGeoIds[0];
                if (vCC[i].size() == 1) {
                    inInfluenceArea = true;
                }
                else {
                    inInfluenceArea = isInInfluenceArea(vCC[i][j]);
                }
                //Base::Console().Warning("%d%d.inInfluenceArea: %d\n", i,j, inInfluenceArea);
                if (inInfluenceArea) {
                    Base::Vector2d currentProjectedPoint;
                    const Part::Geometry* geo = Obj->getGeometry(vCC[i][j].geoId);

                    if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                        const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
                        currentProjectedPoint.ProjectToLine(endpoint - vec3dTo2d(line->getStartPoint()), vec3dTo2d(line->getEndPoint() - line->getStartPoint()));
                        currentProjectedPoint = vec3dTo2d(line->getStartPoint()) + currentProjectedPoint;
                        distanceToCurve = (endpoint - currentProjectedPoint).Length();
                        vCC[i][j].offsetDirectionSign = getPointSideOfVector(endpoint, vec3dTo2d(line->getEndPoint() - line->getStartPoint()), vec3dTo2d(line->getStartPoint()));
                    }
                    else if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                        const Part::GeomCircle* circle = static_cast<const Part::GeomCircle*>(geo);
                        distanceToCurve = (endpoint - vec3dTo2d(circle->getCenter())).Length() - circle->getRadius();
                        vCC[i][j].offsetDirectionSign = Sign(1, distanceToCurve);
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                        const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
                        distanceToCurve = (endpoint - vec3dTo2d(arcOfCircle->getCenter())).Length() - arcOfCircle->getRadius();
                        vCC[i][j].offsetDirectionSign = Sign(1, distanceToCurve);
                    }
                    else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                        const Part::GeomEllipse* ellipse = static_cast<const Part::GeomEllipse*>(geo);
                        Base::Vector2d centerPoint = vec3dTo2d(ellipse->getCenter());
                        Base::Vector2d ellipseAxis = vec3dTo2d(ellipse->getMajorAxisDir());
                        double angleToMajorAxis = (endpoint - centerPoint).Angle() - ellipseAxis.Angle();
                        double radiusAtAngle = abs(cos(angleToMajorAxis)) * ellipse->getMajorRadius() + abs(sin(angleToMajorAxis)) * ellipse->getMinorRadius();
                        distanceToCurve = (endpoint - centerPoint).Length() - radiusAtAngle;
                        vCC[i][j].offsetDirectionSign = Sign(1, distanceToCurve);
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                        const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
                        Base::Vector2d centerPoint = vec3dTo2d(arcOfEllipse->getCenter());
                        Base::Vector2d ellipseAxis = vec3dTo2d(arcOfEllipse->getMajorAxisDir());
                        double angleToMajorAxis = (endpoint - centerPoint).Angle() - ellipseAxis.Angle();
                        double radiusAtAngle = abs(cos(angleToMajorAxis)) * arcOfEllipse->getMajorRadius() + abs(sin(angleToMajorAxis)) * arcOfEllipse->getMinorRadius();
                        distanceToCurve = (endpoint - centerPoint).Length() - radiusAtAngle;
                        vCC[i][j].offsetDirectionSign = Sign(1, distanceToCurve);
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
                        /*const Part::GeomArcOfHyperbola* arcOfHyperbola = static_cast<const Part::GeomArcOfHyperbola*>(geo);
                        Base::Vector3d offsetdCenterPoint = getOffsetdPoint(arcOfHyperbola->getCenter(), referencePoint, offsetFactor);
                        Base::Vector3d ellipseAxis = arcOfHyperbola->getMajorAxisDir();
                        Base::Vector3d periapsis = arcOfHyperbola->getCenter() + (ellipseAxis / ellipseAxis.Length()) * arcOfHyperbola->getMajorRadius();
                        periapsis = getOffsetdPoint(periapsis, referencePoint, offsetFactor);
                        Base::Vector3d ellipseMinorAxis;
                        ellipseMinorAxis.x = -ellipseAxis.y;
                        ellipseMinorAxis.y = ellipseAxis.x;
                        Base::Vector3d positiveB = arcOfHyperbola->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * arcOfHyperbola->getMinorRadius();
                        positiveB = getOffsetdPoint(positiveB, referencePoint, offsetFactor);
                        double arcStartAngle, arcEndAngle;
                        arcOfHyperbola->getRange(arcStartAngle, arcEndAngle, true);*/
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
                        const Part::GeomArcOfParabola* arcOfParabola = static_cast<const Part::GeomArcOfParabola*>(geo);
                        //Base::Vector3d offsetdFocusPoint = getOffsetdPoint(arcOfParabola->getFocus(), referencePoint, offsetFactor);
                        //Base::Vector3d offsetdCenterPoint = getOffsetdPoint(arcOfParabola->getCenter(), referencePoint, offsetFactor);
                        double arcStartAngle, arcEndAngle;
                        arcOfParabola->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                    }
                    else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
                    }

                    distanceToCurve = abs(distanceToCurve);
                    if (distanceToCurve == min(distanceToCurve, distanceToContinuousCurve)) {
                        curveUsed = j;
                        distanceToContinuousCurve = distanceToCurve;
                    }
                }
            }
            if (distanceToContinuousCurve == min(distanceToContinuousCurve, newOffsetLength)) {
                newOffsetCurveUsed = curveUsed;
                newContinuousCurveOfCurvedUsed = i;
                newOffsetLength = distanceToContinuousCurve;
            }
        }
        if (newOffsetLength != 1000000000000) {
            offsetLength = newOffsetLength;
        }
        if (newOffsetCurveUsed != listOfGeoIds[0]) {
            offsetCurveUsed = newOffsetCurveUsed;
            continuousCurveOfCurvedUsed = newContinuousCurveOfCurvedUsed;
        }
    }

    bool isInInfluenceArea(ContinuousCurveElement& curveElement) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        const Part::Geometry* geo = Obj->getGeometry(curveElement.geoId);

        if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
            Base::Vector2d projectedPoint;
            projectedPoint.ProjectToLine(endpoint - vec3dTo2d(line->getStartPoint()), vec3dTo2d(line->getEndPoint() - line->getStartPoint()));
            projectedPoint = vec3dTo2d(line->getStartPoint()) + projectedPoint;
            double d1 = (vec3dTo2d(line->getStartPoint()) - projectedPoint).Length();
            double d2 = (vec3dTo2d(line->getEndPoint()) - projectedPoint).Length();
            double d3 = (line->getEndPoint() - line->getStartPoint()).Length();
            if (d1 < d3 && d2 < d3) {
                return true;
            }
            else {
                if (getPointSideOfVector(endpoint, curveElement.separatingVector, curveElement.pointToNext) == curveElement.separatingVectorSign
                    && getPointSideOfVector(endpoint, curveElement.separatingVectorToPrev, curveElement.pointToPrevious) == curveElement.separatingVectorToPrevSign) {
                    return true;
                }
            }
        }
        else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(geo);
            double startAngle, endAngle, totalAngle, endPointAngle;
            arcOfCircle->getRange(startAngle, endAngle, true);
            totalAngle = abs(endAngle - startAngle);
            endPointAngle = endpoint.Angle();
            if (abs(endPointAngle - startAngle) < totalAngle && abs(endPointAngle - endAngle) < totalAngle) {
                return true;
            }
            else {
                //check if we are outside of the arc on first side, then if we are on the correct side of bisecting it's ok.
                Base::Vector2d vector1, vector2, referencePointForSign;
                int signForVecDir = getSignForVecDirection(geo, curveElement.pointPosToNext);
                if (curveElement.pointPosToNext == Sketcher::PointPos::start) {
                    vector1 = vec3dTo2d(arcOfCircle->getStartPoint()) - vec3dTo2d(arcOfCircle->getCenter());
                    vector2.x = -signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).y;
                    vector2.y = signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).x;
                    referencePointForSign = vec3dTo2d(arcOfCircle->getStartPoint()) + vector2;
                }
                else {
                    vector1 = vec3dTo2d(arcOfCircle->getEndPoint()) - vec3dTo2d(arcOfCircle->getCenter());
                    vector2.x = -signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).y;
                    vector2.y = signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).x;
                    referencePointForSign = vec3dTo2d(arcOfCircle->getEndPoint()) + vector2;
                }
                int refSign = getPointSideOfVector(referencePointForSign, vector1, curveElement.pointToNext);
                if (getPointSideOfVector(endpoint, vector1, curveElement.pointToNext) != refSign) {
                    if (getPointSideOfVector(endpoint, curveElement.separatingVector, curveElement.pointToNext) == curveElement.separatingVectorSign)
                        return true;
                }
                //check second side : 
                signForVecDir = getSignForVecDirection(geo, curveElement.pointPosToPrev);
                if (curveElement.pointPosToPrev == Sketcher::PointPos::start) {
                    vector1 = vec3dTo2d(arcOfCircle->getEndPoint()) - vec3dTo2d(arcOfCircle->getCenter());
                    vector2.x = -signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).y;
                    vector2.y = signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getEndPoint()).x;
                    referencePointForSign = vec3dTo2d(arcOfCircle->getEndPoint()) + vector2;
                }
                else {
                    vector1 = vec3dTo2d(arcOfCircle->getStartPoint()) - vec3dTo2d(arcOfCircle->getCenter());
                    vector2.x = -signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).y;
                    vector2.y = signForVecDir * (arcOfCircle->getCenter() - arcOfCircle->getStartPoint()).x;
                    referencePointForSign = vec3dTo2d(arcOfCircle->getStartPoint()) + vector2;
                }
                refSign = getPointSideOfVector(referencePointForSign, vector1, curveElement.pointToPrevious);
                if (getPointSideOfVector(endpoint, vector1, curveElement.pointToPrevious) != refSign) {
                    if (getPointSideOfVector(endpoint, curveElement.separatingVectorToPrev, curveElement.pointToPrevious) == curveElement.separatingVectorToPrevSign)
                        return true;
                }
            }
        }


        return false;
    }

    void findOffsetDirections() {

        for (size_t i = 0; i < vCC.size(); i++) {
            int curveToUse = 0;
            if (i == continuousCurveOfCurvedUsed) {
                curveToUse = offsetCurveUsed;
            }

            for (size_t j = curveToUse + 1; j < vCC[i].size(); j++) {
                getDirectionOfCurve(vCC[i][j - 1], vCC[i][j], true);
            }
            for (int j = curveToUse - 1; 0 <= j; j--) {
                getDirectionOfCurve(vCC[i][j + 1], vCC[i][j], false);
            }
        }
    }

    void getDirectionOfCurve(ContinuousCurveElement& reference, ContinuousCurveElement& toUpdate, bool toNext) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        Base::Vector2d pointAtSeparatingVector;
        int signOfPointToRef = 1;
        int signOfPointToUpdate = 1;
        bool signReversed = false;
        if (toNext)
            pointAtSeparatingVector = reference.pointToNext + reference.separatingVector;
        else
            pointAtSeparatingVector = reference.pointToPrevious + reference.separatingVectorToPrev;

        const Part::Geometry* geo = Obj->getGeometry(reference.geoId);
        if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
            signOfPointToRef = getPointSideOfVector(pointAtSeparatingVector, vec3dTo2d(line->getEndPoint() - line->getStartPoint()), vec3dTo2d(line->getStartPoint()));
        }
        else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
            const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
            double distanceToCurve = (pointAtSeparatingVector - vec3dTo2d(arcOfCircle->getCenter())).Length() - arcOfCircle->getRadius();
            signOfPointToRef = Base::sgn(distanceToCurve);
        }
        else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
            const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
            Base::Vector2d centerPoint = vec3dTo2d(arcOfEllipse->getCenter());
            Base::Vector2d ellipseAxis = vec3dTo2d(arcOfEllipse->getMajorAxisDir());
            double angleToMajorAxis = (pointAtSeparatingVector - centerPoint).Angle() - ellipseAxis.Angle();
            double radiusAtAngle = abs(cos(angleToMajorAxis)) * arcOfEllipse->getMajorRadius() + abs(sin(angleToMajorAxis)) * arcOfEllipse->getMinorRadius();
            double distanceToCurve = (pointAtSeparatingVector - centerPoint).Length() - radiusAtAngle;
            signOfPointToRef = Base::sgn(distanceToCurve);
        }
        else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
        }
        else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
        }
        else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
        }

        if (signOfPointToRef != reference.offsetDirectionSign)
            signReversed = true;


        const Part::Geometry* geo2 = Obj->getGeometry(toUpdate.geoId);
        if (geo2->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
            const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo2);
            signOfPointToUpdate = getPointSideOfVector(pointAtSeparatingVector, vec3dTo2d(line->getEndPoint() - line->getStartPoint()), vec3dTo2d(line->getStartPoint()));
        }
        else if (geo2->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
            const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo2);
            double distanceToCurve = (pointAtSeparatingVector - vec3dTo2d(arcOfCircle->getCenter())).Length() - arcOfCircle->getRadius();
            signOfPointToUpdate = Base::sgn(distanceToCurve);
        }
        else if (geo2->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
            const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo2);
            Base::Vector2d centerPoint = vec3dTo2d(arcOfEllipse->getCenter());
            Base::Vector2d ellipseAxis = vec3dTo2d(arcOfEllipse->getMajorAxisDir());
            double angleToMajorAxis = (pointAtSeparatingVector - centerPoint).Angle() - ellipseAxis.Angle();
            double radiusAtAngle = abs(cos(angleToMajorAxis)) * arcOfEllipse->getMajorRadius() + abs(sin(angleToMajorAxis)) * arcOfEllipse->getMinorRadius();
            double distanceToCurve = (pointAtSeparatingVector - centerPoint).Length() - radiusAtAngle;
            signOfPointToUpdate = Base::sgn(distanceToCurve);
        }
        else if (geo2->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {
        }
        else if (geo2->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
        }
        else if (geo2->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {
        }

        if (signOfPointToRef != 0) {
            if (signReversed)
                toUpdate.offsetDirectionSign = -signOfPointToUpdate;
            else
                toUpdate.offsetDirectionSign = signOfPointToUpdate;
        }
    }

    void findOffsetPoints() {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();

        //initialize the offset points and angles
        for (size_t i = 0; i < vCC.size(); i++) {
            for (size_t j = 0; j < vCC[i].size(); j++) {
                vCC[i][j].curveLost = false;
                const Part::Geometry* geo = Obj->getGeometry(vCC[i][j].geoId);
                if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                    const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
                    Base::Vector3d offsetVector;
                    offsetVector.x = -(line->getStartPoint() - line->getEndPoint()).y;
                    offsetVector.y = (line->getStartPoint() - line->getEndPoint()).x;
                    offsetVector = offsetVector / offsetVector.Length() * vCC[i][j].offsetDirectionSign * offsetLength;

                    if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                        vCC[i][j].offsetPointToNext = vec3dTo2d(line->getStartPoint() + offsetVector);
                        vCC[i][j].offsetPointToPrevious = vec3dTo2d(line->getEndPoint() + offsetVector);
                    }
                    else {
                        vCC[i][j].offsetPointToNext = vec3dTo2d(line->getEndPoint() + offsetVector);
                        vCC[i][j].offsetPointToPrevious = vec3dTo2d(line->getStartPoint() + offsetVector);
                    }
                }
                else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                    const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
                    vCC[i][j].radius = arcOfCircle->getRadius() + vCC[i][j].offsetDirectionSign * offsetLength;
                    if (0 < vCC[i][j].radius) {
                        double startAngle, endAngle;
                        arcOfCircle->getRange(startAngle, endAngle, true);

                        if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                            vCC[i][j].angleToNext = startAngle;
                            vCC[i][j].angleToPrev = endAngle;
                        }
                        else {
                            vCC[i][j].angleToNext = endAngle;
                            vCC[i][j].angleToPrev = startAngle;
                        }
                    }
                    else {
                        vCC[i][j].curveLost = true;
                    }
                }
                else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                    const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
                    vCC[i][j].radius = arcOfEllipse->getMinorRadius() + vCC[i][j].offsetDirectionSign * offsetLength;
                    if (0 < vCC[i][j].radius) {
                        Base::Vector3d ellipseAxis = arcOfEllipse->getMajorAxisDir();
                        vCC[i][j].offsetPeriapsis = vec3dTo2d(arcOfEllipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * (arcOfEllipse->getMajorRadius() + vCC[i][j].offsetDirectionSign * offsetLength));
                        Base::Vector3d ellipseMinorAxis;
                        ellipseMinorAxis.x = -ellipseAxis.y;
                        ellipseMinorAxis.y = ellipseAxis.x;
                        vCC[i][j].offsetPositiveB = vec3dTo2d(arcOfEllipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * vCC[i][j].radius);
                        double startAngle, endAngle;
                        arcOfEllipse->getRange(startAngle, endAngle, true);

                        if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                            vCC[i][j].angleToNext = startAngle;
                            vCC[i][j].angleToPrev = endAngle;
                        }
                        else {
                            vCC[i][j].angleToNext = endAngle;
                            vCC[i][j].angleToPrev = startAngle;
                        }
                    }
                    else {
                        vCC[i][j].curveLost = true;
                    }
                }
                else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {}
            }
        }
        
        //Update offset points and angles based on neighboors.
        for (size_t i = 0; i < vCC.size(); i++) {
            int curveLostAfterJ = 0;
            for (int j = 0; j < static_cast<int>(vCC[i].size()); j++) {
                size_t jplus1 = j + 1 + curveLostAfterJ;
                if (!(jplus1 < vCC[i].size()) && isCurveiClosed[i])
                    jplus1 = 0 + curveLostAfterJ;

                while (j < static_cast<int>(vCC[i].size()) && vCC[i][j].curveLost == true) {
                    j++;
                    if (0 < curveLostAfterJ )
                        curveLostAfterJ--;
                }

                if (jplus1 < vCC[i].size() ) {
                    const Part::Geometry* geo = Obj->getGeometry(vCC[i][j].geoId);
                    const Part::Geometry* geo2 = Obj->getGeometry(vCC[i][jplus1].geoId);
                    Base::Console().Warning("j - jplus1: %d - %d\n", j, jplus1);

                    if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                        if (geo2->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                            Base::Vector2d intersection = getLineIntersection(vCC[i][j].offsetPointToNext, vCC[i][j].offsetPointToPrevious,
                                vCC[i][jplus1].offsetPointToNext, vCC[i][jplus1].offsetPointToPrevious);
                            Base::Vector2d intersectionJplus1Next = getLineIntersection(vCC[i][jplus1].offsetPointToNext, vCC[i][jplus1].offsetPointToPrevious,
                                vCC[i][jplus1].pointToNext, vCC[i][jplus1].pointToNext + vCC[i][jplus1].separatingVector);

                            if (intersection.x != 0 || intersection.y != 0) {
                                Base::Vector2d vector3;
                                vector3.x = -(vCC[i][j].pointToNext - vCC[i][j].pointToPrevious).y;
                                vector3.y = (vCC[i][j].pointToNext - vCC[i][j].pointToPrevious).x;
                                
                                //if the direction of the lines reversed, then the geo should be lost.
                                if ((Base::sgn((intersection - vCC[i][j].offsetPointToPrevious).x) != Base::sgn((vCC[i][j].pointToNext - vCC[i][j].pointToPrevious).x)
                                    || Base::sgn((intersection - vCC[i][j].offsetPointToPrevious).y) != Base::sgn((vCC[i][j].pointToNext - vCC[i][j].pointToPrevious).y))
                                    //If line is further than offsetlength it's to be lost too. TODO: this is not  perfect.
                                    || (((intersection - vCC[i][j].pointToPrevious).Length() > offsetLength + 1)
                                    && getPointSideOfVector(vCC[i][j].pointToNext, vector3, vCC[i][j].pointToPrevious) != getPointSideOfVector(intersection, vector3, vCC[i][j].pointToPrevious)) ) {
                                    vCC[i][j].curveLost = true;
                                    Base::Console().Warning("j lost: %d\n", j);
                                    if (j != 0) { //if j=0 then we just move to the second.
                                        curveLostAfterJ++;
                                        j = j - 2; //j-1 re-run for j, j-2 re-run from j-1
                                    }
                                }
                                else if (Base::sgn((intersectionJplus1Next - intersection).x) != Base::sgn((vCC[i][jplus1].pointToNext - vCC[i][jplus1].pointToPrevious).x)
                                    || Base::sgn((intersectionJplus1Next - intersection).y) != Base::sgn((vCC[i][jplus1].pointToNext - vCC[i][jplus1].pointToPrevious).y)) {
                                    vCC[i][jplus1].curveLost = true;
                                    curveLostAfterJ++;
                                    j--; //re-run the for for this j and it will run with jplus1 = j+2
                                }
                                else {
                                    vCC[i][j].offsetPointToNext = intersection;
                                    vCC[i][jplus1].offsetPointToPrevious = intersection;
                                    j = j + curveLostAfterJ;
                                    curveLostAfterJ = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Base::Vector2d getLineIntersection(Base::Vector2d p0, Base::Vector2d p1, Base::Vector2d p2, Base::Vector2d p3)
    {
        Base::Vector2d intersection = Base::Vector2d(0., 0.);
        float s1_x, s1_y, s2_x, s2_y;
        s1_x = p1.x - p0.x;     s1_y = p1.y - p0.y;
        s2_x = p3.x - p2.x;     s2_y = p3.y - p2.y;

        float s, t;
        s = (-s1_y * (p0.x - p2.x) + s1_x * (p0.y - p2.y)) / (-s2_x * s1_y + s1_x * s2_y);
        t = (s2_x * (p0.y - p2.y) - s2_y * (p0.x - p2.x)) / (-s2_x * s1_y + s1_x * s2_y);

        // Collision detected
        intersection.x = p0.x + (t * s1_x);
        intersection.y = p0.y + (t * s1_y);
        return intersection;
    }

    void generateOffsetGeos(bool onReleaseButton) {
        if (true/*toolSettings->widget->isCheckBoxChecked(2)*/) {
            deleteOriginal = 0;
            //numberOfCopies = 2;
        }
        else {
            deleteOriginal = 1;
            //numberOfCopies = 1;
        }

        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();

        restartCommand(QT_TRANSLATE_NOOP("Command", "Offset"));
        //Creates geos
        std::stringstream stream;
        stream << "geoList = []\n";
        stream << "constrGeoList = []\n";
        for (size_t i = 0; i < vCC.size(); i++) {
            for (size_t j = 0; j < vCC[i].size(); j++) {
                if (vCC[i][j].curveLost == false) {
                    const Part::Geometry* geo = Obj->getGeometry(vCC[i][j].geoId);
                    if (GeometryFacade::getConstruction(geo)) {
                        stream << "constrGeoList.";
                    }
                    else {
                        stream << "geoList.";
                    }
                    if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                        const Part::GeomLineSegment* line = static_cast<const Part::GeomLineSegment*>(geo);
                        Base::Vector2d startPoint, endPoint;
                        if (vCC[i][j].pointPosToNext == Sketcher::PointPos::start) {
                            startPoint = vCC[i][j].offsetPointToNext;
                            endPoint = vCC[i][j].offsetPointToPrevious;
                        }
                        else {
                            endPoint = vCC[i][j].offsetPointToNext;
                            startPoint = vCC[i][j].offsetPointToPrevious;
                        }
                        stream << "append(Part.LineSegment(App.Vector(" << startPoint.x << "," << startPoint.y
                            << ",0),App.Vector(" << endPoint.x << "," << endPoint.y << ",0)))\n";

                        //for debug
                        Base::Vector2d point1, point2;
                        if (vCC[i][j].pointPosToNext == PointPos::start) {
                            point1 = vec3dTo2d(line->getStartPoint());
                            point2 = vec3dTo2d(line->getEndPoint());
                        }
                        else {
                            point1 = vec3dTo2d(line->getEndPoint());
                            point2 = vec3dTo2d(line->getStartPoint());
                        }
                        stream << "constrGeoList.append(Part.LineSegment(App.Vector(" << (point1).x << "," << (point1).y
                            << ",0),App.Vector(" << (point1 + vCC[i][j].separatingVector * 20).x << "," << (point1 + vCC[i][j].separatingVector * 20).y << ",0)))\n";
                        stream << "constrGeoList.append(Part.LineSegment(App.Vector(" << (point2).x << "," << (point2).y
                            << ",0),App.Vector(" << (point2 + vCC[i][j].separatingVectorToPrev * 20).x << "," << (point2 + vCC[i][j].separatingVectorToPrev * 20).y << ",0)))\n";
                    }
                    else if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                        const Part::GeomCircle* circle = static_cast<const Part::GeomCircle*>(geo);
                        double radius = circle->getRadius() + vCC[i][j].offsetDirectionSign * offsetLength;
                        if (0 < radius) {
                            stream << "append(Part.Circle(App.Vector(" << circle->getCenter().x << "," << circle->getCenter().y << ",0),App.Vector(0,0,1)," << radius << "))\n";
                        }
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                        const Part::GeomArcOfCircle* arcOfCircle = static_cast<const Part::GeomArcOfCircle*>(geo);
                        double radius = arcOfCircle->getRadius() + vCC[i][j].offsetDirectionSign * offsetLength;
                        if (0 < radius) {
                            double arcStartAngle, arcEndAngle;
                            arcOfCircle->getRange(arcStartAngle, arcEndAngle, /*emulateCCWXY=*/true);
                            stream << "append(Part.ArcOfCircle(Part.Circle(App.Vector(" << arcOfCircle->getCenter().x << "," << arcOfCircle->getCenter().y
                                << ",0),App.Vector(0,0,1)," << radius << "),"
                                << arcStartAngle << "," << arcEndAngle << "))\n";

                            //for debug
                            Base::Vector2d point1, point2;
                            if (vCC[i][j].pointPosToNext == PointPos::start) {
                                point1 = vec3dTo2d(arcOfCircle->getStartPoint());
                                point2 = vec3dTo2d(arcOfCircle->getEndPoint());
                            }
                            else {
                                point1 = vec3dTo2d(arcOfCircle->getEndPoint());
                                point2 = vec3dTo2d(arcOfCircle->getStartPoint());
                            }
                            stream << "constrGeoList.append(Part.LineSegment(App.Vector(" << (point1).x << "," << (point1).y
                                << ",0),App.Vector(" << (point1 + vCC[i][j].separatingVector * 20).x << "," << (point1 + vCC[i][j].separatingVector * 20).y << ",0)))\n";
                            stream << "constrGeoList.append(Part.LineSegment(App.Vector(" << (point2).x << "," << (point2).y
                                << ",0),App.Vector(" << (point2 + vCC[i][j].separatingVectorToPrev * 20).x << "," << (point2 + vCC[i][j].separatingVectorToPrev * 20).y << ",0)))\n";
                        }
                    }
                    else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {
                        const Part::GeomEllipse* ellipse = static_cast<const Part::GeomEllipse*>(geo);
                        Base::Vector3d ellipseAxis = ellipse->getMajorAxisDir();
                        Base::Vector3d periapsis = ellipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * (ellipse->getMajorRadius() + vCC[i][j].offsetDirectionSign * offsetLength);
                        Base::Vector3d ellipseMinorAxis;
                        ellipseMinorAxis.x = -ellipseAxis.y;
                        ellipseMinorAxis.y = ellipseAxis.x;
                        Base::Vector3d positiveB = ellipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * (ellipse->getMinorRadius() + vCC[i][j].offsetDirectionSign * offsetLength);
                        stream << "append(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y << ",0),App.Vector("
                            << ellipse->getCenter().x << "," << ellipse->getCenter().y << ",0)))\n";
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {
                        const Part::GeomArcOfEllipse* arcOfEllipse = static_cast<const Part::GeomArcOfEllipse*>(geo);
                        Base::Vector3d ellipseAxis = arcOfEllipse->getMajorAxisDir();
                        Base::Vector3d periapsis = arcOfEllipse->getCenter() + (ellipseAxis / ellipseAxis.Length()) * (arcOfEllipse->getMajorRadius() + vCC[i][j].offsetDirectionSign * offsetLength);
                        Base::Vector3d ellipseMinorAxis;
                        ellipseMinorAxis.x = -ellipseAxis.y;
                        ellipseMinorAxis.y = ellipseAxis.x;
                        Base::Vector3d positiveB = arcOfEllipse->getCenter() + (ellipseMinorAxis / ellipseMinorAxis.Length()) * (arcOfEllipse->getMinorRadius() + vCC[i][j].offsetDirectionSign * offsetLength);
                        double arcStartAngle, arcEndAngle;
                        arcOfEllipse->getRange(arcStartAngle, arcEndAngle, true);
                        stream << "append(Part.ArcOfEllipse(Part.Ellipse(App.Vector(" << periapsis.x << "," << periapsis.y << ",0),App.Vector(" << positiveB.x << "," << positiveB.y
                            << ",0),App.Vector(" << arcOfEllipse->getCenter().x << "," << arcOfEllipse->getCenter().y << ",0)),"
                            << arcStartAngle << "," << arcEndAngle << "))\n";
                    }
                    else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {}
                    else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {}
                    else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {}
                }
            }
        }
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(geoList,False)\n";
        stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addGeometry(constrGeoList,True)\n";
        stream << "del geoList\n";
        stream << "del constrGeoList\n";
        //Base::Console().Warning(stream.str().c_str());
        Gui::Command::doCommand(Gui::Command::Doc, stream.str().c_str());

        //Create constraints
        if (onReleaseButton) {
            //stream << "conList = []\n"; //not sure this way would be better
            const std::vector< Sketcher::Constraint* >& vals = Obj->Constraints.getValues();
            std::vector< Constraint* > newconstrVals(vals);
            std::vector<int> geoIdsWhoAlreadyHasEqual = {}; //avoid applying equal several times if cloning distanceX and distanceY of the same part.

            std::vector< Sketcher::Constraint* >::const_iterator itEnd = vals.end(); //we need vals.end before adding any constraints
            for (std::vector< Sketcher::Constraint* >::const_iterator it = vals.begin(); it != itEnd; ++it) {
                int firstIndex = indexInVec(listOfGeoIds, (*it)->First);
                int secondIndex = indexInVec(listOfGeoIds, (*it)->Second);
                int thirdIndex = indexInVec(listOfGeoIds, (*it)->Third);

                if (((*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Perpendicular)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    constNew->Third = firstCurveCreated + thirdIndex;
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Coincident
                    || (*it)->Type == Sketcher::Tangent
                    || (*it)->Type == Sketcher::Symmetric
                    || (*it)->Type == Sketcher::Perpendicular
                    || (*it)->Type == Sketcher::Parallel
                    || (*it)->Type == Sketcher::Equal
                    || (*it)->Type == Sketcher::PointOnObject)
                    && firstIndex >= 0 && secondIndex >= 0 && thirdIndex == GeoEnum::GeoUndef) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Radius
                    || (*it)->Type == Sketcher::Diameter)
                    && firstIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->setValue(constNew->getValue() );
                    newconstrVals.push_back(constNew);
                }
                else if (((*it)->Type == Sketcher::Distance
                    || (*it)->Type == Sketcher::DistanceX
                    || (*it)->Type == Sketcher::DistanceY)
                    && firstIndex >= 0 && secondIndex >= 0) {
                    Constraint* constNew = (*it)->copy();
                    constNew->First = firstCurveCreated + firstIndex;
                    constNew->Second = firstCurveCreated + secondIndex;
                    constNew->setValue(constNew->getValue() );
                    newconstrVals.push_back(constNew);
                }
            }
            if (newconstrVals.size() > vals.size())
                Obj->Constraints.setValues(std::move(newconstrVals));
            //stream << Gui::Command::getObjectCmd(sketchgui->getObject()) << ".addConstraint(conList)\n";
            //stream << "del conList\n";
        }

        if (deleteOriginal) {
            std::stringstream stream;
            for (size_t j = 0; j < listOfGeoIds.size() - 1; j++) {
                stream << listOfGeoIds[j] << ",";
            }
            stream << listOfGeoIds[listOfGeoIds.size() - 1];
            try {
                Gui::cmdAppObjectArgs(sketchgui->getObject(), "delGeometries([%s])", stream.str().c_str());
            }
            catch (const Base::Exception& e) {
                Base::Console().Error("%s\n", e.what());
            }
        }
    }

    int getPointSideOfVector(Base::Vector2d pointToCheck, Base::Vector2d separatingVector, Base::Vector2d pointOnVector) {
        Base::Vector2d secondPointOnVec = pointOnVector + separatingVector;
        double d = (pointToCheck.x - pointOnVector.x) * (secondPointOnVec.y - pointOnVector.y)
            - (pointToCheck.y - pointOnVector.y) * (secondPointOnVec.x - pointOnVector.x);
        if (abs(d) < Precision::Confusion()) {
            return 0;
        }
        else if (d < 0) {
            return -1;
        }
        else {
            return 1;
        }
    }

    bool getSnapPoint(Base::Vector2d& snapPoint) {
        int pointGeoId = GeoEnum::GeoUndef;
        Sketcher::PointPos pointPosId = Sketcher::PointPos::none;
        int VtId = getPreselectPoint();
        int CrsId = getPreselectCross();
        if (CrsId == 0) {
            pointGeoId = Sketcher::GeoEnum::RtPnt;
            pointPosId = Sketcher::PointPos::start;
        }
        else if (VtId >= 0) {
            sketchgui->getSketchObject()->getGeoVertexIndex(VtId, pointGeoId, pointPosId);
        }
        if (pointGeoId != GeoEnum::GeoUndef && pointGeoId < firstCurveCreated) {
            //don't want to snap to the point of a geometry which is being previewed!
            auto sk = static_cast<Sketcher::SketchObject*>(sketchgui->getObject());
            snapPoint.x = sk->getPoint(pointGeoId, pointPosId).x;
            snapPoint.y = sk->getPoint(pointGeoId, pointPosId).y;
            return true;
        }
        return false;
    }

    int indexInVec(std::vector<int> vec, int elem)
    {
        if (elem == GeoEnum::GeoUndef) {
            return GeoEnum::GeoUndef;
        }
        for (size_t i = 0; i < vec.size(); i++)
        {
            if (vec[i] == elem)
            {
                return i;
            }
        }
        return -1;
    }

    CoincidencePointPos checkForCoincidence(int GeoId1, int GeoId2) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        const std::vector< Sketcher::Constraint* >& vals = Obj->Constraints.getValues();
        CoincidencePointPos positions;
        positions.FirstGeoPos = Sketcher::PointPos::none;
        positions.SecondGeoPos = Sketcher::PointPos::none;
        positions.SecondCoincidenceFirstGeoPos = Sketcher::PointPos::none;
        positions.SecondCoincidenceSecondGeoPos = Sketcher::PointPos::none;
        bool firstCoincidenceFound = 0;
        for (std::vector< Sketcher::Constraint* >::const_iterator it = vals.begin(); it != vals.end(); ++it) {
            if ((*it)->Type == Sketcher::Coincident) {
                if ((*it)->First == GeoId1 && (*it)->FirstPos != Sketcher::PointPos::mid 
                    && (*it)->Second == GeoId2 && (*it)->SecondPos != Sketcher::PointPos::mid) {
                    if (!firstCoincidenceFound) {
                        positions.FirstGeoPos = (*it)->FirstPos;
                        positions.SecondGeoPos = (*it)->SecondPos;
                        firstCoincidenceFound = 1;
                    }
                    else {
                        positions.SecondCoincidenceFirstGeoPos = (*it)->FirstPos;
                        positions.SecondCoincidenceSecondGeoPos = (*it)->SecondPos;
                    }
                }
                else if ((*it)->First == GeoId2 && (*it)->FirstPos != Sketcher::PointPos::mid
                    && (*it)->Second == GeoId1 && (*it)->SecondPos != Sketcher::PointPos::mid) {
                    if (!firstCoincidenceFound) {
                        positions.FirstGeoPos = (*it)->SecondPos;
                        positions.SecondGeoPos = (*it)->FirstPos;
                        firstCoincidenceFound = 1;
                    }
                    else {
                        positions.SecondCoincidenceFirstGeoPos = (*it)->SecondPos;
                        positions.SecondCoincidenceSecondGeoPos = (*it)->FirstPos;
                    }
                }
            }
        }
        return positions;
    }

    int getSignForVecDirection(const Part::Geometry* geo, Sketcher::PointPos pointPos) {
        if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()
            || geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {
            const Part::GeomArcOfConic* arcOfCircle = static_cast<const Part::GeomArcOfConic*>(geo);
            double totalAngle, startAngle, endAngle;
            arcOfCircle->getRange(startAngle, endAngle, true);
            totalAngle = endAngle - startAngle;
            if (pointPos == Sketcher::PointPos::start) {
                if (totalAngle > 0)
                    return -1;
                else
                    return 1;
            }
            else if (pointPos == Sketcher::PointPos::end) {
                if (totalAngle > 0)
                    return 1;
                else
                    return -1;
            }
        }
        return 1;
    }

    Base::Vector2d vec3dTo2d(Base::Vector3d pointToProcess) {
        Base::Vector2d pointToReturn;
        pointToReturn.x = pointToProcess.x;
        pointToReturn.y = pointToProcess.y;
        return pointToReturn;
    }

    void restartCommand(const char* cstrName) {
        Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
        Gui::Command::abortCommand();
        Obj->solve(true);
        sketchgui->draw(false, false); // Redraw
        Gui::Command::openCommand(cstrName);
    }
};

DEF_STD_CMD_A(CmdSketcherOffset)

CmdSketcherOffset::CmdSketcherOffset()
    : Command("Sketcher_Offset")
{
    sAppModule = "Sketcher";
    sGroup = "Sketcher";
    sMenuText = QT_TR_NOOP("Offset geometries");
    sToolTipText = QT_TR_NOOP("Offset selected geometries.");
    sWhatsThis = "Sketcher_Offset";
    sStatusTip = sToolTipText;
    sPixmap = "Sketcher_Offset";
    sAccel = "S";
    eType = ForEdit;
}

void CmdSketcherOffset::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    std::vector<int> listOfGeoIds = {};

    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
            QObject::tr("Wrong selection"),
            QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string>& SubNames = selection[0].getSubNames();
    if (!SubNames.empty()) {
        Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

        for (std::vector<std::string>::const_iterator it = SubNames.begin(); it != SubNames.end(); ++it) {
            // only handle non-external edges
            if (it->size() > 4 && it->substr(0, 4) == "Edge") {
                int geoId = std::atoi(it->substr(4, 4000).c_str()) - 1;
                if (geoId >= 0) {
                    listOfGeoIds.push_back(geoId);
                }
            }
            else if (it->size() > 6 && it->substr(0, 6) == "Vertex") {
                // only if it is a GeomPoint
                int VtId = std::atoi(it->substr(6, 4000).c_str()) - 1;
                int geoId;
                Sketcher::PointPos PosId;
                Obj->getGeoVertexIndex(VtId, geoId, PosId);
                if (Obj->getGeometry(geoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                    if (geoId >= 0) {
                        listOfGeoIds.push_back(geoId);
                    }
                }
            }
        }
    }

    getSelection().clearSelection();

    ActivateAcceleratorHandler(getActiveGuiDocument(), new DrawSketchHandlerOffset(listOfGeoIds));
}

bool CmdSketcherOffset::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherDeleteAllGeometry)

CmdSketcherDeleteAllGeometry::CmdSketcherDeleteAllGeometry()
    :Command("Sketcher_DeleteAllGeometry")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Delete all geometry");
    sToolTipText    = QT_TR_NOOP("Delete all geometry and constraints in the current sketch, "
                                 "with the exception of external geometry");
    sWhatsThis      = "Sketcher_DeleteAllGeometry";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_DeleteGeometry";
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherDeleteAllGeometry::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    int ret = QMessageBox::question(Gui::getMainWindow(), QObject::tr("Delete All Geometry"),
                                    QObject::tr("Are you really sure you want to delete all geometry and constraints?"),
                                    QMessageBox::Yes, QMessageBox::Cancel);
    // use an equality constraint
    if (ret == QMessageBox::Yes) {
        getSelection().clearSelection();
        Gui::Document * doc= getActiveGuiDocument();
        ReleaseHandler(doc);
        SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
        Sketcher::SketchObject* Obj= vp->getSketchObject();

        try {
            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Delete all geometry"));
            Gui::cmdAppObjectArgs(Obj, "deleteAllGeometry()");
            Gui::Command::commitCommand();
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Failed to delete all geometry: %s\n", e.what());
            Gui::Command::abortCommand();
        }

        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher");
        bool autoRecompute = hGrp->GetBool("AutoRecompute", false);

        if (autoRecompute)
            Gui::Command::updateActive();
        else
            Obj->solve();
    }
    else if (ret == QMessageBox::Cancel) {
        // do nothing
        return;
    }
}

bool CmdSketcherDeleteAllGeometry::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================

DEF_STD_CMD_A(CmdSketcherDeleteAllConstraints)

CmdSketcherDeleteAllConstraints::CmdSketcherDeleteAllConstraints()
    :Command("Sketcher_DeleteAllConstraints")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Delete all constraints");
    sToolTipText    = QT_TR_NOOP("Delete all constraints in the sketch");
    sWhatsThis      = "Sketcher_DeleteAllConstraints";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_DeleteConstraints";
    sAccel          = "";
    eType           = ForEdit;
}

void CmdSketcherDeleteAllConstraints::activated(int iMsg)
{
    Q_UNUSED(iMsg);

    int ret = QMessageBox::question(Gui::getMainWindow(), QObject::tr("Delete All Constraints"),
                                    QObject::tr("Are you really sure you want to delete all the constraints?"),
                                    QMessageBox::Yes, QMessageBox::Cancel);

    if (ret == QMessageBox::Yes) {
        getSelection().clearSelection();
        Gui::Document * doc= getActiveGuiDocument();
        ReleaseHandler(doc);
        SketcherGui::ViewProviderSketch* vp = static_cast<SketcherGui::ViewProviderSketch*>(doc->getInEdit());
        Sketcher::SketchObject* Obj= vp->getSketchObject();

        try {
            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Delete All Constraints"));
            Gui::cmdAppObjectArgs(Obj, "deleteAllConstraints()");
            Gui::Command::commitCommand();
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Failed to delete All Constraints: %s\n", e.what());
            Gui::Command::abortCommand();
        }

        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher");
        bool autoRecompute = hGrp->GetBool("AutoRecompute",false);

        if (autoRecompute)
            Gui::Command::updateActive();
        else
            Obj->solve();
    }
    else if (ret == QMessageBox::Cancel) {
        // do nothing
        return;
    }

}

bool CmdSketcherDeleteAllConstraints::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), false);
}

// ================================================================================


DEF_STD_CMD_A(CmdSketcherRemoveAxesAlignment)

CmdSketcherRemoveAxesAlignment::CmdSketcherRemoveAxesAlignment()
    :Command("Sketcher_RemoveAxesAlignment")
{
    sAppModule      = "Sketcher";
    sGroup          = "Sketcher";
    sMenuText       = QT_TR_NOOP("Remove axes alignment");
    sToolTipText    = QT_TR_NOOP("Modifies constraints to remove axes alignment while trying to preserve the constraint relationship of the selection");
    sWhatsThis      = "Sketcher_RemoveAxesAlignment";
    sStatusTip      = sToolTipText;
    sPixmap         = "Sketcher_RemoveAxesAlignment";
    sAccel          = "Z, R";
    eType           = ForEdit;
}

void CmdSketcherRemoveAxesAlignment::activated(int iMsg)
{
    Q_UNUSED(iMsg);
    // get the selection
    std::vector<Gui::SelectionObject> selection;
    selection = getSelection().getSelectionEx(0, Sketcher::SketchObject::getClassTypeId());

    // only one sketch with its subelements are allowed to be selected
    if (selection.size() != 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    // get the needed lists and objects
    const std::vector<std::string> &SubNames = selection[0].getSubNames();
    if (SubNames.empty()) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Select elements from a single sketch."));
        return;
    }

    Sketcher::SketchObject* Obj = static_cast<Sketcher::SketchObject*>(selection[0].getObject());

    getSelection().clearSelection();

    int LastGeoId = 0;

    // create python command with list of elements
    std::stringstream stream;
    int geoids = 0;

    for (std::vector<std::string>::const_iterator it=SubNames.begin(); it != SubNames.end(); ++it) {
        // only handle non-external edges
        if (it->size() > 4 && it->substr(0,4) == "Edge") {
            LastGeoId = std::atoi(it->substr(4,4000).c_str()) - 1;

            // lines to copy
            if (LastGeoId >= 0) {
                geoids++;
                stream << LastGeoId << ",";
            }
        }
        else if (it->size() > 6 && it->substr(0,6) == "Vertex") {
            // only if it is a GeomPoint
            int VtId = std::atoi(it->substr(6,4000).c_str()) - 1;
            int GeoId;
            Sketcher::PointPos PosId;
            Obj->getGeoVertexIndex(VtId, GeoId, PosId);
            if (Obj->getGeometry(GeoId)->getTypeId() == Part::GeomPoint::getClassTypeId()) {
                LastGeoId = GeoId;
                // points to copy
                if (LastGeoId >= 0) {
                    geoids++;
                    stream << LastGeoId << ",";
                }
            }
        }
    }

    if (geoids < 1) {
        QMessageBox::warning(Gui::getMainWindow(),
                             QObject::tr("Wrong selection"),
                             QObject::tr("Removal of axes alignment requires at least one selected non-external geometric element"));
        return;
    }

    std::string geoIdList = stream.str();

    // remove the last added comma and brackets to make the python list
    int index = geoIdList.rfind(',');
    geoIdList.resize(index);
    geoIdList.insert(0, 1, '[');
    geoIdList.append(1, ']');

    Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Remove Axes Alignment"));

    try {
        Gui::cmdAppObjectArgs(  Obj,
                                "removeAxesAlignment(%s)",
                                geoIdList.c_str());
        Gui::Command::commitCommand();
    }
    catch (const Base::Exception& e) {
        Base::Console().Error("%s\n", e.what());
        Gui::Command::abortCommand();
    }

    tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(Obj));

}

bool CmdSketcherRemoveAxesAlignment::isActive(void)
{
    return isSketcherAcceleratorActive(getActiveGuiDocument(), true);
}

void CreateSketcherCommandsConstraintAccel(void)
{
    Gui::CommandManager &rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdSketcherCloseShape());
    rcCmdMgr.addCommand(new CmdSketcherConnect());
    rcCmdMgr.addCommand(new CmdSketcherSelectConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectOrigin());
    rcCmdMgr.addCommand(new CmdSketcherSelectVerticalAxis());
    rcCmdMgr.addCommand(new CmdSketcherSelectHorizontalAxis());
    rcCmdMgr.addCommand(new CmdSketcherSelectRedundantConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectConflictingConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectMalformedConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectPartiallyRedundantConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectElementsAssociatedWithConstraints());
    rcCmdMgr.addCommand(new CmdSketcherSelectElementsWithDoFs());
    rcCmdMgr.addCommand(new CmdSketcherRestoreInternalAlignmentGeometry());
    rcCmdMgr.addCommand(new CmdSketcherSymmetry());
    rcCmdMgr.addCommand(new CmdSketcherCopy());
    rcCmdMgr.addCommand(new CmdSketcherClone());
    rcCmdMgr.addCommand(new CmdSketcherMove());
    rcCmdMgr.addCommand(new CmdSketcherCompCopy());
    rcCmdMgr.addCommand(new CmdSketcherRectangularArray());
    rcCmdMgr.addCommand(new CmdSketcherDeleteAllGeometry());
    rcCmdMgr.addCommand(new CmdSketcherDeleteAllConstraints());
    rcCmdMgr.addCommand(new CmdSketcherRemoveAxesAlignment());
    rcCmdMgr.addCommand(new CmdSketcherRotate());
    rcCmdMgr.addCommand(new CmdSketcherScale());
    rcCmdMgr.addCommand(new CmdSketcherOffset());
}
