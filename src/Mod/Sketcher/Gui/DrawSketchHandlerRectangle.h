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


#ifndef SKETCHERGUI_DrawSketchHandlerRectangle_H
#define SKETCHERGUI_DrawSketchHandlerRectangle_H


#include "GeometryCreationMode.h"
#include "Utils.h"

namespace SketcherGui {

extern GeometryCreationMode geometryCreationMode; // defined in CommandCreateGeo.cpp

class DrawSketchHandlerRectangle;

namespace ConstructionMethods {

enum class RectangleConstructionMethod {
    Diagonal,
    CenterAndCorner,
    ThreePoints,
    CenterAnd3Points,
    End // Must be the last one
};

}

using DrawSketchHandlerRectangleBase = DrawSketchDefaultWidgetHandler<  DrawSketchHandlerRectangle,
                                                                        StateMachines::FiveSeekEnd,
                                                                        /*PEditCurveSize =*/ 5,
                                                                        /*PAutoConstraintSize =*/ 3,
                                                                        /*WidgetParametersT =*/WidgetParameters<6, 6, 8, 8>,
                                                                        /*WidgetCheckboxesT =*/WidgetCheckboxes<2, 2, 2, 2>,
                                                                        /*WidgetComboboxesT =*/WidgetComboboxes<0, 0, 0, 0>,
                                                                        ConstructionMethods::RectangleConstructionMethod,
                                                                        /*bool PFirstComboboxIsConstructionMethod =*/ true>;

class DrawSketchHandlerRectangle: public DrawSketchHandlerRectangleBase
{
    friend DrawSketchHandlerRectangleBase; // allow DrawSketchHandlerRectangleBase specialisations access DrawSketchHandlerRectangle private members

public:

    DrawSketchHandlerRectangle(ConstructionMethod constrMethod = ConstructionMethod::Diagonal, bool roundcorners = false, bool frame = false) :
        DrawSketchHandlerRectangleBase(constrMethod),
        roundCorners(roundcorners),
        makeFrame(frame),
        cornersReversed(false),
        thickness(0.) {}

    virtual ~DrawSketchHandlerRectangle() = default;

private:
    virtual void updateDataAndDrawToPosition(Base::Vector2d onSketchPos) override {
        switch(state()) {
            case SelectMode::SeekFirst:
            {
                drawPositionAtCursor(onSketchPos);

                if(constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::ThreePoints)
                    firstCorner = onSketchPos;
                else //(constructionMethod == ConstructionMethod::CenterAndCorner)
                    center = onSketchPos;

                if (seekAutoConstraint(sugConstraints[0], onSketchPos, Base::Vector2d(0.f,0.f))) {
                    renderSuggestConstraintsCursor(sugConstraints[0]);
                    return;
                }
            }
            break;
            case SelectMode::SeekSecond:
            {
                if(constructionMethod() == ConstructionMethod::Diagonal) {
                    drawDirectionAtCursor(onSketchPos, firstCorner);

                    thirdCorner = onSketchPos;
                    if (Base::sgn(thirdCorner.x - firstCorner.x) * Base::sgn(thirdCorner.y - firstCorner.y) > 0) {
                        secondCorner = Base::Vector2d(onSketchPos.x, firstCorner.y);
                        fourthCorner = Base::Vector2d(firstCorner.x, onSketchPos.y);
                    }
                    else {
                        fourthCorner = Base::Vector2d(onSketchPos.x, firstCorner.y);
                        secondCorner = Base::Vector2d(firstCorner.x, onSketchPos.y);
                    }
                    angle123 = M_PI / 2;
                    angle412 = M_PI / 2;
                }
                else if (constructionMethod() == ConstructionMethod::CenterAndCorner) {
                    drawDirectionAtCursor(onSketchPos, center);

                    firstCorner = center - (onSketchPos - center);
                    thirdCorner = onSketchPos;
                    if (Base::sgn(thirdCorner.x - firstCorner.x) * Base::sgn(thirdCorner.y - firstCorner.y) > 0) {
                        secondCorner = Base::Vector2d(onSketchPos.x, firstCorner.y);
                        fourthCorner = Base::Vector2d(firstCorner.x, onSketchPos.y);
                    }
                    else {
                        fourthCorner = Base::Vector2d(onSketchPos.x, firstCorner.y);
                        secondCorner = Base::Vector2d(firstCorner.x, onSketchPos.y);
                    }
                    angle123 = M_PI / 2;
                    angle412 = M_PI / 2;
                }
                else if (constructionMethod() == ConstructionMethod::ThreePoints) {
                    drawDirectionAtCursor(onSketchPos, firstCorner);

                    secondCorner = onSketchPos;
                    Base::Vector2d perpendicular;
                    perpendicular.x = -(secondCorner - firstCorner).y;
                    perpendicular.y = (secondCorner - firstCorner).x;
                    thirdCorner = secondCorner + perpendicular;
                    fourthCorner = firstCorner + perpendicular;
                    angle123 = M_PI / 2;
                    angle412 = M_PI / 2;
                    secondCornerInitial = secondCorner;
                    side = getPointSideOfVector(thirdCorner, secondCorner - firstCorner, firstCorner);
                }
                else {
                    drawDirectionAtCursor(onSketchPos, center);

                    firstCorner = onSketchPos;
                    thirdCorner = center - (onSketchPos - center);
                    Base::Vector2d perpendicular;
                    perpendicular.x = -(onSketchPos - center).y;
                    perpendicular.y = (onSketchPos - center).x;
                    secondCorner = center + perpendicular;
                    fourthCorner = center - perpendicular;
                    angle123 = M_PI / 2;
                    angle412 = M_PI / 2;
                    side = getPointSideOfVector(secondCorner, thirdCorner - firstCorner, firstCorner);
                }

                if (roundCorners) {
                    length = (secondCorner - firstCorner).Length();
                    width = (fourthCorner - firstCorner).Length();
                    radius = std::min(length, width) / 6;
                }
                else
                    radius = 0.;

                try {
                    CreateAndDrawShapeGeometry();
                }
                catch(const Base::ValueError &) {} // equal points while hovering raise an objection that can be safely ignored

                if (seekAutoConstraint(sugConstraints[1], onSketchPos, Base::Vector2d(0.0,0.0))) {
                    renderSuggestConstraintsCursor(sugConstraints[1]);
                    return;
                }
            }
            break;
            case SelectMode::SeekThird:
            {
                if (constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                    if (roundCorners)
                        calculateRadius(onSketchPos);
                    else
                        calculateThickness(onSketchPos); //This is the case of frame of normal rectangle.
                }
                else if (constructionMethod() == ConstructionMethod::ThreePoints) {
                    secondCorner = secondCornerInitial;
                    thirdCorner = onSketchPos;
                    if (side == getPointSideOfVector(thirdCorner, secondCorner - firstCorner, firstCorner)) {
                        fourthCorner = firstCorner + (thirdCorner - secondCorner); 
                        cornersReversed = false;
                    }
                    else {
                        fourthCorner = secondCorner;
                        secondCorner = firstCorner + (thirdCorner - fourthCorner);
                        cornersReversed = true;
                    }
                    Base::Vector2d a = firstCorner - secondCorner;
                    Base::Vector2d b = thirdCorner - secondCorner;
                    if(fabs((sqrt(a.x * a.x + a.y * a.y) * sqrt(b.x * b.x + b.y * b.y))) > Precision::Confusion())
                        angle123 = acos((a.x * b.x + a.y * b.y) / (sqrt(a.x * a.x + a.y * a.y) * sqrt(b.x * b.x + b.y * b.y)));
                    angle412 = M_PI - angle123;
                    if (roundCorners)
                        radius = std::min(length, width) / 6 * std::min(sqrt(1 - cos(angle412) * cos(angle412)), sqrt(1 - cos(angle123) * cos(angle123)));
                    else
                        radius = 0.;
                    
                    SbString text;
                    text.sprintf(" (%.1f Angle)", angle123 / M_PI * 180);
                    setPositionText(onSketchPos, text);
                }
                else {
                    secondCorner = onSketchPos;
                    fourthCorner = center - (onSketchPos - center);
                    cornersReversed = false;
                    if (side != getPointSideOfVector(secondCorner, thirdCorner - firstCorner, firstCorner)) {
                        fourthCorner = onSketchPos;
                        secondCorner = center - (onSketchPos - center);
                        cornersReversed = true;
                    }
                    Base::Vector2d a = fourthCorner - firstCorner;
                    Base::Vector2d b = secondCorner - firstCorner;
                    if (fabs((sqrt(a.x * a.x + a.y * a.y) * sqrt(b.x * b.x + b.y * b.y))) > Precision::Confusion())
                        angle412 = acos((a.x * b.x + a.y * b.y) / (sqrt(a.x * a.x + a.y * a.y) * sqrt(b.x * b.x + b.y * b.y)));
                    angle123 = M_PI - angle412;
                    if (roundCorners)
                        radius = std::min(length, width) / 6 * std::min(sqrt(1 - cos(angle412) * cos(angle412)), sqrt(1 - cos(angle123) * cos(angle123)));
                    else
                        radius = 0.;

                    SbString text;
                    text.sprintf(" (%.1f Angle)", angle412 / M_PI * 180);
                    setPositionText(onSketchPos, text);
                }

                try {
                    CreateAndDrawShapeGeometry();
                }
                catch (const Base::ValueError&) {} // equal points while hovering raise an objection that can be safely ignored

                if ((constructionMethod() == ConstructionMethod::ThreePoints || constructionMethod() == ConstructionMethod::CenterAnd3Points) 
                    && seekAutoConstraint(sugConstraints[2], onSketchPos, Base::Vector2d(0.0, 0.0))) {
                    renderSuggestConstraintsCursor(sugConstraints[2]);
                    return;
                }
            }
            break;
            case SelectMode::SeekFourth:
            {
                if (constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner)
                    calculateThickness(onSketchPos); //This is the case of frame of round corner rectangle.
                else {
                    if (roundCorners)
                        calculateRadius(onSketchPos);
                    else
                        calculateThickness(onSketchPos);
                }

                CreateAndDrawShapeGeometry();
            }
            break;
            case SelectMode::SeekFifth:
            {
                calculateThickness(onSketchPos);

                CreateAndDrawShapeGeometry();
            }
            break;
            default:
                break;
        }
    }

