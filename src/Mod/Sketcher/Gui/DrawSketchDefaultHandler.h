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


#ifndef SKETCHERGUI_DrawSketchDefaultHandler_H
#define SKETCHERGUI_DrawSketchDefaultHandler_H

#include "DrawSketchHandler.h"

#include "Utils.h"

namespace bp = boost::placeholders;

namespace SketcherGui {


/************************ List of snap mods ************************************/

    enum class SnapMode {
        Free,
        Snap5Degree,
        SnapToObject,
        SnapToGrid
    };
/*********************** Ancillary classes for DrawSketch Hierarchy *******************************/

namespace StateMachines {

enum class OneSeekEnd {
    SeekFirst,
    End // MUST be the last one
};

enum class TwoSeekEnd {
    SeekFirst,
    SeekSecond,
    End // MUST be the last one
};

enum class ThreeSeekEnd {
    SeekFirst,
    SeekSecond,
    SeekThird,
    End // MUST be the last one
};

enum class FourSeekEnd {
    SeekFirst,
    SeekSecond,
    SeekThird,
    SeekFourth,
    End // MUST be the last one
};

enum class TwoSeekDoEnd {
    SeekFirst,
    SeekSecond,
    Do,
    End // MUST be the last one
};

} // namespace StateMachines

/** @brief A state machine to encapsulate a state
 *
 * @details
 *
 * A template class for a state machine defined by template type SelectModeT,
 * automatically initialised to the first state, encapsulating the actual state,
 * and enabling to change the state, while generating a call to onModeChanged()
 * after every change.
 *
 * the getNextMode() returns the next mode in the state machine, unless it is in
 * End mode, in which End mode is returned.
 *
 * NOTE: The machine provided MUST include a last state named End.
 */
template <typename SelectModeT>
class StateMachine
{
public:
    StateMachine():Mode(static_cast<SelectModeT>(0)) {}
    virtual ~StateMachine(){}

protected:
    void setState(SelectModeT mode) {
        Mode = mode;
        onModeChanged();
    }

    SelectModeT state() {
        return Mode;
    }

    bool isState(SelectModeT state) { return Mode == state;}

    SelectModeT getNextMode() {
        auto modeint = static_cast<int>(state());


        if(modeint < maxMode) {
            auto newmode = static_cast<SelectModeT>(modeint+1);
            return newmode;
        }
        else {
            return SelectModeT::End;
        }
    }

    void moveToNextMode() {
        setState(getNextMode());
    }

    void reset() {
        setState(static_cast<SelectModeT>(0));
    }

    virtual void onModeChanged() {};

private:
    SelectModeT Mode;
    static const constexpr int maxMode = static_cast<int>(SelectModeT::End);

};

/** @brief A state machine DrawSketchHandler for geometry.
 *
 * @details
 * A state machine DrawSketchHandler defining a EditCurve and AutoConstraints, providing:
 * - generic initialisation including setting the cursor
 * - structured command finalisation
 * - handling of continuous creation mode
 *
 * Two ways of using it:
 * 1. By instanting and specialising functions.
 * 2. By creating a new class deriving from this one
 *
 * You need way 2 if you must add additional data members to your class.
 *
 * This class provides an NVI interface for extension. Alternatively, specialisation of those functions
 * may be effected if it is opted to instantiate and specialise.
 *
 * Template Types/Parameters:
 * PTool : Parameter to specialise behaviour to a specific tool
 * SelectModeT : The type of statemachine to be used (see namespace StateMachines above).
 * PInitEditCurveSize : Initial size of the EditCurve vector
 * PInitAutoConstraintSize : Initial size of the AutoConstraint vector
 *
 * Question 1: Do I need to use this handler or derive from this handler to make a new hander?
 *
 * No, you do not NEED to. But you are encouraged to. Structuring a handler following this NVI, apart
 * from savings in amount of code typed, enables a much easier and less verbose implementation of a handler
 * using a default widget (toolwidget).
 *
 * For handlers using a custom widget it will also help by structuring the code in a way consistent with other handlers.
 * It will result in an easier to maintain code.
 *
 * Question 2: I want to use the default widget, do I need to use this handler or derive from this handler?
 *
 * You should use DrawSketchDefaultWidgetHandler instead. However, both clases use the same interface, so if you derive from
 * this class when implementing your handler and then decide to use the tool widget, all you have to do is to change
 * the base class from DrawSketchDefaultHandler to DrawSketchDefaultWidgetHandler. Then you will have to implement the code that
 * is exclusively necessary for the default widget to work.
 */
template < typename HandlerT,         // The geometry tool for which the template is created (See GeometryTools above)
           typename SelectModeT,        // The state machine defining the states that the handle iterates
           int PInitEditCurveSize,      // The initial size of the EditCurve
           int PInitAutoConstraintSize> // The initial size of the AutoConstraint>
class DrawSketchDefaultHandler: public DrawSketchHandler, public StateMachine<SelectModeT>
{
public:
    DrawSketchDefaultHandler():   initialEditCurveSize(PInitEditCurveSize)
                                , EditCurve(PInitEditCurveSize)
                                ,sugConstraints(PInitAutoConstraintSize)
    {
        applyCursor();
    }

