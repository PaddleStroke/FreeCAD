/***************************************************************************
 *   Copyright (c) 2022 Abdullah Tahiri <abdullah.tahiri.yo@gmail.com>     *
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


#ifndef SKETCHERGUI_DrawSketchHandlerPolygon_H
#define SKETCHERGUI_DrawSketchHandlerPolygon_H

#include "DrawSketchDefaultWidgetHandler.h"

#include "GeometryCreationMode.h"
#include "Utils.h"

namespace SketcherGui {

extern GeometryCreationMode geometryCreationMode; // defined in CommandCreateGeo.cpp

class DrawSketchHandlerPolygon;

using DrawSketchHandlerPolygonBase = DrawSketchDefaultWidgetHandler<  DrawSketchHandlerPolygon,
    StateMachines::TwoSeekEnd,
    /*PEditCurveSize =*/ 7,
    /*PAutoConstraintSize =*/ 2,
    /*WidgetParametersT =*/ WidgetParameters<5>,
    /*WidgetCheckboxesT =*/ WidgetCheckboxes<0>,
    /*WidgetComboboxesT =*/ WidgetComboboxes<0>>;

class DrawSketchHandlerPolygon : public DrawSketchHandlerPolygonBase
{
    friend DrawSketchHandlerPolygonBase;
public:

    DrawSketchHandlerPolygon(int corners = 6) :
        Corners(corners),
        AngleOfSeparation(2.0 * M_PI / static_cast<double>(Corners)),
        cos_v(cos(AngleOfSeparation)),
        sin_v(sin(AngleOfSeparation)){
            initialEditCurveSize = corners + 1;
            EditCurve.resize(initialEditCurveSize);
        }
    virtual ~DrawSketchHandlerPolygon() = default;

private:
    virtual void updateDataAndDrawToPosition(Base::Vector2d onSketchPos) override {
        switch (state()) {
        case SelectMode::SeekFirst:
        {
            drawPositionAtCursor(onSketchPos);
            centerPoint = onSketchPos;

            if (seekAutoConstraint(sugConstraints[0], onSketchPos, Base::Vector2d(0.f, 0.f))) {
                renderSuggestConstraintsCursor(sugConstraints[0]);
                return;
            }
        }
        break;
        case SelectMode::SeekSecond:
        {
            Base::Console().Error("EditCurve size: %f\n", EditCurve[0].x, EditCurve[0].y);
            EditCurve[0] = Base::Vector2d(onSketchPos.x, onSketchPos.y);
            EditCurve[Corners] = Base::Vector2d(onSketchPos.x, onSketchPos.y);

            Base::Vector2d dV = onSketchPos - centerPoint;
            double rx = dV.x;
            double ry = dV.y;
            for (int i = 1; i < static_cast<int>(Corners); i++) {
                const double old_rx = rx;
                rx = cos_v * rx - sin_v * ry;
                ry = cos_v * ry + sin_v * old_rx;
                EditCurve[i] = Base::Vector2d(centerPoint.x + rx, centerPoint.y + ry);
            }

            // Display radius for user
            const float radius = dV.Length();
            const float angle = (180.0 / M_PI) * atan2(dV.y, dV.x);

            SbString text;
            text.sprintf(" (%.1fR %.1fdeg)", radius, angle);
            setPositionText(onSketchPos, text);

            drawEdit(EditCurve);
            if (seekAutoConstraint(sugConstraints[1], onSketchPos, Base::Vector2d(0.f, 0.f))) {
                renderSuggestConstraintsCursor(sugConstraints[1]);
                return;
            }
        }
        break;
        default:
            break;
        }
    }