    virtual void executeCommands() override {
        try {
            firstCurve = getHighestCurveIndex() + 1;

            createShape(false);

            Gui::Command::openCommand(QT_TRANSLATE_NOOP("Command", "Add sketch box"));

            commandAddShapeGeometryAndConstraints();

            Gui::Command::commitCommand();
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Failed to add box: %s\n", e.what());
            Gui::Command::abortCommand();
            THROWM(Base::RuntimeError, "Tool execution aborted\n") // This prevents constraints from being applied on non existing geometry
        }

        thickness = 0.;
    }

    virtual void generateAutoConstraints() override {

        if(constructionMethod() == ConstructionMethod::Diagonal) {
            // add auto constraints at the start of the first side
            if (radius > Precision::Confusion()) {
                if (!sugConstraints[0].empty())
                    generateAutoConstraintsOnElement(sugConstraints[0], constructionPointOneId, Sketcher::PointPos::start);

                if (!sugConstraints[1].empty())
                    generateAutoConstraintsOnElement(sugConstraints[1], constructionPointTwoId, Sketcher::PointPos::start);
            }
            else {
                if (!sugConstraints[0].empty())
                    generateAutoConstraintsOnElement(sugConstraints[0], firstCurve, Sketcher::PointPos::start);

                if (!sugConstraints[1].empty())
                    generateAutoConstraintsOnElement(sugConstraints[1], firstCurve + 1, Sketcher::PointPos::end);
            }
        }
        else if (constructionMethod() == ConstructionMethod::CenterAndCorner) {
            // add auto constraints at center
            if (!sugConstraints[0].empty())
                generateAutoConstraintsOnElement(sugConstraints[0], centerPointId, Sketcher::PointPos::start);

            // add auto constraints for the line segment end
            if (!sugConstraints[1].empty()) {
                if (radius > Precision::Confusion())
                    generateAutoConstraintsOnElement(sugConstraints[1], constructionPointOneId, Sketcher::PointPos::start);
                else
                    generateAutoConstraintsOnElement(sugConstraints[1], firstCurve + 1, Sketcher::PointPos::end);
            }
        }
        else if (constructionMethod() == ConstructionMethod::ThreePoints) {
            if (radius > Precision::Confusion()) {
                if (!sugConstraints[0].empty())
                    generateAutoConstraintsOnElement(sugConstraints[0], constructionPointOneId, Sketcher::PointPos::start);

                if (!sugConstraints[1].empty())
                    generateAutoConstraintsOnElement(sugConstraints[1], constructionPointTwoId, Sketcher::PointPos::start);

                if (!sugConstraints[2].empty())
                    generateAutoConstraintsOnElement(sugConstraints[2], constructionPointThreeId, Sketcher::PointPos::start);
            }
            else {
                if (!sugConstraints[0].empty())
                    generateAutoConstraintsOnElement(sugConstraints[0], firstCurve, Sketcher::PointPos::start);

                if (!sugConstraints[1].empty()) {
                    if (!cornersReversed)
                        generateAutoConstraintsOnElement(sugConstraints[1], firstCurve + 1, Sketcher::PointPos::start);
                    else
                        generateAutoConstraintsOnElement(sugConstraints[1], firstCurve + 3, Sketcher::PointPos::start);
                }

                if (!sugConstraints[2].empty())
                    generateAutoConstraintsOnElement(sugConstraints[2], firstCurve + 2, Sketcher::PointPos::start);
            }
        }
        else if (constructionMethod() == ConstructionMethod::CenterAnd3Points) {
            // add auto constraints at center
            if (!sugConstraints[0].empty())
                generateAutoConstraintsOnElement(sugConstraints[0], centerPointId, Sketcher::PointPos::start);

            // add auto constraints for the line segment end
            if (radius > Precision::Confusion()) {
                if (!sugConstraints[1].empty())
                    generateAutoConstraintsOnElement(sugConstraints[1], constructionPointOneId, Sketcher::PointPos::start);

                if (!sugConstraints[2].empty())
                    generateAutoConstraintsOnElement(sugConstraints[2], constructionPointTwoId, Sketcher::PointPos::start);
            }
            else {
                if (!sugConstraints[1].empty())
                    generateAutoConstraintsOnElement(sugConstraints[1], firstCurve, Sketcher::PointPos::start);

                if (!sugConstraints[2].empty()) {
                    if (!cornersReversed)
                        generateAutoConstraintsOnElement(sugConstraints[2], firstCurve + 1, Sketcher::PointPos::start);
                    else
                        generateAutoConstraintsOnElement(sugConstraints[2], firstCurve + 3, Sketcher::PointPos::start);
                }
            }
        }

        // Ensure temporary autoconstraints do not generate a redundancy and that the geometry parameters are accurate
        // This is particularly important for adding widget mandated constraints.
        removeRedundantAutoConstraints();
    }

    virtual void createAutoConstraints() override {
        createGeneratedAutoConstraints(true);

        sugConstraints[0].clear();
        sugConstraints[1].clear();
    }

    virtual std::string getToolName() const override {
        return "DSH_Rectangle";
    }

    virtual QString getCrosshairCursorSVGName() const override {
        if(!roundCorners && !makeFrame) {
            if(constructionMethod() == ConstructionMethod::CenterAndCorner)
                return QString::fromLatin1("Sketcher_Pointer_Create_Box_Center");
            else
                return QString::fromLatin1("Sketcher_Pointer_Create_Box");
        }
        else if (roundCorners && !makeFrame) {
            if(constructionMethod() == ConstructionMethod::CenterAndCorner)
                return QString::fromLatin1("Sketcher_Pointer_Oblong_Center");
            else
                return QString::fromLatin1("Sketcher_Pointer_Oblong");
        }
        else if (!roundCorners && makeFrame) {
            if(constructionMethod() == ConstructionMethod::CenterAndCorner)
                return QString::fromLatin1("Sketcher_Pointer_Create_Frame_Center");
            else
                return QString::fromLatin1("Sketcher_Pointer_Create_Frame");
        }
        else { // both roundCorners and makeFrame
            if(constructionMethod() == ConstructionMethod::CenterAndCorner)
                return QString::fromLatin1("Sketcher_Pointer_Oblong_Frame_Center");
            else
                return QString::fromLatin1("Sketcher_Pointer_Oblong_Frame");
        }
    }

    //reimplement because if not radius then it's 2 steps
    virtual void onButtonPressed(Base::Vector2d onSketchPos) override {
        this->updateDataAndDrawToPosition(onSketchPos);
        if (constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner){
            if (state() == SelectMode::SeekSecond && !roundCorners && !makeFrame)
                setState(SelectMode::End);
            else if ((state() == SelectMode::SeekThird && roundCorners && !makeFrame) || (state() == SelectMode::SeekThird && !roundCorners && makeFrame))
                setState(SelectMode::End);
            else if (state() == SelectMode::SeekFourth)
                setState(SelectMode::End);
            else {
                this->moveToNextMode();
            }
        }
        else{
            if (state() == SelectMode::SeekThird && !roundCorners && !makeFrame)
                setState(SelectMode::End);
            else if ((state() == SelectMode::SeekFourth && roundCorners && !makeFrame) || (state() == SelectMode::SeekFourth && !roundCorners && makeFrame))
                setState(SelectMode::End);
            else
                this->moveToNextMode();
        }
    }

private:
    Base::Vector2d center, firstCorner, secondCorner, thirdCorner, fourthCorner, firstCornerFrame, secondCornerFrame, thirdCornerFrame, fourthCornerFrame, secondCornerInitial;
    Base::Vector3d arc1Center, arc2Center, arc3Center, arc4Center;
    bool roundCorners, makeFrame, cornersReversed;
    double radius, length, width, thickness, radiusFrame, angle, angle123, angle412;
    int firstCurve, constructionPointOneId, constructionPointTwoId, constructionPointThreeId, centerPointId, side;

