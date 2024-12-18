#pragma once

#include "Doc/Document.h"
#include "../App/SingletonAccessor.h"
#include "../Inter/UndoableActions.h"
#include "JuceHeader.h"

class EditWatcher :
        public Document::Listener
    ,	public SingletonAccessor
    ,	public AsyncUpdater {
public:
    class Client {
    public:
        virtual void updateTitle(const String& name) {}
        virtual void notifyChange() {}
    };

    /* ----------------------------------------------------------------------------- */

    explicit EditWatcher(SingletonRepo* repo);
    ~EditWatcher() override = default;

    void mouseDrag(const MouseEvent& e);
    void mouseDown(const MouseEvent& e);

    void undo();
    void redo();

    void reset() override;
    bool getHaveEdited();
    void setHaveEditedWithoutUndo(bool have);
    void update(bool justUpdateTitle = false);

    void addClient(Client* client) 			{ clients.add(client); }

    void documentAboutToLoad() override 	{ reset(); 			   }
    void documentHasLoaded() override 		{ update(); 		   }
    void handleAsyncUpdate() override		{ update(); 		   }

    bool addAction(NamedUndoableAction* action, bool startNewTransaction = true);
    bool addAction(UndoableAction* action, bool startNewTransaction = true);

    UndoManager& getUndoManager() 			{ return undoManager; }
    void beginNewTransaction(const String& name) { undoManager.beginNewTransaction(name); }

private:
    bool editedWithoutUndo;

    UndoManager undoManager;
    ListenerList<Client> clients;
};