    virtual ~DrawSketchDefaultHandler() = default;

    /** @name public DrawSketchHandler interface
     * NOTE: Not intended to be specialised. It calls some functions intended to be
     * overridden/specialised instead.
     */
    //@{
    virtual void mouseMove(Base::Vector2d onSketchPos) override
    {
        updateDataAndDrawToPosition(onSketchPos);
    }

    virtual bool pressButton(Base::Vector2d onSketchPos) override
    {

        onButtonPressed(onSketchPos);
        return true;
    }

    virtual bool releaseButton(Base::Vector2d onSketchPos) override {
        Q_UNUSED(onSketchPos);
        finish();
        return true;
    }
    //@}

    bool getSnapPosition(SnapMod snapMod, Base::Vector2d& pointToOverride, Base::Vector2d referencePoint = Base::Vector2d(0., 0.)) {
        //Snap to grid should probably be a toggle button. I would say in taskview near 'show grid' and 'grid size' we could add a checkbox 'Snap to grid'.
        //Then the snap mod should be set here to SnapToGrid by looking at the pref.

        if (snapMod == SnapMod::Free)
            return false;
        else if (snapMod == SnapMod::SnapToObject || snapMod == SnapMod::Snap5Degree || snapMod == SnapMod::SnapToGrid) {
            //If we are using Snap5Degree or SnapToGrid we still want to snap to object if there is an object preselected. They are kind of a sub-type of SnapToObject.
            Sketcher::SketchObject* Obj = sketchgui->getSketchObject();
            int geoId = GeoEnum::GeoUndef;
            Sketcher::PointPos posId = Sketcher::PointPos::none;

            int VtId = getPreselectPoint();
            int CrsId = getPreselectCross();
            int CrvId = getPreselectCurve();

            if (CrsId == 0 || VtId >= 0) {
                if (CrsId == 0) {
                    geoId = Sketcher::GeoEnum::RtPnt;
                    posId = Sketcher::PointPos::start;
                }
                else if (VtId >= 0) {
                    Obj->getGeoVertexIndex(VtId, geoId, posId);
                }

                if (geoId != GeoEnum::GeoUndef) {
                    pointToOverride.x = Obj->getPoint(geoId, posId).x;
                    pointToOverride.y = Obj->getPoint(geoId, posId).y;
                    return true;
                }
            }
            else if (CrsId == 1) { //H_Axis
                pointToOverride.y = 0;
                return true;
            }
            else if (CrsId == 2) { //V_Axis
                pointToOverride.x = 0;
                return true;
            }
            else if (CrvId >= 0 || CrvId <= Sketcher::GeoEnum::RefExt) { //Curves
                const Part::Geometry* geo = Obj->getGeometry(CrvId);
                //Todo: find a way to snap to infinite line (if line is in view). Currently it's not possible because infinite line doesn't get preselected.
                //Todo: find how to project point on ellipse/parabola/hyperbola...
                if (geo->getTypeId() == Part::GeomLineSegment::getClassTypeId()) {
                    Part::GeomLineSegment* line = static_cast<Part::GeomLineSegment*>(geo);
                    Base::Vector2d startPoint = Base::Vector2d(line->getStartPoint().x, line->getStartPoint().y);
                    Base::Vector2d endPoint = Base::Vector2d(line->getEndPoint().x, line->getEndPoint().y);
                    pointToOverride.ProjectToLine(pointToOverride - startPoint, endPoint - startPoint);
                    pointToOverride = startPoint + pointToOverride;
                    return true;
                }
                else if (geo->getTypeId() == Part::GeomCircle::getClassTypeId()) {
                    Part::GeomCircle* circle = static_cast<Part::GeomCircle*>(geo);
                    Base::Vector2d centerPoint = Base::Vector2d(circle->getCenter().x, circle->getCenter().y);

                    Base::Vector2d v = pointToOverride - centerPoint;
                    pointToOverride = centerPoint + v * circle->getRadius() / v.Length();
                    return true;
                }
                else if (geo->getTypeId() == Part::GeomArcOfCircle::getClassTypeId()) {
                    Part::GeomArcOfCircle* circle = static_cast<Part::GeomArcOfCircle*>(geo);
                    Base::Vector2d centerPoint = Base::Vector2d(circle->getCenter().x, circle->getCenter().y);

                    Base::Vector2d v = pointToOverride - centerPoint;
                    pointToOverride = centerPoint + v * circle->getRadius() / v.Length();
                    return true;
                }
                else if (geo->getTypeId() == Part::GeomEllipse::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomArcOfEllipse::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomArcOfHyperbola::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomArcOfParabola::getClassTypeId()) {}
                else if (geo->getTypeId() == Part::GeomBSplineCurve::getClassTypeId()) {}
            }
        }
        else if (snapMod == SnapMod::Snap5Degree) {
            double length = (pointToOverride - referencePoint).Length();
            double angle = (pointToOverride - referencePoint).Angle();
            angle = round(angle / (M_PI / 36)) * M_PI / 36;
            pointToOverride = referencePoint + length * Base::Vector2d(cos(angle), sin(angle));
            return true;
        }
        else if (snapMod == SnapMod::SnapToGrid) {
            double gridSize = sketchgui->GridSize.getValue();
            int nx = floor(pointToOverride.x / gridSize);
            int ny = floor(pointToOverride.y / gridSize);
            int signX = static_cast<int>(Base::sgn(pointToOverride.x));
            int signY = static_cast<int>(Base::sgn(pointToOverride.y));
            if (pointToOverride.x < (nx + signX * 0.5) * gridSize)
                pointToOverride.x = nx * gridSize;
            else
                pointToOverride.x = (nx + signX * 1) * gridSize;

            if (pointToOverride.y < (ny + signY * 0.5) * gridSize)
                pointToOverride.y = ny * gridSize;
            else
                pointToOverride.y = (ny + signY * 1) * gridSize;
            return true;
        }
        return false;
    }

protected:
    using SelectMode = SelectModeT;
    using ModeStateMachine = StateMachine<SelectModeT>;


