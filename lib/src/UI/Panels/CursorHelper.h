#pragma once

#include "../MiscGraphics.h"
#include "../../Inter/Interactor.h"
#include "../../App/Settings.h"
#include "../../App/SingletonRepo.h"

class CursorHelper {
public:
    static MouseCursor getCursor(SingletonRepo* repo, Interactor* i) {
        if (i == nullptr) {
            return MouseCursor::NormalCursor;
        }

        PanelState& state = i->state;
        bool is3D         = i->is3DInteractor();
        int tool          = getSetting(Tool);
        int highlitCorner = getStateValue(HighlitCorner);

        const ModifierKeys& mods = ModifierKeys::getCurrentModifiers();

        if (highlitCorner >= 0) {
            if (highlitCorner == PanelState::RotateHandle) {
                return MouseCursor::DraggingHandCursor;
            } else if (highlitCorner == PanelState::MoveHandle) {
                return MouseCursor::UpDownLeftRightResizeCursor;
            } else if (highlitCorner % 2 == PanelState::LeftRightCorner) {
                return MouseCursor::LeftRightResizeCursor;
            } else if (highlitCorner % 2 == PanelState::DiagCorner) {
                return MouseCursor::UpDownResizeCursor;
            }
        } else if (tool == Tools::Nudge) {
            return i->isCursorApplicable(tool)
                       ? getObj(MiscGraphics).getCursor(MiscGraphics::NotApplicableCursor)
                       : is3D
                             ? MouseCursor::UpDownResizeCursor
                             : MouseCursor::LeftRightResizeCursor;
        } else if (tool == Tools::Pencil || tool == Tools::Axe) {
            if (i->is3DInteractor() ^ (tool == Tools::Axe)) {
                return getObj(MiscGraphics).getCursor(MiscGraphics::NotApplicableCursor);
            } else if (tool == Tools::Pencil) {
                return getObj(MiscGraphics).getCursor(
                    mods.isRightButtonDown() ? MiscGraphics::PencilEditCursor : MiscGraphics::PencilCursor);
            } else if (tool == Tools::Axe) {
                return MouseCursor::NoCursor;
            }
        } else if (i->state.actionState == PanelState::BoxSelecting) {
            return getObj(MiscGraphics).getCursor(
                mods.isShiftDown()
                    ? (mods.isCommandDown() ? MiscGraphics::CrossSubCursor : MiscGraphics::CrossAddCursor)
                    : MiscGraphics::CrossCursor);
        } else if (i->mouseFlag(WithinReshapeThresh)) {
            return MouseCursor::UpDownResizeCursor;
        }

        return getObj(MiscGraphics).getCursor(MiscGraphics::CrossCursor);
    }

    static void setCursor(SingletonRepo* repo, Component* c, Interactor* i) {
        if (c != nullptr) {
            c->setMouseCursor(getCursor(repo, i));
        }
    }
};