    virtual void createShape(bool onlyeditoutline) override {

        ShapeGeometry.clear();

        Base::Vector2d vecL = secondCorner - firstCorner;
        Base::Vector2d vecW = fourthCorner - firstCorner;
        length = vecL.Length();
        width = vecW.Length();
        angle = vecL.Angle();
        if (length > Precision::Confusion() && width > Precision::Confusion() && fmod(fabs(angle123), M_PI) > Precision::Confusion() ){
            vecL = vecL / length;
            vecW = vecW / width;
            double end = angle - M_PI / 2;
            double L1 = radius;
            double L2 = radius;
            if (cos(angle123 / 2) != 1 && cos(angle412 / 2) != 1) {
                L1 = radius / sqrt(1 - cos(angle123 / 2) * cos(angle123 / 2));
                L2 = radius / sqrt(1 - cos(angle412 / 2) * cos(angle412 / 2));
            }

            addLineToShapeGeometry(v2dTo3d(firstCorner + vecL * L2 * cos(angle412 / 2)), v2dTo3d(secondCorner - vecL * L1 * cos(angle123 / 2)), geometryCreationMode);
            addLineToShapeGeometry(v2dTo3d(secondCorner + vecW * L1 * cos(angle123 / 2)), v2dTo3d(thirdCorner - vecW * L2 * cos(angle412 / 2)), geometryCreationMode);
            addLineToShapeGeometry(v2dTo3d(thirdCorner - vecL * L2 * cos(angle412 / 2)), v2dTo3d(fourthCorner + vecL * L1 * cos(angle123 / 2)), geometryCreationMode);
            addLineToShapeGeometry(v2dTo3d(fourthCorner - vecW * L1 * cos(angle123 / 2)), v2dTo3d(firstCorner + vecW * L2 * cos(angle412 / 2)), geometryCreationMode);

            if (roundCorners && radius > Precision::Confusion()) {
                //center points required later for special case of round corner frame with radiusFrame = 0.
                Base::Vector2d b1 = (vecL + vecW) / (vecL + vecW).Length();
                Base::Vector2d b2 = (vecL - vecW) / (vecL - vecW).Length();
                arc1Center = v2dTo3d(firstCorner + b1 * L2);
                arc2Center = v2dTo3d(secondCorner - b2 * L1);
                arc3Center = v2dTo3d(thirdCorner - b1 * L2);
                arc4Center = v2dTo3d(fourthCorner + b2 * L1);

                addArcToShapeGeometry(arc1Center, end - M_PI + angle412, end, radius, geometryCreationMode);
                addArcToShapeGeometry(arc2Center, end, end - M_PI - angle123, radius, geometryCreationMode);
                addArcToShapeGeometry(arc3Center, end + angle412, end - M_PI, radius, geometryCreationMode);
                addArcToShapeGeometry(arc4Center, end - M_PI, end - angle123, radius, geometryCreationMode);
            }

            if (makeFrame && state() != SelectMode::SeekSecond && fabs(thickness) > Precision::Confusion()) {
                if (radius < Precision::Confusion()) {
                    radiusFrame = 0.;
                }
                else {
                    radiusFrame = radius + thickness;
                    if (radiusFrame < 0.) {
                        radiusFrame = 0.;
                    }
                }

                Base::Vector2d vecLF = secondCornerFrame - firstCornerFrame;
                Base::Vector2d vecWF = fourthCornerFrame - firstCornerFrame;
                double lengthF = vecLF.Length();
                double widthF = vecWF.Length();

                double L1F = 0.;
                double L2F = 0.;
                if (radius > Precision::Confusion()) {
                    L1F = L1 * radiusFrame / radius;
                    L2F = L2 * radiusFrame / radius;
                }

                addLineToShapeGeometry(v2dTo3d(firstCornerFrame + vecLF / lengthF * L2F * cos(angle412 / 2)), v2dTo3d(secondCornerFrame - vecLF / lengthF * L1F * cos(angle123 / 2)), geometryCreationMode);
                addLineToShapeGeometry(v2dTo3d(secondCornerFrame + vecWF / widthF * L1F * cos(angle123 / 2)), v2dTo3d(thirdCornerFrame - vecWF / widthF * L2F * cos(angle412 / 2)), geometryCreationMode);
                addLineToShapeGeometry(v2dTo3d(thirdCornerFrame - vecLF / lengthF * L2F * cos(angle412 / 2)), v2dTo3d(fourthCornerFrame + vecLF / lengthF * L1F * cos(angle123 / 2)), geometryCreationMode);
                addLineToShapeGeometry(v2dTo3d(fourthCornerFrame - vecWF / widthF * L1F * cos(angle123 / 2)), v2dTo3d(firstCornerFrame + vecWF / widthF * L2F * cos(angle412 / 2)), geometryCreationMode);

                if (roundCorners && radiusFrame > Precision::Confusion()) {
                    Base::Vector2d b1 = (vecL + vecW) / (vecL + vecW).Length();
                    Base::Vector2d b2 = (vecL - vecW) / (vecL - vecW).Length();

                    addArcToShapeGeometry(v2dTo3d(firstCornerFrame + b1 * L2F), end - M_PI + angle412, end, radiusFrame, geometryCreationMode);
                    addArcToShapeGeometry(v2dTo3d(secondCornerFrame - b2 * L1F), end, end - M_PI - angle123, radiusFrame, geometryCreationMode);
                    addArcToShapeGeometry(v2dTo3d(thirdCornerFrame - b1 * L2F), end + angle412, end - M_PI, radiusFrame, geometryCreationMode);
                    addArcToShapeGeometry(v2dTo3d(fourthCornerFrame + b2 * L1F), end - M_PI, end - angle123, radiusFrame, geometryCreationMode);
                }
            }

            if (!onlyeditoutline) {
                ShapeConstraints.clear();
                Sketcher::ConstraintType typeA = Sketcher::Horizontal;
                Sketcher::ConstraintType typeB = Sketcher::Vertical;
                if (Base::sgn(thirdCorner.x - firstCorner.x) * Base::sgn(thirdCorner.y - firstCorner.y) < 0) {
                    typeA = Sketcher::Vertical;
                    typeB = Sketcher::Horizontal;

                }

                if (radius > Precision::Confusion()) {

                    addToShapeConstraints(Sketcher::Tangent, firstCurve, Sketcher::PointPos::start, firstCurve + 4, Sketcher::PointPos::end);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve, Sketcher::PointPos::end, firstCurve + 5, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 1, Sketcher::PointPos::start, firstCurve + 5, Sketcher::PointPos::end);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 1, Sketcher::PointPos::end, firstCurve + 6, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 2, Sketcher::PointPos::start, firstCurve + 6, Sketcher::PointPos::end);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 2, Sketcher::PointPos::end, firstCurve + 7, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 3, Sketcher::PointPos::start, firstCurve + 7, Sketcher::PointPos::end);
                    addToShapeConstraints(Sketcher::Tangent, firstCurve + 3, Sketcher::PointPos::end, firstCurve + 4, Sketcher::PointPos::start);

                    if (fabs(angle) < Precision::Confusion() || constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                        addToShapeConstraints(typeA, firstCurve);
                        addToShapeConstraints(typeA, firstCurve + 2);
                        addToShapeConstraints(typeB, firstCurve + 1);
                        addToShapeConstraints(typeB, firstCurve + 3);
                    }
                    else {
                        addToShapeConstraints(Sketcher::Parallel, firstCurve, Sketcher::PointPos::none, firstCurve + 2);
                        addToShapeConstraints(Sketcher::Parallel, firstCurve + 1, Sketcher::PointPos::none, firstCurve + 3);
                        if (fabs(angle123 - M_PI / 2) < Precision::Confusion())
                            addToShapeConstraints(Sketcher::Perpendicular, firstCurve, Sketcher::PointPos::none, firstCurve + 1);
                    }
                    addToShapeConstraints(Sketcher::Equal, firstCurve + 4, Sketcher::PointPos::none, firstCurve + 5);
                    addToShapeConstraints(Sketcher::Equal, firstCurve + 5, Sketcher::PointPos::none, firstCurve + 6);
                    addToShapeConstraints(Sketcher::Equal, firstCurve + 6, Sketcher::PointPos::none, firstCurve + 7);

                    if (fabs(thickness) > Precision::Confusion()) {
                        if (radiusFrame < Precision::Confusion()) { //case inner rectangle is normal rectangle

                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 8, Sketcher::PointPos::end, firstCurve + 9, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 9, Sketcher::PointPos::end, firstCurve + 10, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 10, Sketcher::PointPos::end, firstCurve + 11, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 11, Sketcher::PointPos::end, firstCurve + 8, Sketcher::PointPos::start);

                            if (fabs(angle) < Precision::Confusion() || constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                                addToShapeConstraints(typeA, firstCurve + 8);
                                addToShapeConstraints(typeA, firstCurve + 10);
                                addToShapeConstraints(typeB, firstCurve + 9);
                                addToShapeConstraints(typeB, firstCurve + 11);
                            }
                            else {
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 8, Sketcher::PointPos::none, firstCurve + 10);
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 9, Sketcher::PointPos::none, firstCurve + 11);
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 8, Sketcher::PointPos::none, firstCurve);
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 9, Sketcher::PointPos::none, firstCurve + 1);
                            }

                            //add construction lines +12, +13, +14, +15
                            addLineToShapeGeometry(arc1Center, Base::Vector3d(firstCornerFrame.x, firstCornerFrame.y, 0.), true);
                            addLineToShapeGeometry(arc2Center, Base::Vector3d(secondCornerFrame.x, secondCornerFrame.y, 0.), true);
                            addLineToShapeGeometry(arc3Center, Base::Vector3d(thirdCornerFrame.x, thirdCornerFrame.y, 0.), true);
                            addLineToShapeGeometry(arc4Center, Base::Vector3d(fourthCornerFrame.x, fourthCornerFrame.y, 0.), true);

                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 12, Sketcher::PointPos::start, firstCurve + 4, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 12, Sketcher::PointPos::end, firstCurve + 8, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 13, Sketcher::PointPos::start, firstCurve + 5, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 13, Sketcher::PointPos::end, firstCurve + 9, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 14, Sketcher::PointPos::start, firstCurve + 6, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 14, Sketcher::PointPos::end, firstCurve + 10, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 15, Sketcher::PointPos::start, firstCurve + 7, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 15, Sketcher::PointPos::end, firstCurve + 11, Sketcher::PointPos::start);

                            addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 12, Sketcher::PointPos::none, firstCurve + 13);
                            addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 13, Sketcher::PointPos::none, firstCurve + 14);
                            addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 14, Sketcher::PointPos::none, firstCurve + 15);

                        }
                        else { //case inner rectangle is rounded rectangle
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 8, Sketcher::PointPos::start, firstCurve + 12, Sketcher::PointPos::end);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 8, Sketcher::PointPos::end, firstCurve + 13, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 9, Sketcher::PointPos::start, firstCurve + 13, Sketcher::PointPos::end);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 9, Sketcher::PointPos::end, firstCurve + 14, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 10, Sketcher::PointPos::start, firstCurve + 14, Sketcher::PointPos::end);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 10, Sketcher::PointPos::end, firstCurve + 15, Sketcher::PointPos::start);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 11, Sketcher::PointPos::start, firstCurve + 15, Sketcher::PointPos::end);
                            addToShapeConstraints(Sketcher::Tangent, firstCurve + 11, Sketcher::PointPos::end, firstCurve + 12, Sketcher::PointPos::start);

                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 4, Sketcher::PointPos::mid, firstCurve + 12, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 5, Sketcher::PointPos::mid, firstCurve + 13, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 6, Sketcher::PointPos::mid, firstCurve + 14, Sketcher::PointPos::mid);
                            addToShapeConstraints(Sketcher::Coincident, firstCurve + 7, Sketcher::PointPos::mid, firstCurve + 15, Sketcher::PointPos::mid);

                            if (fabs(angle) < Precision::Confusion() || constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                                addToShapeConstraints(typeA, firstCurve + 8);
                                addToShapeConstraints(typeA, firstCurve + 10);
                                addToShapeConstraints(typeB, firstCurve + 9);
                            }
                            else {
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 8, Sketcher::PointPos::none, firstCurve + 10);
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 9, Sketcher::PointPos::none, firstCurve + 11);
                                addToShapeConstraints(Sketcher::Parallel, firstCurve + 8, Sketcher::PointPos::none, firstCurve );
                            }
                        }
                    }

                    if (constructionMethod() == ConstructionMethod::ThreePoints) {
                        if (fabs(thickness) > Precision::Confusion()) {
                            constructionPointOneId = firstCurve + 16;
                            constructionPointTwoId = firstCurve + 17;
                            constructionPointThreeId = firstCurve + 18;
                        }
                        else {
                            constructionPointOneId = firstCurve + 8;
                            constructionPointTwoId = firstCurve + 9;
                            constructionPointThreeId = firstCurve + 10;
                        }

                        addPointToShapeGeometry(Base::Vector3d(firstCorner.x, firstCorner.y, 0.), true);
                        if (!cornersReversed) {
                            addPointToShapeGeometry(Base::Vector3d(secondCorner.x, secondCorner.y, 0.), true);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 1);
                        }
                        else {
                            addPointToShapeGeometry(Base::Vector3d(fourthCorner.x, fourthCorner.y, 0.), true);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 2);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 3);
                        }
                        addPointToShapeGeometry(Base::Vector3d(thirdCorner.x, thirdCorner.y, 0.), true);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve + 3);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointThreeId, Sketcher::PointPos::start, firstCurve + 1);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointThreeId, Sketcher::PointPos::start, firstCurve + 2);
                    }
                    else if (constructionMethod() == ConstructionMethod::CenterAnd3Points) {
                        if (fabs(thickness) > Precision::Confusion()) {
                            constructionPointOneId = firstCurve + 16;
                           constructionPointTwoId = firstCurve + 17;
                            centerPointId = firstCurve + 18;
                        }
                        else {
                            constructionPointOneId = firstCurve + 8;
                            constructionPointTwoId = firstCurve + 9;
                            centerPointId = firstCurve + 10;
                        }

                        addPointToShapeGeometry(Base::Vector3d(firstCorner.x, firstCorner.y, 0.), true);
                        if (!cornersReversed) {
                            addPointToShapeGeometry(Base::Vector3d(secondCorner.x, secondCorner.y, 0.), true);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 1);
                        }
                        else {
                            addPointToShapeGeometry(Base::Vector3d(fourthCorner.x, fourthCorner.y, 0.), true);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 2);
                            addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 3);
                        }
                        addPointToShapeGeometry(Base::Vector3d(center.x, center.y, 0.), true);
                        addToShapeConstraints(Sketcher::Symmetric, firstCurve + 2, Sketcher::PointPos::start, firstCurve, Sketcher::PointPos::start, centerPointId, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve + 3);
                    }
                    else if (constructionMethod() == ConstructionMethod::CenterAndCorner) {
                        if (fabs(thickness) > Precision::Confusion()) {
                            constructionPointOneId = firstCurve + 16;
                            centerPointId = firstCurve + 17;
                        }
                        else {
                            constructionPointOneId = firstCurve + 8;
                            centerPointId = firstCurve + 9;
                        }

                        addPointToShapeGeometry(Base::Vector3d(thirdCorner.x, thirdCorner.y, 0.), true);
                        addPointToShapeGeometry(Base::Vector3d(center.x, center.y, 0.), true);
                        addToShapeConstraints(Sketcher::Symmetric, firstCurve + 2, Sketcher::PointPos::start, firstCurve, Sketcher::PointPos::start, centerPointId, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve + 1);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve + 2);
                    }
                    else {
                        if (fabs(thickness) > Precision::Confusion()) {
                            constructionPointOneId = firstCurve + 16;
                            constructionPointTwoId = firstCurve + 17;
                        }
                        else {
                            constructionPointOneId = firstCurve + 8;
                            constructionPointTwoId = firstCurve + 9;
                        }

                        addPointToShapeGeometry(Base::Vector3d(firstCorner.x, firstCorner.y, 0.), true);
                        addPointToShapeGeometry(Base::Vector3d(thirdCorner.x, thirdCorner.y, 0.), true);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointOneId, Sketcher::PointPos::start, firstCurve + 3);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 1);
                        addToShapeConstraints(Sketcher::PointOnObject, constructionPointTwoId, Sketcher::PointPos::start, firstCurve + 2);
                    }

                }
                else { //cases of normal rectangles and normal frames
                    addToShapeConstraints(Sketcher::Coincident, firstCurve, Sketcher::PointPos::end, firstCurve + 1, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Coincident, firstCurve + 1, Sketcher::PointPos::end, firstCurve + 2, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Coincident, firstCurve + 2, Sketcher::PointPos::end, firstCurve + 3, Sketcher::PointPos::start);
                    addToShapeConstraints(Sketcher::Coincident, firstCurve + 3, Sketcher::PointPos::end, firstCurve, Sketcher::PointPos::start);
                    if (fabs(angle) < Precision::Confusion() || constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                        addToShapeConstraints(typeA, firstCurve);
                        addToShapeConstraints(typeA, firstCurve + 2);
                        addToShapeConstraints(typeB, firstCurve + 1);
                        addToShapeConstraints(typeB, firstCurve + 3);
                    }
                    else {
                        addToShapeConstraints(Sketcher::Parallel, firstCurve, Sketcher::PointPos::none, firstCurve + 2);
                        addToShapeConstraints(Sketcher::Parallel, firstCurve + 1, Sketcher::PointPos::none, firstCurve + 3);
                        if (fabs(angle123 - M_PI / 2) < Precision::Confusion())
                            addToShapeConstraints(Sketcher::Perpendicular, firstCurve, Sketcher::PointPos::none, firstCurve + 1);
                    }

                    if (fabs(thickness) > Precision::Confusion()) {
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 4, Sketcher::PointPos::end, firstCurve + 5, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 5, Sketcher::PointPos::end, firstCurve + 6, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 6, Sketcher::PointPos::end, firstCurve + 7, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 7, Sketcher::PointPos::end, firstCurve + 4, Sketcher::PointPos::start);

                        if (fabs(angle) < Precision::Confusion() || constructionMethod() == ConstructionMethod::Diagonal || constructionMethod() == ConstructionMethod::CenterAndCorner) {
                            addToShapeConstraints(typeA, firstCurve + 4);
                            addToShapeConstraints(typeA, firstCurve + 6);
                            addToShapeConstraints(typeB, firstCurve + 5);
                            addToShapeConstraints(typeB, firstCurve + 7);
                        }
                        else {
                            addToShapeConstraints(Sketcher::Parallel, firstCurve + 4, Sketcher::PointPos::none, firstCurve + 6);
                            addToShapeConstraints(Sketcher::Parallel, firstCurve + 5, Sketcher::PointPos::none, firstCurve + 7);
                            if (fabs(angle123 - M_PI / 2) < Precision::Confusion())
                                addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 4, Sketcher::PointPos::none, firstCurve + 5);
                        }

                        //add construction lines
                        addLineToShapeGeometry(Base::Vector3d(firstCorner.x, firstCorner.y, 0.), Base::Vector3d(firstCornerFrame.x, firstCornerFrame.y, 0.), true);
                        addLineToShapeGeometry(Base::Vector3d(secondCorner.x, secondCorner.y, 0.), Base::Vector3d(secondCornerFrame.x, secondCornerFrame.y, 0.), true);
                        addLineToShapeGeometry(Base::Vector3d(thirdCorner.x, thirdCorner.y, 0.), Base::Vector3d(thirdCornerFrame.x, thirdCornerFrame.y, 0.), true);
                        addLineToShapeGeometry(Base::Vector3d(fourthCorner.x, fourthCorner.y, 0.), Base::Vector3d(fourthCornerFrame.x, fourthCornerFrame.y, 0.), true);

                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 8, Sketcher::PointPos::start, firstCurve, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 8, Sketcher::PointPos::end, firstCurve + 4, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 9, Sketcher::PointPos::start, firstCurve + 1, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 9, Sketcher::PointPos::end, firstCurve + 5, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 10, Sketcher::PointPos::start, firstCurve + 2, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 10, Sketcher::PointPos::end, firstCurve + 6, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 11, Sketcher::PointPos::start, firstCurve + 3, Sketcher::PointPos::start);
                        addToShapeConstraints(Sketcher::Coincident, firstCurve + 11, Sketcher::PointPos::end, firstCurve + 7, Sketcher::PointPos::start);

                        addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 8, Sketcher::PointPos::none, firstCurve + 9);
                        addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 9, Sketcher::PointPos::none, firstCurve + 10);
                        addToShapeConstraints(Sketcher::Perpendicular, firstCurve + 10, Sketcher::PointPos::none, firstCurve + 11);
                    }

                    if (constructionMethod() == ConstructionMethod::CenterAndCorner || constructionMethod() == ConstructionMethod::CenterAnd3Points) {
                        if (fabs(thickness) > Precision::Confusion())
                            centerPointId = firstCurve + 12;
                        else
                            centerPointId = firstCurve + 4;

                        addPointToShapeGeometry(Base::Vector3d(center.x, center.y, 0.), true);
                        addToShapeConstraints(Sketcher::Symmetric, firstCurve + 2, Sketcher::PointPos::start, firstCurve, Sketcher::PointPos::start, centerPointId, Sketcher::PointPos::start);
                    }
                }
            }
        }
    }

    Base::Vector3d v2dTo3d(Base::Vector2d vec2d) {
        return Base::Vector3d(vec2d.x, vec2d.y, 0.);
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

    void calculateRadius(Base::Vector2d onSketchPos) {
        Base::Vector2d u = (secondCorner - firstCorner) / (secondCorner - firstCorner).Length();
        Base::Vector2d v = (fourthCorner - firstCorner) / (fourthCorner - firstCorner).Length();
        Base::Vector2d e = onSketchPos - firstCorner;
        double du = (v.y * e.x - v.x * e.y) / (u.x * v.y - u.y * v.x);
        double dv = (-u.y * e.x + u.x * e.y) / (u.x * v.y - u.y * v.x);
        if (du < 0 || du > length || dv < 0 || dv > width) {
            radius = 0.;
        }
        else {
            if (du < length - du && dv < width - dv) {
                radius = (du + dv + std::max(2 * sqrt(du * dv) * sin(angle412 / 2), - 2 * sqrt(du * dv) * sin(angle412 / 2))) * tan(angle412 / 2);
            }
            else if (du > length - du && dv < width - dv) {
                du = length - du;
                radius = (du + dv + std::max(2 * sqrt(du * dv) * sin(angle123 / 2), - 2 * sqrt(du * dv) * sin(angle123 / 2))) * tan(angle123 / 2);
            }
            else if (du < length - du && dv > width - dv) {
                dv = width - dv;
                radius = (du + dv + std::max(2 * sqrt(du * dv) * sin(angle123 / 2), - 2 * sqrt(du * dv) * sin(angle123 / 2))) * tan(angle123 / 2);
            }
            else{
                du = length - du;
                dv = width - dv;
                radius = (du + dv + std::max(2 * sqrt(du * dv) * sin(angle412 / 2), - 2 * sqrt(du * dv) * sin(angle412 / 2))) * tan(angle412 / 2);
            }
            radius = std::min(radius, 
                std::min(length * 0.999, width * 0.999) 
                / (   cos(angle412 / 2) / sqrt(1 - cos(angle412 / 2) * cos(angle412 / 2)) 
                    + cos(angle123 / 2) / sqrt(1 - cos(angle123 / 2) * cos(angle123 / 2)) ) );
        }

        SbString text;
        text.sprintf(" (%.1f radius)", radius);
        setPositionText(onSketchPos, text);
    }

    void calculateThickness(Base::Vector2d onSketchPos) {

        Base::Vector2d u = (secondCorner - firstCorner) / (secondCorner - firstCorner).Length();
        Base::Vector2d v = (fourthCorner - firstCorner) / (fourthCorner - firstCorner).Length();
        Base::Vector2d e = onSketchPos - firstCorner;
        double du = (v.y * e.x - v.x * e.y) / (u.x * v.y - u.y * v.x);
        double dv = (-u.y * e.x + u.x * e.y) / (u.x * v.y - u.y * v.x);
        if (du > 0 && du < length && !( dv > 0 && dv < width))
            thickness = std::min(fabs(dv), fabs(width - dv));
        else if (dv > 0 && dv < width && !(du > 0 && du < length))
            thickness = std::min(fabs(du), fabs(length - du));
        else if (du > 0 && du < length && dv > 0 && dv < width)
            thickness = -std::min(std::min(fabs(du), fabs(length - du)), std::min(fabs(dv), fabs(width - dv)));
        else
            thickness = std::max(std::min(fabs(du), fabs(length - du)), std::min(fabs(dv), fabs(width - dv)));


        firstCornerFrame = firstCorner - u * thickness - v * thickness;
        secondCornerFrame = secondCorner + u * thickness - v * thickness;
        thirdCornerFrame = thirdCorner + u * thickness + v * thickness;
        fourthCornerFrame = fourthCorner - u * thickness + v * thickness;

        SbString text;
        text.sprintf(" (%.1fT)", thickness);
        setPositionText(onSketchPos, text);
    }
};