    /** @name functions NOT intended for specialisation or to be hidden
        These functions define a predefined structure and are extendable using NVI.
        1. Call them from your handle
        2. Do not hide them or otherwise redefine them UNLESS YOU HAVE A REALLY GOOD REASON TO
        3. EXTEND their functionality, if needed, using the NVI interface (or if you do not need to derive, by specialising these functions).*/
    //@{
    /** @brief This function finalises the creation operation. It works only if the state machine is in state End.
    *
    * @details
    * The functionality need to be provided by extending these virtual private functions:
    * 1. executeCommands() : Must be provided with the Commands to create the geometry
    * 2. beforeCreateAutoConstraints() : Enables derived clases to define specific actions before executeCommands and createAutoConstraints (optional).
    * 3. createAutoConstraints() : Must be provided with the commands to create autoconstraints
    *
    * It recomputes if not solves and handles continuous mode automatically
    */
    void finish() {

        if(this->isState(SelectMode::End)) {
            unsetCursor();
            resetPositionText();

            executeCommands();

            beforeCreateAutoConstraints();

            createAutoConstraints();

            tryAutoRecomputeIfNotSolve(static_cast<Sketcher::SketchObject *>(sketchgui->getObject()));

            handleContinuousMode();
        }
    }

    /** @brief This function resets the handler to the initial state.
    *
    * @details
    * The functionality can be extended using these virtual private function:
    * 1. onReset() : Any further initialisation applicable to your handler
    *
    * It clears the edit curve, resets the state machine and resizes edit curve and autoconstraints
    * to initial size. Reapplies the cursor bitmap.
    */
    void reset() {
        EditCurve.clear();
        drawEdit(EditCurve);

        ModeStateMachine::reset();
        EditCurve.resize(initialEditCurveSize);
        for(auto & ac : sugConstraints)
            ac.clear();

        onReset();
        applyCursor();
    }

