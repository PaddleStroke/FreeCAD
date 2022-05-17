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


#ifndef SKETCHERGUI_DrawSketchHandlerCircle_H
#define SKETCHERGUI_DrawSketchHandlerCircle_H

#include "DrawSketchDefaultWidgetHandler.h"

#include "GeometryCreationMode.h"
#include "Utils.h"

#include "CircleEllipseConstructionMethod.h"

namespace SketcherGui {

extern GeometryCreationMode geometryCreationMode; // defined in CommandCreateGeo.cpp

class DrawSketchHandlerCircle;

using DrawSketchHandlerCircleBase = DrawSketchDefaultWidgetHandler<  DrawSketchHandlerCircle,
    StateMachines::ThreeSeekEnd,
    /*PEditCurveSize =*/ 0,
    /*PAutoConstraintSize =*/ 3,
    /*WidgetParametersT =*/WidgetParameters<3, 6>,
    /*WidgetCheckboxesT =*/WidgetCheckboxes<0, 0>,
    /*WidgetComboboxesT =*/WidgetComboboxes<1, 1>,
    ConstructionMethods::CircleEllipseConstructionMethod,
    /*bool PFirstComboboxIsConstructionMethod =*/ true>;

class DrawSketchHandlerCircle : public DrawSketchHandlerCircleBase
{
    friend DrawSketchHandlerCircleBase;

public:
    DrawSketchHandlerCircle(ConstructionMethod constrMethod = ConstructionMethod::Center) :
        DrawSketchHandlerCircleBase(constrMethod) {}
    virtual ~DrawSketchHandlerCircle() = default;

private:
    virtual void updateDataAndDrawToPosition(Base::Vector2d onSketchPos) override {
        switch (state()) {
        case SelectMode::SeekFirst:
        {
            drawPositionAtCursor(onSketchPos);
            if (constructionMethod() == ConstructionMethod::Center) {
                centerPoint = onSketchPos;
            }
            else {
                firstPoint = onSketchPos;
            }

            if (seekAutoConstraint(sugConstraints[0], onSketchPos, Base::Vector2d(0.f, 0.f))) {
                renderSuggestConstraintsCursor(sugConstraints[0]);
                return;
            }
        }
        break;
        case SelectMode::SeekSecond:
        {
            if (constructionMethod() == ConstructionMethod::ThreeRim) {
                centerPoint = (onSketchPos - firstPoint) / 2 + firstPoint;
                secondPoint = onSketchPos;
            }

            radius = (onSketchPos - centerPoint).Length();

            std::vector<Part::Geometry*> geometriesToAdd;
            Part::GeomCircle* circle = new Part::GeomCircle();
            circle->setRadius(radius);
            circle->setCenter(Base::Vector3d(centerPoint.x, centerPoint.y, 0.));
            geometriesToAdd.push_back(circle);
            drawEdit(geometriesToAdd);

            SbString text;
            setPositionText(onSketchPos, text);
            if (constructionMethod() == ConstructionMethod::Center) {
                text.sprintf(" (%.1fR)", radius);

                if (seekAutoConstraint(sugConstraints[1], onSketchPos, onSketchPos - centerPoint, AutoConstraint::CURVE)) {
                    renderSuggestConstraintsCursor(sugConstraints[1]);
                    return;
                }
            }
            else {
                double lineAngle = GetPointAngle(centerPoint, onSketchPos);
                // This lineAngle will report counter-clockwise from +X, not relatively
                text.sprintf(" (%.1fR,%.1fdeg)", (float)radius, (float)lineAngle * 180 / M_PI);

                if (seekAutoConstraint(sugConstraints[1], onSketchPos, Base::Vector2d(0.f, 0.f), AutoConstraint::CURVE)) {
                    renderSuggestConstraintsCursor(sugConstraints[1]);
                    return;
                }
            }
        }
        break;
        case SelectMode::SeekThird:
        {
            try
            {
                centerPoint = Part::Geom2dCircle::getCircleCenter(firstPoint, secondPoint, onSketchPos);
                radius = (onSketchPos - centerPoint).Length();

                std::vector<Part::Geometry*> geometriesToAdd;
                Part::GeomCircle* circle = new Part::GeomCircle();
                circle->setRadius(radius);
                circle->setCenter(Base::Vector3d(centerPoint.x, centerPoint.y, 0.));
                geometriesToAdd.push_back(circle);
                drawEdit(geometriesToAdd);

                double lineAngle = GetPointAngle(centerPoint, onSketchPos);
                // This lineAngle will report counter-clockwise from +X, not relatively
                SbString text;
                text.sprintf(" (%.1fR,%.1fdeg)", (float)radius, (float)lineAngle * 180 / M_PI);
                setPositionText(onSketchPos, text);

                if (seekAutoConstraint(sugConstraints[2], onSketchPos, Base::Vector2d(0.f, 0.f), AutoConstraint::CURVE)) {
                    renderSuggestConstraintsCursor(sugConstraints[2]);
                    return;
                }
            }
            catch (Base::ValueError& e) {
                e.ReportException();
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

        try {
            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Add sketch circle"));
            Gui::cmdAppObjectArgs(sketchgui->getObject(), "addGeometry(Part.Circle"
                "(App.Vector(%f,%f,0),App.Vector(0,0,1),%f),%s)",
                centerPoint.x, centerPoint.y,
                radius,
                geometryCreationMode == Construction ? "True" : "False");

            Gui::Command::commitCommand();
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Failed to add circle: %s\n", e.what());
            Gui::Command::abortCommand();
        }
    }

    virtual void createAutoConstraints() override {
        if (constructionMethod() == ConstructionMethod::Center) {
            // add auto constraints for the center point
            if (!sugConstraints[0].empty()) {
                DrawSketchHandler::createAutoConstraints(sugConstraints[0], getHighestCurveIndex(), Sketcher::PointPos::mid);
                sugConstraints[0].clear();
            }

            // add suggested constraints for circumference
            if (!sugConstraints[1].empty()) {
                DrawSketchHandler::createAutoConstraints(sugConstraints[1], getHighestCurveIndex(), Sketcher::PointPos::none);
                sugConstraints[1].clear();
            }
        }
        else {
            // Auto Constraint first picked point
            if (sugConstraints[0].size() > 0) {
                DrawSketchHandler::createAutoConstraints(sugConstraints[0], getHighestCurveIndex(), Sketcher::PointPos::none);
                sugConstraints[0].clear();
            }

            // Auto Constraint second picked point
            if (sugConstraints[1].size() > 0) {
                DrawSketchHandler::createAutoConstraints(sugConstraints[1], getHighestCurveIndex(), Sketcher::PointPos::none);
                sugConstraints[1].clear();
            }

            // Auto Constraint third picked point
            if (sugConstraints[2].size() > 0) {
                DrawSketchHandler::createAutoConstraints(sugConstraints[2], getHighestCurveIndex(), Sketcher::PointPos::none);
                sugConstraints[2].clear();
            }
        }
    }

    virtual std::string getToolName() const override {
        return "DSH_Circle";
    }

    virtual QString getCrosshairCursorSVGName() const override {
        if (constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center)
            return QString::fromLatin1("Sketcher_Pointer_Create_Circle");
        else // constructionMethod == DrawSketchHandlerCircle::ConstructionMethod::ThreeRim
            return QString::fromLatin1("Sketcher_Pointer_Create_3PointCircle");
    }

    //reimplement because circle is 2 steps while 3rims is 3 steps
    virtual void onButtonPressed(Base::Vector2d onSketchPos) override {
        this->updateDataAndDrawToPosition(onSketchPos);
        if (state() == SelectMode::SeekSecond && constructionMethod() == ConstructionMethod::Center) {
            setState(SelectMode::End);
        }
        else {
            this->moveToNextMode();
        }
    }

private:
    Base::Vector2d centerPoint, firstPoint, secondPoint;
    double radius;
};

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::configureToolWidget() {

    if(!init) { // Code to be executed only upon initialisation
        QStringList names = {QStringLiteral("Center"), QStringLiteral("3 rim points")};
        toolWidget->setComboboxElements(WCombobox::FirstCombo, names);

        syncConstructionMethodComboboxToHandler(); // in case the DSH was called with a specific construction method
    }

    if (dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_circle", "x of center"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_circle", "y of center"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_circle", "Radius"));
    }
    else {
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("ToolWidgetManager_p1", "x of 1st point"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("ToolWidgetManager_p2", "y of 1st point"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("ToolWidgetManager_p3", "x of 2nd point"));
        toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("ToolWidgetManager_p4", "y of 2nd point"));
        toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("ToolWidgetManager_p5", "x of 3rd point"));
        toolWidget->setParameterLabel(WParameter::Sixth, QApplication::translate("ToolWidgetManager_p6", "y of 3rd point"));
    }
}

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::adaptDrawingToParameterChange(int parameterindex, double value) {
    if (dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {
        switch (parameterindex) {
        case WParameter::First:
            dHandler->centerPoint.x = value;
            break;
        case WParameter::Second:
            dHandler->centerPoint.y = value;
            break;
        }
    }
    else { //if (constructionMethod == ConstructionMethod::ThreeRim)
        switch (parameterindex) {
        case WParameter::First:
            dHandler->firstPoint.x = value;
            break;
        case WParameter::Second:
            dHandler->firstPoint.y = value;
            break;
        case WParameter::Third:
            dHandler->secondPoint.x = value;
            break;
        case WParameter::Fourth:
            dHandler->secondPoint.y = value;
            break;
        }
    }
}

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::doEnforceWidgetParameters(Base::Vector2d& onSketchPos) {

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
        if (dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {
            if (toolWidget->isParameterSet(WParameter::Third)) {
                double radius = toolWidget->getParameter(WParameter::Third);
                onSketchPos.x = dHandler->centerPoint.x + radius;
                onSketchPos.y = dHandler->centerPoint.y;
            }
        }
        else {
            if (toolWidget->isParameterSet(WParameter::Third))
                onSketchPos.x = toolWidget->getParameter(WParameter::Third);

            if (toolWidget->isParameterSet(WParameter::Fourth))
                onSketchPos.y = toolWidget->getParameter(WParameter::Fourth);
        }
    }
    break;
    case SelectMode::SeekThird:
    { //3 rims only
        if (toolWidget->isParameterSet(WParameter::Fifth))
            onSketchPos.x = toolWidget->getParameter(WParameter::Fifth);

        if (toolWidget->isParameterSet(WParameter::Sixth))
            onSketchPos.y = toolWidget->getParameter(WParameter::Sixth);
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::adaptWidgetParameters(Base::Vector2d onSketchPos) {
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
        if (dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {
            if (!toolWidget->isParameterSet(WParameter::Third))
                toolWidget->updateVisualValue(WParameter::Third,dHandler->radius);
        }
        else {
            if (!toolWidget->isParameterSet(WParameter::Third))
                toolWidget->updateVisualValue(WParameter::Third, onSketchPos.x);

            if (!toolWidget->isParameterSet(WParameter::Fourth))
                toolWidget->updateVisualValue(WParameter::Fourth, onSketchPos.y);
        }
    }
    break;
    case SelectMode::SeekThird:
    { //3 rims only
        if (!toolWidget->isParameterSet(WParameter::Fifth))
            toolWidget->updateVisualValue(WParameter::Fifth, onSketchPos.x);

        if (!toolWidget->isParameterSet(WParameter::Sixth))
            toolWidget->updateVisualValue(WParameter::Sixth, onSketchPos.y);
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::doChangeDrawSketchHandlerMode() {
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
                dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {

                handler->setState(SelectMode::End);
                handler->finish();
            }
            else if (toolWidget->isParameterSet(WParameter::Third) &&
                toolWidget->isParameterSet(WParameter::Fourth) &&
                dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::ThreeRim) {

                handler->setState(SelectMode::SeekThird);

            }
        }
    }
    break;
    case SelectMode::SeekThird:
    {
        if (toolWidget->isParameterSet(WParameter::Fifth) ||
            toolWidget->isParameterSet(WParameter::Sixth)) {

            handler->updateDataAndDrawToPosition(prevCursorPosition);

            if (toolWidget->isParameterSet(WParameter::Fifth) &&
                toolWidget->isParameterSet(WParameter::Sixth)) {

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

template <> void DrawSketchHandlerCircleBase::ToolWidgetManager::addConstraints() {
    if (dHandler->constructionMethod() == DrawSketchHandlerCircle::ConstructionMethod::Center) {
        int firstCurve = handler->getHighestCurveIndex();

        auto x0 = toolWidget->getParameter(WParameter::First);
        auto y0 = toolWidget->getParameter(WParameter::Second);

        auto x0set = toolWidget->isParameterSet(WParameter::First);
        auto y0set = toolWidget->isParameterSet(WParameter::Second);
        auto radiusSet = toolWidget->isParameterSet(WParameter::Third);

        using namespace Sketcher;

        if (x0set && y0set && x0 == 0. && y0 == 0.) {
            ConstraintToAttachment(GeoElementId(firstCurve, PointPos::mid), GeoElementId::RtPnt,
                x0, handler->sketchgui->getObject());
        }
        else {
            if (x0set)
                ConstraintToAttachment(GeoElementId(firstCurve, PointPos::mid), GeoElementId::VAxis,
                    x0, handler->sketchgui->getObject());

            if (y0set)
                ConstraintToAttachment(GeoElementId(firstCurve, PointPos::mid), GeoElementId::HAxis,
                    y0, handler->sketchgui->getObject());
        }

        if (radiusSet)
            Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Radius',%d,%f)) ",
                firstCurve, dHandler->radius);
    }
    //No constraint possible for 3 rim circle.
}

} // namespace SketcherGui


#endif // SKETCHERGUI_DrawSketchHandlerCircle_H