template <> auto DrawSketchHandlerRectangleBase::ToolWidgetManager::getState(int parameterindex) const {
    if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
        || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
        switch (parameterindex) {
        case WParameter::First:
        case WParameter::Second:
            return SelectMode::SeekFirst;
            break;
        case WParameter::Third:
        case WParameter::Fourth:
            return SelectMode::SeekSecond;
            break;
        case WParameter::Fifth:
            return SelectMode::SeekThird;
            break;
        case WParameter::Sixth:
            if (!dHandler->roundCorners)
                return SelectMode::SeekThird;
            else
                return SelectMode::SeekFourth;
            break;
        default:
            THROWM(Base::ValueError, "Parameter index without an associated machine state")
        }
    }
    else {
        switch (parameterindex) {
        case WParameter::First:
        case WParameter::Second:
            return SelectMode::SeekFirst;
            break;
        case WParameter::Third:
        case WParameter::Fourth:
            return SelectMode::SeekSecond;
            break;
        case WParameter::Fifth:
        case WParameter::Sixth:
            return SelectMode::SeekThird;
            break;
        case WParameter::Seventh:
            return SelectMode::SeekFourth;
            break;
        case WParameter::Eighth:
            if (!dHandler->roundCorners)
                return SelectMode::SeekFourth;
            else
                return SelectMode::SeekFifth;
            break;
        default:
            THROWM(Base::ValueError, "Parameter index without an associated machine state")
        }
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::setModeIcons() {
    if (geometryCreationMode) {
        toolWidget->setModeIcon(0, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle_Constr"));
        toolWidget->setModeIcon(1, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle_Center_Constr"));
        toolWidget->setModeIcon(2, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle3Points_Constr"));
        toolWidget->setModeIcon(3, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle3Points_Center_Constr"));

        toolWidget->setCheckboxIcon(WCheckbox::FirstBox, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateOblong_Constr"));
        toolWidget->setCheckboxIcon(WCheckbox::SecondBox, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateFrame_Constr"));
    }
    else {
        toolWidget->setModeIcon(0, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle"));
        toolWidget->setModeIcon(1, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle_Center"));
        toolWidget->setModeIcon(2, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle3Points"));
        toolWidget->setModeIcon(3, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateRectangle3Points_Center"));

        toolWidget->setCheckboxIcon(WCheckbox::FirstBox, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateOblong"));
        toolWidget->setCheckboxIcon(WCheckbox::SecondBox, Gui::BitmapFactory().iconFromTheme("Sketcher_CreateFrame"));
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::configureToolWidget() {
    if(!init) { // Code to be executed only upon initialisation
        toolWidget->initNModes(4);
        QStringList names = {QStringLiteral("Diagonal corners"), QStringLiteral("Center and corner"), QStringLiteral("3 corners") , QStringLiteral("Center and 2 corners") };
        toolWidget->setModeToolTips(names);

        setModeIcons();
        toolWidget->useConstructionGeometryButtons(true);

        syncConstructionMethodButtonToHandler(); // in case the DSH was called with a specific construction method
    }

    if(dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal){
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_rectangle", "x of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_rectangle", "y of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_rectangle", "Length (X axis)"));
        toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("TaskSketcherTool_p4_rectangle", "Width (Y axis)"));
        toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("TaskSketcherTool_p5_rectangle", "Corner radius"));
        toolWidget->setParameterEnabled(WParameter::Fifth, dHandler->roundCorners);
        toolWidget->setParameterLabel(WParameter::Sixth, QApplication::translate("TaskSketcherTool_p6_rectangle", "Thickness"));
        toolWidget->setParameterEnabled(WParameter::Sixth, dHandler->makeFrame);
    }
    else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_rectangle", "x of center point"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_rectangle", "y of center point"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_rectangle", "Length (X axis)"));
        toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("TaskSketcherTool_p4_rectangle", "Width (Y axis)"));
        toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("TaskSketcherTool_p5_rectangle", "Corner radius"));
        toolWidget->setParameterEnabled(WParameter::Fifth, dHandler->roundCorners);
        toolWidget->setParameterLabel(WParameter::Sixth, QApplication::translate("TaskSketcherTool_p6_rectangle", "Thickness"));
        toolWidget->setParameterEnabled(WParameter::Sixth, dHandler->makeFrame);
    }
    else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_rectangle", "x of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_rectangle", "y of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_rectangle", "Length"));
        toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("TaskSketcherTool_p4_rectangle", "Angle (to HAxis)"));
        toolWidget->configureParameterUnit(WParameter::Fourth, Base::Unit::Angle);
        toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("TaskSketcherTool_p3_rectangle", "Width"));
        toolWidget->setParameterLabel(WParameter::Sixth, QApplication::translate("TaskSketcherTool_p4_rectangle", "Angle (to first edge)"));
        toolWidget->configureParameterUnit(WParameter::Sixth, Base::Unit::Angle);
        toolWidget->setParameterLabel(WParameter::Seventh, QApplication::translate("TaskSketcherTool_p5_rectangle", "Corner radius"));
        toolWidget->setParameterEnabled(WParameter::Seventh, dHandler->roundCorners);
        toolWidget->setParameterLabel(WParameter::Eighth, QApplication::translate("TaskSketcherTool_p6_rectangle", "Thickness"));
        toolWidget->setParameterEnabled(WParameter::Eighth, dHandler->makeFrame);
        //TODO: Enable is not reset when construction mode change. So here we force Fifth and Sixth to be enable manually
        toolWidget->setParameterEnabled(WParameter::Fifth, true);
        toolWidget->setParameterEnabled(WParameter::Sixth, true);
    }
    else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points) {
        toolWidget->setParameterLabel(WParameter::First, QApplication::translate("TaskSketcherTool_p1_rectangle", "x of center point"));
        toolWidget->setParameterLabel(WParameter::Second, QApplication::translate("TaskSketcherTool_p2_rectangle", "y of center point"));
        toolWidget->setParameterLabel(WParameter::Third, QApplication::translate("TaskSketcherTool_p3_rectangle", "x of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Fourth, QApplication::translate("TaskSketcherTool_p4_rectangle", "y of 1st corner"));
        toolWidget->setParameterLabel(WParameter::Fifth, QApplication::translate("TaskSketcherTool_p3_rectangle", "Width"));
        toolWidget->setParameterLabel(WParameter::Sixth, QApplication::translate("TaskSketcherTool_p4_rectangle", "Angle (to first edge)"));
        toolWidget->configureParameterUnit(WParameter::Sixth, Base::Unit::Angle);
        toolWidget->setParameterLabel(WParameter::Seventh, QApplication::translate("TaskSketcherTool_p5_rectangle", "Corner radius"));
        toolWidget->setParameterEnabled(WParameter::Seventh, dHandler->roundCorners);
        toolWidget->setParameterLabel(WParameter::Eighth, QApplication::translate("TaskSketcherTool_p6_rectangle", "Thickness"));
        toolWidget->setParameterEnabled(WParameter::Eighth, dHandler->makeFrame);
        toolWidget->setParameterEnabled(WParameter::Fifth, true);
        toolWidget->setParameterEnabled(WParameter::Sixth, true);
    }

    toolWidget->setCheckboxLabel(WCheckbox::FirstBox, QApplication::translate("TaskSketcherTool_c1_rectangle", "Rounded corners (U)"));
    toolWidget->setCheckboxToolTip(WCheckbox::FirstBox, QApplication::translate("TaskSketcherTool_c1_rectangle", "Create a rectangle with rounded corners."));
    syncCheckboxToHandler(WCheckbox::FirstBox, dHandler->roundCorners);

    toolWidget->setCheckboxLabel(WCheckbox::SecondBox, QApplication::translate("TaskSketcherTool_c2_rectangle", "Frame (J)"));
    toolWidget->setCheckboxToolTip(WCheckbox::SecondBox, QApplication::translate("TaskSketcherTool_c2_rectangle", "Create two rectangles, one in the other with a constant thickness."));
    syncCheckboxToHandler(WCheckbox::SecondBox, dHandler->makeFrame);

    handler->updateCursor();
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::adaptDrawingToParameterChange(int parameterindex, double value) {
    if(dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal){
        switch(parameterindex) {
            case WParameter::First:
                dHandler->firstCorner.x = value;
                dHandler->fourthCorner.x = value;
                break;
            case WParameter::Second:
                dHandler->firstCorner.y = value;
                dHandler->secondCorner.y = value;
                break;
            case WParameter::Third:
                dHandler->length = value;
                dHandler->thirdCorner.x = dHandler->firstCorner.x + dHandler->length;
                dHandler->secondCorner.x = dHandler->thirdCorner.x;
                break;
            case WParameter::Fourth:
                dHandler->width = value;
                dHandler->thirdCorner.y = dHandler->firstCorner.y + dHandler->width;
                dHandler->fourthCorner.y = dHandler->thirdCorner.y;
                break;
            case WParameter::Fifth:
                dHandler->radius = value;
                break;
            case WParameter::Sixth:
                dHandler->thickness = value;
                break;
        }
    }
    else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) { //if (constructionMethod == ConstructionMethod::CenterAndCorner)
        switch(parameterindex) {
            case WParameter::First:
                dHandler->center.x = value;
                dHandler->firstCorner.x = value - dHandler->length / 2;
                dHandler->secondCorner.x = value + dHandler->length / 2;
                dHandler->thirdCorner.x = value + dHandler->length / 2;
                dHandler->fourthCorner.x = value - dHandler->length / 2;
                break;
            case WParameter::Second:
                dHandler->center.y = value;
                dHandler->firstCorner.y = value - dHandler->width / 2;
                dHandler->secondCorner.y = value - dHandler->width / 2;
                dHandler->thirdCorner.y = value + dHandler->width / 2;
                dHandler->fourthCorner.y = value + dHandler->width / 2;
                break;
            case WParameter::Third:
                dHandler->length = value;
                dHandler->firstCorner.x = dHandler->center.x + dHandler->length/2;
                dHandler->thirdCorner.x = dHandler->center.x - dHandler->length/2;
                dHandler->secondCorner.x = dHandler->thirdCorner.x;
                dHandler->fourthCorner.x = dHandler->firstCorner.x;
                break;
            case WParameter::Fourth:
                dHandler->width = value;
                dHandler->firstCorner.y = dHandler->center.y + dHandler->width / 2;
                dHandler->thirdCorner.y = dHandler->center.y - dHandler->width / 2;
                dHandler->secondCorner.y = dHandler->firstCorner.y;
                dHandler->fourthCorner.y = dHandler->thirdCorner.y;
                break;
            case WParameter::Fifth:
                dHandler->radius = value;
                break;
            case WParameter::Sixth:
                dHandler->thickness = value;
                break;
        }
    }
    else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
        switch (parameterindex) {
        case WParameter::First:
            dHandler->firstCorner.x = value;
            break;
        case WParameter::Second:
            dHandler->firstCorner.y = value;
            break;
        case WParameter::Third:
            dHandler->length = value;
            break;
        case WParameter::Fourth:
            dHandler->angle = value;
            break;
        case WParameter::Fifth:
            dHandler->width = value;
            break;
        case WParameter::Sixth:
            dHandler->angle123 = value;
            break;
        case WParameter::Seventh:
            dHandler->radius = value;
            break;
        case WParameter::Eighth:
            dHandler->thickness = value;
            break;
        }
    }
    else {
        switch (parameterindex) {
        case WParameter::First:
            dHandler->center.x = value;
            break;
        case WParameter::Second:
            dHandler->center.y = value;
            break;
        case WParameter::Third:
            dHandler->firstCorner.x = value;
            break;
        case WParameter::Fourth:
            dHandler->firstCorner.y = value;
            break;
        case WParameter::Fifth:
            dHandler->width = value;
            break;
        case WParameter::Sixth:
            dHandler->angle123 = value;
            break;
        case WParameter::Seventh:
            dHandler->radius = value;
            break;
        case WParameter::Eighth:
            dHandler->thickness = value;
            break;
        }
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::adaptDrawingToCheckboxChange(int checkboxindex, bool value) {
    Q_UNUSED(checkboxindex);

    switch (checkboxindex) {
    case WCheckbox::FirstBox:
        dHandler->roundCorners = value;
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner)
            toolWidget->setParameterEnabled(WParameter::Fifth, value);
        else
            toolWidget->setParameterEnabled(WParameter::Seventh, value);

        break;
    case WCheckbox::SecondBox:
        dHandler->makeFrame = value;
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner)
            toolWidget->setParameterEnabled(WParameter::Sixth, value);
        else
            toolWidget->setParameterEnabled(WParameter::Eighth, value);
        break;
    }

    handler->updateCursor();
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::doEnforceWidgetParameters(Base::Vector2d& onSketchPos) {
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
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal) {
            if (toolWidget->isParameterSet(WParameter::Third)) {
                onSketchPos.x = dHandler->firstCorner.x + toolWidget->getParameter(WParameter::Third);
            }
            if (toolWidget->isParameterSet(WParameter::Fourth)) {
                onSketchPos.y = dHandler->firstCorner.y + toolWidget->getParameter(WParameter::Fourth);
            }
        }
        else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (toolWidget->isParameterSet(WParameter::Third)) {
                onSketchPos.x = dHandler->center.x + toolWidget->getParameter(WParameter::Third) /2;
            }
            if (toolWidget->isParameterSet(WParameter::Fourth)) {
                onSketchPos.y = dHandler->center.y + toolWidget->getParameter(WParameter::Fourth) / 2;
            }
        }
        else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
            Base::Vector2d v = onSketchPos - dHandler->firstCorner;
            double length = v.Length();
            if (toolWidget->isParameterSet(WParameter::Third)) {
                onSketchPos = dHandler->firstCorner + v * toolWidget->getParameter(WParameter::Third) / length;
                length = toolWidget->getParameter(WParameter::Third);

            }
            if (toolWidget->isParameterSet(WParameter::Fourth)) {
                double angle = toolWidget->getParameter(WParameter::Fourth) * M_PI / 180;
                onSketchPos.x = dHandler->firstCorner.x + cos(angle) * length;
                onSketchPos.y = dHandler->firstCorner.y + sin(angle) * length;
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
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal 
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (dHandler->roundCorners) {
                if (toolWidget->isParameterSet(WParameter::Fifth)) {
                    Base::Vector2d vecL = (dHandler->secondCorner - dHandler->firstCorner) / (dHandler->secondCorner - dHandler->firstCorner).Length();
                    onSketchPos = dHandler->firstCorner + vecL * toolWidget->getParameter(WParameter::Fifth);
                }
            }
            else {
                if (toolWidget->isParameterSet(WParameter::Sixth)) {
                    double thickness = toolWidget->getParameter(WParameter::Sixth);
                    Base::Vector2d u = (dHandler->secondCorner - dHandler->firstCorner) / (dHandler->secondCorner - dHandler->firstCorner).Length();
                    Base::Vector2d v = (dHandler->fourthCorner - dHandler->firstCorner) / (dHandler->fourthCorner - dHandler->firstCorner).Length();
                    onSketchPos = dHandler->firstCorner - u * thickness - v * thickness;
                }
            }
        }
        else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
            Base::Vector2d v = onSketchPos - dHandler->secondCornerInitial;
            double width = v.Length();
            if (toolWidget->isParameterSet(WParameter::Fifth)) {
                onSketchPos = dHandler->secondCornerInitial + v * toolWidget->getParameter(WParameter::Fifth) / width;
                width = toolWidget->getParameter(WParameter::Fifth);
            }
            if (toolWidget->isParameterSet(WParameter::Sixth)) {
                double angle123 = (dHandler->secondCornerInitial - dHandler->firstCorner).Angle() + M_PI - toolWidget->getParameter(WParameter::Sixth) * M_PI / 180;

                if (dHandler->cornersReversed)
                    angle123 = (dHandler->secondCornerInitial - dHandler->firstCorner).Angle() + M_PI + toolWidget->getParameter(WParameter::Sixth) * M_PI / 180;
                onSketchPos.x = dHandler->secondCornerInitial.x + cos(angle123) * width;
                onSketchPos.y = dHandler->secondCornerInitial.y + sin(angle123) * width;
            }
        }
        else {
            Base::Vector2d v = onSketchPos - dHandler->firstCorner;
            double width = v.Length();
            if (toolWidget->isParameterSet(WParameter::Fifth)) {
                onSketchPos = dHandler->firstCorner + v * toolWidget->getParameter(WParameter::Fifth) / width;
                width = toolWidget->getParameter(WParameter::Fifth);
            }
            if (toolWidget->isParameterSet(WParameter::Sixth)) {
                double angle412 = (dHandler->secondCornerInitial - dHandler->firstCorner).Angle() + M_PI - toolWidget->getParameter(WParameter::Sixth) * M_PI / 180;
                if (dHandler->cornersReversed)
                    angle412 = (dHandler->secondCornerInitial - dHandler->firstCorner).Angle() - M_PI - toolWidget->getParameter(WParameter::Sixth) * M_PI / 180;

                onSketchPos.x = dHandler->firstCorner.x + cos(angle412) * width;
                onSketchPos.y = dHandler->firstCorner.y + sin(angle412) * width;
            }
        }
    }
    break;
    case SelectMode::SeekFourth:
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {

            if (toolWidget->isParameterSet(WParameter::Sixth)) {
                double thickness = toolWidget->getParameter(WParameter::Sixth);
                Base::Vector2d u = (dHandler->secondCorner - dHandler->firstCorner) / (dHandler->secondCorner - dHandler->firstCorner).Length();
                Base::Vector2d v = (dHandler->fourthCorner - dHandler->firstCorner) / (dHandler->fourthCorner - dHandler->firstCorner).Length();
                onSketchPos = dHandler->firstCorner - u * thickness - v * thickness;
            }
        }
        else {
            if (dHandler->roundCorners) {
                if (toolWidget->isParameterSet(WParameter::Seventh)) {
                    double angleToUse = dHandler->angle412 / 2;
                    if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points)
                        angleToUse = dHandler->angle123 / 2;
                    Base::Vector2d vecL = (dHandler->secondCornerInitial - dHandler->firstCorner) / (dHandler->secondCornerInitial - dHandler->firstCorner).Length();
                    double L2 = toolWidget->getParameter(WParameter::Seventh) / sqrt(1 - cos(angleToUse) * cos(angleToUse));
                    onSketchPos = dHandler->firstCorner + vecL * L2 * cos(angleToUse);
                }
            }
            else {
                if (toolWidget->isParameterSet(WParameter::Eighth)) {
                    double thickness = toolWidget->getParameter(WParameter::Eighth);
                    Base::Vector2d u = (dHandler->secondCorner - dHandler->firstCorner) / (dHandler->secondCorner - dHandler->firstCorner).Length();
                    Base::Vector2d v = (dHandler->fourthCorner - dHandler->firstCorner) / (dHandler->fourthCorner - dHandler->firstCorner).Length();
                    onSketchPos = dHandler->firstCorner - u * thickness - v * thickness;
                }
            }
        }
    }
    break;
    case SelectMode::SeekFifth:
    {
        if (toolWidget->isParameterSet(WParameter::Eighth)) {
            double thickness = toolWidget->getParameter(WParameter::Eighth);
            Base::Vector2d u = (dHandler->secondCorner - dHandler->firstCorner) / (dHandler->secondCorner - dHandler->firstCorner).Length();
            Base::Vector2d v = (dHandler->fourthCorner - dHandler->firstCorner) / (dHandler->fourthCorner - dHandler->firstCorner).Length();
            onSketchPos = dHandler->firstCorner - u * thickness - v * thickness;
        }
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::adaptWidgetParameters(Base::Vector2d onSketchPos) {

    // If checkboxes need synchronisation (they were changed by the DSH, e.g. by using 'M' to switch construction method), synchronise them and return.
    if(syncCheckboxToHandler(WCheckbox::FirstBox, dHandler->roundCorners))
        return;

    if(syncCheckboxToHandler(WCheckbox::SecondBox, dHandler->makeFrame))
        return;


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
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal) {
            if (!toolWidget->isParameterSet(WParameter::Third))
                toolWidget->updateVisualValue(WParameter::Third, onSketchPos.x - dHandler->firstCorner.x);

            if (!toolWidget->isParameterSet(WParameter::Fourth))
                toolWidget->updateVisualValue(WParameter::Fourth, onSketchPos.y - dHandler->firstCorner.y);
        }
        else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (!toolWidget->isParameterSet(WParameter::Third))
                toolWidget->updateVisualValue(WParameter::Third, fabs(onSketchPos.x - dHandler->center.x)*2);

            if (!toolWidget->isParameterSet(WParameter::Fourth))
                toolWidget->updateVisualValue(WParameter::Fourth, fabs(onSketchPos.y - dHandler->center.y)*2);
        }
        else if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
            if (!toolWidget->isParameterSet(WParameter::Third))
                toolWidget->updateVisualValue(WParameter::Third, dHandler->length);

            if (!toolWidget->isParameterSet(WParameter::Fourth))
                toolWidget->updateVisualValue(WParameter::Fourth, dHandler->angle * 180 / M_PI, Base::Unit::Angle);
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
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal 
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (dHandler->roundCorners) {
                if (!toolWidget->isParameterSet(WParameter::Fifth))
                    toolWidget->updateVisualValue(WParameter::Fifth, dHandler->radius);
            }
            else {
                if (!toolWidget->isParameterSet(WParameter::Sixth))
                    toolWidget->updateVisualValue(WParameter::Sixth, dHandler->thickness);
            }
        }
        else {
            if (!toolWidget->isParameterSet(WParameter::Fifth)){
                if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
                    && !dHandler->cornersReversed)
                    toolWidget->updateVisualValue(WParameter::Fifth, dHandler->width);
                else
                    toolWidget->updateVisualValue(WParameter::Fifth, dHandler->length);
            }
            if (!toolWidget->isParameterSet(WParameter::Sixth)) {
                if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints)
                    toolWidget->updateVisualValue(WParameter::Sixth, dHandler->angle123 * 180 / M_PI, Base::Unit::Angle);
                else
                    toolWidget->updateVisualValue(WParameter::Sixth, dHandler->angle412 * 180 / M_PI, Base::Unit::Angle);
            }
        }

    }
    break;
    case SelectMode::SeekFourth:
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (!toolWidget->isParameterSet(WParameter::Sixth))
                toolWidget->updateVisualValue(WParameter::Sixth, dHandler->thickness);
        }
        else {
            if (dHandler->roundCorners) {
                if (!toolWidget->isParameterSet(WParameter::Seventh))
                    toolWidget->updateVisualValue(WParameter::Seventh, dHandler->radius);
            }
            else {
                if (!toolWidget->isParameterSet(WParameter::Eighth))
                    toolWidget->updateVisualValue(WParameter::Eighth, dHandler->thickness);
            }
        }
    }
    break;
    case SelectMode::SeekFifth:
    {
        if (!toolWidget->isParameterSet(WParameter::Eighth))
            toolWidget->updateVisualValue(WParameter::Eighth, dHandler->thickness);
    }
    break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::doChangeDrawSketchHandlerMode() {
    switch (handler->state()) {
    case SelectMode::SeekFirst:
    {
        if (toolWidget->isParameterSet(WParameter::First) &&
            toolWidget->isParameterSet(WParameter::Second)) {

            handler->setState(SelectMode::SeekSecond);
        }
    }
    break;
    case SelectMode::SeekSecond:
    {
        if (toolWidget->isParameterSet(WParameter::Third) ||
            toolWidget->isParameterSet(WParameter::Fourth)) {

            if (toolWidget->isParameterSet(WParameter::Third) &&
                toolWidget->isParameterSet(WParameter::Fourth)) {

                if (dHandler->roundCorners || dHandler->makeFrame || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
                    || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points) {
                    
                    handler->setState(SelectMode::SeekThird);
                }
                else
                    handler->setState(SelectMode::End);
            }
        }
    }
    break;
    case SelectMode::SeekThird:
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (dHandler->roundCorners && toolWidget->isParameterSet(WParameter::Fifth)) {

                if (dHandler->makeFrame)
                    handler->setState(SelectMode::SeekFourth);
                else
                    handler->setState(SelectMode::End);
            }
            else if (dHandler->makeFrame && toolWidget->isParameterSet(WParameter::Sixth)) {

                handler->setState(SelectMode::End);
            }
        }
        else {
            if (toolWidget->isParameterSet(WParameter::Fifth) &&
                toolWidget->isParameterSet(WParameter::Sixth)) {
                if (dHandler->roundCorners || dHandler->makeFrame)
                    handler->setState(SelectMode::SeekFourth);
                else
                    handler->setState(SelectMode::End);
            }
        }
    }
    break;
    case SelectMode::SeekFourth:
    {
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAndCorner) {
            if (toolWidget->isParameterSet(WParameter::Sixth)) {
                handler->setState(SelectMode::End);
            }
        }
        else {
            if (dHandler->roundCorners && toolWidget->isParameterSet(WParameter::Seventh)) {

                if (dHandler->makeFrame)
                    handler->setState(SelectMode::SeekFifth);
                else
                    handler->setState(SelectMode::End);
            }
            else if (dHandler->makeFrame && toolWidget->isParameterSet(WParameter::Eighth))
                handler->setState(SelectMode::End);
        }
    }
    break;
    case SelectMode::SeekFifth:
    {
        if (dHandler->makeFrame && toolWidget->isParameterSet(WParameter::Eighth)) 
                handler->setState(SelectMode::End);
    }
    break;
    default:
        break;
    }

}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::onHandlerModeChanged() {
    switch (handler->state()) {
    case SelectMode::SeekFirst:
        toolWidget->setParameterFocus(WParameter::First);
        break;
    case SelectMode::SeekSecond:
        toolWidget->setParameterFocus(WParameter::Third);
        break;
    case SelectMode::SeekThird:
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points)
            toolWidget->setParameterFocus(WParameter::Fifth);
        else if(!dHandler->roundCorners)
            toolWidget->setParameterFocus(WParameter::Sixth);
        else
            toolWidget->setParameterFocus(WParameter::Fifth);
        break;
    case SelectMode::SeekFourth:
        if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
            || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points) {
            if(!dHandler->roundCorners)
                toolWidget->setParameterFocus(WParameter::Eighth);
            else
                toolWidget->setParameterFocus(WParameter::Seventh);
        }
        else
            toolWidget->setParameterFocus(WParameter::Sixth);
        break;
    case SelectMode::SeekFifth:
        toolWidget->setParameterFocus(WParameter::Eighth);
        break;
    default:
        break;
    }
}

