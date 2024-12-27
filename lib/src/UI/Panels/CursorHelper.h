#pragma once

#include "../MiscGraphics.h"
#include "../../Inter/Interactor.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"

class CursorHelper {
public:
    static void setCursor(SingletonRepo* repo, Component* c, Interactor* i) {
        PanelState& state = i->state;

        if (!i) {
            return;
        }

        bool is3D         = i->is3DInteractor();
        int tool          = getSetting(Tool);
        int highlitCorner = getStateValue(HighlitCorner);

        const ModifierKeys& mods = ModifierKeys::getCurrentModifiers();

        if (highlitCorner >= 0) {
            if (highlitCorner == PanelState::RotateHandle) {
                c->setMouseCursor(MouseCursor::DraggingHandCursor);
            } else if (highlitCorner == PanelState::MoveHandle) {
                c->setMouseCursor(MouseCursor::UpDownLeftRightResizeCursor);
            } else if (highlitCorner % 2 == PanelState::LeftRightCorner) {
                c->setMouseCursor(MouseCursor::LeftRightResizeCursor);
            } else if (highlitCorner % 2 == PanelState::DiagCorner) {
                c->setMouseCursor(MouseCursor::UpDownResizeCursor);
            }
        } else if (tool == Tools::Nudge) {
            c->setMouseCursor(i->isCursorApplicable(tool)
                                  ? getObj(MiscGraphics).getCursor(MiscGraphics::NotApplicableCursor)
                                  : is3D
                                        ? MouseCursor::UpDownResizeCursor
                                        : MouseCursor::LeftRightResizeCursor);
        } else if (tool == Tools::Pencil || tool == Tools::Axe) {
            if (i->is3DInteractor() ^ (tool == Tools::Axe)) {
                c->setMouseCursor(getObj(MiscGraphics).getCursor(MiscGraphics::NotApplicableCursor));
            } else {
                if (tool == Tools::Pencil) {
                    c->setMouseCursor(
                        getObj(MiscGraphics).getCursor(
                            mods.isRightButtonDown() ? MiscGraphics::PencilEditCursor : MiscGraphics::PencilCursor));
                } else if (tool == Tools::Axe) {
                    c->setMouseCursor(MouseCursor::NoCursor);
                }
            }
        } else if (i->state.actionState == PanelState::BoxSelecting) {
            c->setMouseCursor(
                getObj(MiscGraphics).getCursor(
                    mods.isShiftDown()
                        ? (mods.isCommandDown() ? MiscGraphics::CrossSubCursor : MiscGraphics::CrossAddCursor)
                        : MiscGraphics::CrossCursor));
        } else if (i->mouseFlag(WithinReshapeThresh)) {
            c->setMouseCursor(MouseCursor::UpDownResizeCursor);
        } else {
            c->setMouseCursor(getObj(MiscGraphics).getCursor(MiscGraphics::CrossCursor));
        }
    }
};