    virtual void executeCommands() override {
        unsetCursor();
        resetPositionText();
        Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Add polygon"));

        try {
            Gui::Command::doCommand(Gui::Command::Doc,
                "import ProfileLib.RegularPolygon\n"
                "ProfileLib.RegularPolygon.makeRegularPolygon(%s,%i,App.Vector(%f,%f,0),App.Vector(%f,%f,0),%s)",
                Gui::Command::getObjectCmd(sketchgui->getObject()).c_str(),
                Corners,
                centerPoint.x, centerPoint.y, EditCurve[0].x, EditCurve[0].y,
                geometryCreationMode == Construction ? "True" : "False");

            Gui::Command::commitCommand();

            tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject*>(sketchgui->getObject()));
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Failed to add hexagon: %s\n", e.what());
            Gui::Command::abortCommand();

            tryAutoRecompute(static_cast<Sketcher::SketchObject*>(sketchgui->getObject()));
        }
    }

    virtual void createAutoConstraints() override {
        // add auto constraints at the center of the polygon
        if (sugConstraints[0].size() > 0) {
            DrawSketchHandler::createAutoConstraints(sugConstraints[0], getHighestCurveIndex(), Sketcher::PointPos::mid);
            sugConstraints[0].clear();
        }

        // add auto constraints to the last side of the polygon
        if (sugConstraints[1].size() > 0) {
            DrawSketchHandler::createAutoConstraints(sugConstraints[1], getHighestCurveIndex() - 1, Sketcher::PointPos::end);
            sugConstraints[1].clear();
        }
    }

    virtual std::string getToolName() const override {
        return "DSH_Polygon";
    }

    virtual QString getCrosshairCursorSVGName() const override {
        return QString::fromLatin1("Sketcher_Pointer_Regular_Polygon");
    }