template <> void DrawSketchHandlerRectangleBase::ToolWidgetManager::addConstraints() {
    int firstCurve = dHandler->firstCurve;

    auto x0 = toolWidget->getParameter(WParameter::First);
    auto y0 = toolWidget->getParameter(WParameter::Second);
    auto length = toolWidget->getParameter(WParameter::Third);
    auto width = toolWidget->getParameter(WParameter::Fourth);
    auto radius = toolWidget->getParameter(WParameter::Fifth);
    auto thickness = toolWidget->getParameter(WParameter::Sixth);

    auto x0set = toolWidget->isParameterSet(WParameter::First);
    auto y0set = toolWidget->isParameterSet(WParameter::Second);
    auto lengthSet = toolWidget->isParameterSet(WParameter::Third);
    auto widthSet = toolWidget->isParameterSet(WParameter::Fourth);
    auto radiusSet = toolWidget->isParameterSet(WParameter::Fifth);
    auto thicknessSet = toolWidget->isParameterSet(WParameter::Sixth);

    auto angle = toolWidget->getParameter(WParameter::Fourth) / 180 * M_PI;
    auto innerAngle = toolWidget->getParameter(WParameter::Sixth) / 180 * M_PI;

    auto angleSet = toolWidget->isParameterSet(WParameter::Fourth);
    auto innerAngleSet = toolWidget->isParameterSet(WParameter::Sixth);

    if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
        || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points) {
        width = toolWidget->getParameter(WParameter::Fifth);
        radius = toolWidget->getParameter(WParameter::Seventh);
        thickness = toolWidget->getParameter(WParameter::Eighth);

        widthSet = toolWidget->isParameterSet(WParameter::Fifth);
        radiusSet = toolWidget->isParameterSet(WParameter::Seventh);
        thicknessSet = toolWidget->isParameterSet(WParameter::Eighth);
    }

    using namespace Sketcher;

    int firstPointId = firstCurve;
    if (handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::Diagonal
        || handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
        if (dHandler->radius > Precision::Confusion())
            firstPointId = dHandler->constructionPointOneId;
    }
    else {
        firstPointId = dHandler->centerPointId;
    }

    auto constraintx0 = [&]() {
        ConstraintToAttachment(GeoElementId(firstPointId,PointPos::start), GeoElementId::VAxis, x0, handler->sketchgui->getObject());
    };

    auto constrainty0 = [&]() {
        ConstraintToAttachment(GeoElementId(firstPointId,PointPos::start), GeoElementId::HAxis, y0, handler->sketchgui->getObject());
    };

    auto constraintlength = [&]() {
        if (handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points
            || handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
            if (!dHandler->cornersReversed)
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve + 1, 1, firstCurve + 3, 2, fabs(length));
            else
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve, 1, firstCurve + 2, 2, fabs(length));
        }
        else {
            if (Base::sgn(dHandler->thirdCorner.x - dHandler->firstCorner.x) * Base::sgn(dHandler->thirdCorner.y - dHandler->firstCorner.y) > 0)
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve + 1, 1, firstCurve + 3, 2, fabs(length));
            else
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve, 1, firstCurve + 2, 2, fabs(length));
        }
    };

    auto constraintwidth = [&]() {
        if (handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points
            || handler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints) {
            if (!dHandler->cornersReversed)
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve, 1, firstCurve + 2, 2, fabs(width));
            else
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve + 1, 1, firstCurve + 3, 2, fabs(width));
        }
        else {
            if (Base::sgn(dHandler->thirdCorner.x - dHandler->firstCorner.x) * Base::sgn(dHandler->thirdCorner.y - dHandler->firstCorner.y) > 0)
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve, 1, firstCurve + 2, 2, fabs(width));
            else
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%d,%f)) ",
                    firstCurve + 1, 1, firstCurve + 3, 2, fabs(width));
        }
    };

    // NOTE: if AutoConstraints is empty, we can add constraints directly without any diagnose. No diagnose was run.
    if(handler->AutoConstraints.empty()) {
        if(x0set)
            constraintx0();

        if(y0set)
            constrainty0();

        if (lengthSet)
            constraintlength();

        if (widthSet)
            constraintwidth();
    }
    else { // There is a valid diagnose.
        auto firstpointinfo = handler->getPointInfo(GeoElementId(firstPointId, PointPos::start));

        if(x0set && firstpointinfo.isXDoF()) {
            constraintx0();

            handler->diagnoseWithAutoConstraints(); // ensure we have recalculated parameters after each constraint addition

            firstpointinfo = handler->getPointInfo(GeoElementId(firstPointId, PointPos::start)); // get updated point position
        }

        if(y0set && firstpointinfo.isYDoF()) {
            constrainty0();

            handler->diagnoseWithAutoConstraints(); // ensure we have recalculated parameters after each constraint addition
        }

        if (lengthSet) {
            auto startpointinfo = handler->getPointInfo(GeoElementId(firstCurve + 1, PointPos::start));
            auto endpointinfo = handler->getPointInfo(GeoElementId(firstCurve + 3, PointPos::end));

            int DoFs = startpointinfo.getDoFs();
            DoFs += endpointinfo.getDoFs();

            if(DoFs > 0) {
                constraintlength();
            }

            handler->diagnoseWithAutoConstraints();
        }

        if (widthSet) {
            auto startpointinfo = handler->getPointInfo(GeoElementId(firstCurve , PointPos::start));
            auto endpointinfo = handler->getPointInfo(GeoElementId(firstCurve + 2, PointPos::end));

            int DoFs = startpointinfo.getDoFs();
            DoFs += endpointinfo.getDoFs();

            if(DoFs > 0) {
                constraintwidth();
            }
        }
    }

    // NOTE: As of today, there are no autoconstraints on the radius or on the frame thickness, therefore, they are necessarily constrainable were applicable.

    if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints
        || dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::CenterAnd3Points) {

        if (angleSet) {
            if (fabs(angle - M_PI) < Precision::Confusion() || fabs(angle + M_PI) < Precision::Confusion() || fabs(angle) < Precision::Confusion()) {
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Horizontal',%d)) ", firstCurve);
            }
            else if (fabs(angle - M_PI / 2) < Precision::Confusion() || fabs(angle + M_PI / 2) < Precision::Confusion()) {
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Vertical',%d)) ", firstCurve);
            }
            else {
                Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Angle',%d,%d,%f)) ",
                    Sketcher::GeoEnum::HAxis, firstCurve, angle);
            }
        }
        if (innerAngleSet) {
            if (fabs(innerAngle - M_PI / 2) > Precision::Confusion()) {//if 90° then perpendicular already created.
                if (dHandler->constructionMethod() == DrawSketchHandlerRectangle::ConstructionMethod::ThreePoints)
                    Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Angle',%d,%d,%f)) ",
                        firstCurve, firstCurve + 1, innerAngle);
                else
                    Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Angle',%d,%d,%f)) ",
                        firstCurve, firstCurve + 3, innerAngle);
            }
        }
    }

    if (radiusSet && radius > Precision::Confusion())
        Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Radius',%d,%f)) ",
            firstCurve + 5, radius);

    if (thicknessSet) {
        Gui::cmdAppObjectArgs(handler->sketchgui->getObject(), "addConstraint(Sketcher.Constraint('Distance',%d,%d,%d,%f)) ",
            firstCurve + (dHandler->roundCorners == true ? 8 : 4), 1, firstCurve, fabs(thickness));
    }
}


} // namespace SketcherGui


#endif // SKETCHERGUI_DrawSketchHandlerRectangle_H