    /** @brief This function handles the geometry continuous mode.
    *
    * @details
    * The functionality can be extended using the virtual private function called from reset(), namely:
    * 1. onReset() : Any further initialisation applicable to your handler
    *
    * It performs all the operations in reset().
    */
    void handleContinuousMode() {

        if(continuousMode){
            // This code enables the continuous creation mode.
            reset();
            // It is ok not to call to purgeHandler in continuous creation mode because the
            // handler is destroyed by the quit() method on pressing the right button of the mouse
        }
        else{
            sketchgui->purgeHandler(); // no code after this line, Handler get deleted in ViewProvider
        }
    }
    //@}

private:
    /** @name functions are intended to be overridden/specialised to extend basic functionality
        NVI interface. See documentation of the functions above.*/
    //@{
    virtual void onReset() { }
    virtual void executeCommands() {}
    virtual void beforeCreateAutoConstraints() {}
    virtual void createAutoConstraints() {}

    virtual void onModeChanged() override { };

    virtual void updateDataAndDrawToPosition(Base::Vector2d onSketchPos) {Q_UNUSED(onSketchPos)};
    //@}
protected:
    /** @name functions are intended to be overridden/specialised to extend basic functionality
        See documentation of the functions above*/
    //@{
    /// Handles avoid redundants and continuous mode, if overridden the base class must be called!
    virtual void activated() override {
        avoidRedundants = sketchgui->AvoidRedundant.getValue()  && sketchgui->Autoconstraints.getValue();

        ParameterGrp::handle hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/Mod/Sketcher");

        continuousMode = hGrp->GetBool("ContinuousCreationMode",true);
    }

    // Default implementation is that on every mouse click it redraws and the mode is changed to the next seek
    // On the last seek, it changes to SelectMode::End
    // If this behaviour is not acceptable, then the function must be specialised (or overloaded).
    virtual void onButtonPressed(Base::Vector2d onSketchPos) {
        this->updateDataAndDrawToPosition(onSketchPos);
        this->moveToNextMode();
    }
    //@}

protected:
    // The initial size may need to change in some tools due to the configuration of the tool, so resetting may lead to a
    // different number than the compiled time value
    int initialEditCurveSize;

    std::vector<Base::Vector2d> EditCurve;
    std::vector<std::vector<AutoConstraint>> sugConstraints;

    bool avoidRedundants;
    bool continuousMode;
};

} // namespace SketcherGui


#endif // SKETCHERGUI_DrawSketchDefaultHandler_H