private:
    unsigned int Corners;
    Base::Vector2d centerPoint, firstPoint, secondPoint;
    double AngleOfSeparation, cos_v, sin_v;
};

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::configureToolWidget() {

    if(!init) { // Code to be executed only upon initialisation
        toolWidget->configureParameterInitialValue(WParameter::Fifth, dHandler->Corners);
    }

    toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_polygon", "x of center"));
    toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_polygon", "y of center"));
    toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_polygon", "Radius"));
    toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("ToolWidgetManager_p4", "Angle"));
    toolWidget->configureParameterUnit(WParameter::Fourth, Base::Unit::Angle);
    toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("ToolWidgetManager_p4", "Side number"));
    toolWidget->configureParameterUnit(WParameter::Fifth, Base::Unit());
}

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::adaptDrawingToParameterChange(int parameterindex, double value) {
    switch (parameterindex) {
        case WParameter::First:
            dHandler->centerPoint.x = value;
            break;
        case WParameter::Second:
            dHandler->centerPoint.y = value;
            break;
        case WParameter::Fifth: {
            dHandler->Corners = std::max(3, static_cast<int>(value));
            dHandler->AngleOfSeparation = 2.0 * M_PI / static_cast<double>(dHandler->Corners);
            dHandler->cos_v = cos(dHandler->AngleOfSeparation);
            dHandler->sin_v = sin(dHandler->AngleOfSeparation);
            dHandler->EditCurve.clear();
            dHandler->initialEditCurveSize = dHandler->Corners + 1;
            dHandler->EditCurve.resize(dHandler->initialEditCurveSize);
        }
            break;
    }
}

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::doEnforceWidgetParameters(Base::Vector2d& onSketchPos) {

    switch (handler->state()) {
    case SelectMode::SeekFirst:
    {
        if (toolWidget->isParameterSet(WParameter::First))
            onSketchPos.x = toolWidget->getParameter(WParameter::First);

        if (toolWidget->isParameterSet(WParameter::Second))
            onSketchPos.y = toolWidget->getParameter(WParameter::Second);
    }
    break;
    case SelectMode::SeekSecond:
    {
        double length = (onSketchPos - dHandler->centerPoint).Length();
        if (toolWidget->isParameterSet(WParameter::Third)) {
            double radius = toolWidget->getParameter(WParameter::Third);
            if (length != 0.) {
                onSketchPos.x = dHandler->centerPoint.x + (onSketchPos.x - dHandler->centerPoint.x) * radius / length;
                onSketchPos.y = dHandler->centerPoint.y + (onSketchPos.y - dHandler->centerPoint.y) * radius / length;
            }
        }
        if (toolWidget->isParameterSet(WParameter::Fourth)) {
            double angle = toolWidget->getParameter(WParameter::Fourth);
            onSketchPos.x = dHandler->centerPoint.x + cos(angle * M_PI / 180) * length;
            onSketchPos.y = dHandler->centerPoint.y + sin(angle * M_PI / 180) * length;
        }
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::adaptWidgetParameters(Base::Vector2d onSketchPos) {
    switch (handler->state()) {
    case SelectMode::SeekFirst:
    {
        if (!toolWidget->isParameterSet(WParameter::First))
            toolWidget->updateVisualValue(WParameter::First, onSketchPos.x);

        if (!toolWidget->isParameterSet(WParameter::Second))
            toolWidget->updateVisualValue(WParameter::Second, onSketchPos.y);
    }
    break;
    case SelectMode::SeekSecond:
    {

        if (!toolWidget->isParameterSet(WParameter::Third))
            toolWidget->updateVisualValue(WParameter::Third, (onSketchPos - dHandler->centerPoint).Length());

        if (!toolWidget->isParameterSet(WParameter::Fourth))
            toolWidget->updateVisualValue(WParameter::Fourth, (onSketchPos - dHandler->centerPoint).Angle() * 180 / M_PI, Base::Unit::Angle);
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::doChangeDrawSketchHandlerMode() {
    switch (handler->state()) {
    case SelectMode::SeekFirst:
    {
        if (toolWidget->isParameterSet(WParameter::First) &&
            toolWidget->isParameterSet(WParameter::Second)) {

            handler->setState(SelectMode::SeekSecond);

            handler->updateDataAndDrawToPosition(prevCursorPosition);
        }
    }
    break;
    case SelectMode::SeekSecond:
    {
        if (toolWidget->isParameterSet(WParameter::Third) ||
            toolWidget->isParameterSet(WParameter::Fourth)) {

            handler->updateDataAndDrawToPosition(prevCursorPosition);

            if (toolWidget->isParameterSet(WParameter::Third) &&
                toolWidget->isParameterSet(WParameter::Fourth) &&
                toolWidget->isParameterSet(WParameter::Fifth)) {

                handler->setState(SelectMode::End);
                handler->finish();
            }
        }
    }
    break;
    default:
        break;
    }

}

template <> void DrawSketchHandlerPolygonBase::ToolWidgetManager::addConstraints() {
    int lastCurve = handler->getHighestCurveIndex();

    auto x0 = toolWidget->getParameter(WParameter::First);
    auto y0 = toolWidget->getParameter(WParameter::Second);
    auto radius = toolWidget->getParameter(WParameter::Third);

    auto x0set = toolWidget->isParameterSet(WParameter::First);
    auto y0set = toolWidget->isParameterSet(WParameter::Second);
    auto radiusSet = toolWidget->isParameterSet(WParameter::Third);

    using namespace Sketcher;

    if (x0set && y0set && x0 == 0. && y0 == 0.) {
        ConstraintToAttachment(GeoElementId(lastCurve, PointPos::mid), GeoElementId::RtPnt,
            x0, handler->sketchgui->getObject());
    }
    else {
        if (x0set)
            ConstraintToAttachment(GeoElementId(lastCurve, PointPos::mid), GeoElementId::VAxis,
                x0, handler->sketchgui->getObject());

        if (y0set)
            ConstraintToAttachment(GeoElementId(lastCurve, PointPos::mid), GeoElementId::HAxis,
                y0, handler->sketchgui->getObject());
    }

    if (radiusSet)
        Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Radius',%d,%f)) ",
            lastCurve, radius);
}


} // namespace SketcherGui


#endif // SKETCHERGUI_DrawSketchHandlerPolygon_H

