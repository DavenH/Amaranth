#include "EditWatcher.h"
#include "SingletonRepo.h"
#include "../Definitions.h"
#include "../App/AppConstants.h"
#include "../UI/IConsole.h"

EditWatcher::EditWatcher(SingletonRepo* repo) :
        SingletonAccessor(repo, "EditWatcher")
    ,   editedWithoutUndo(false) {
}

void EditWatcher::update(bool justUpdateTitle) {
    String currPreset   = getStrConstant(DocumentName);
    String prodName     = getStrConstant(ProductName);

    String titleBar = (prodName + " - ") + currPreset;

    if (undoManager.canUndo() || editedWithoutUndo) {
        titleBar += String("*");
    }

    clients.call(&Client::updateTitle, titleBar);

    if (! justUpdateTitle) {
        clients.call(&Client::notifyChange);
    }
}

bool EditWatcher::getHaveEdited() {
    return undoManager.canUndo() || editedWithoutUndo;
}

void EditWatcher::reset() {
    undoManager.clearUndoHistory();
    editedWithoutUndo = false;

    update(true);
}

bool EditWatcher::addAction(NamedUndoableAction* action, bool startNewTransaction) {
    if (startNewTransaction) {
        undoManager.beginNewTransaction();
    }

    const String& description = action->getDescription();
    return undoManager.perform(action, description);
}

bool EditWatcher::addAction(UndoableAction* action, bool startNewTransaction) {
    if (startNewTransaction) {
        undoManager.beginNewTransaction();
}

    return undoManager.perform(action);
}

void EditWatcher::setHaveEditedWithoutUndo(bool have) {
    editedWithoutUndo = have;
    triggerAsyncUpdate();
}

void EditWatcher::undo() {
    if (undoManager.undo()) {
        update(true);
    } else {
        showCritical("No undo available");
    }
}

void EditWatcher::redo() {
    if (undoManager.redo()) {
        update(true);
    } else {
        showCritical("No redo available");
    }
}
