#pragma once
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <App/SingletonAccessor.h>

class Mesh;
class Dialogs;
class PlaybackPanel;
class Waveform3D;
class Waveform2D;
class WaveformInter3D;
class WaveformInter2D;
class Interactor;
class MainPanel;
class Console;

class KeyboardInputHandler :
        public KeyListener
    ,   public ApplicationCommandTarget
	,	public SingletonAccessor
{
public:
    enum Command : CommandID {
        CommandNewFile = 0x2000,
        CommandOpenFile,
        CommandSaveFile,
        CommandSaveAsFile,
        CommandUndo,
        CommandRedo,
        CommandEraseSelected,
        CommandExtrudeSelected,

        CommandTogglePlayback,
        CommandTransportToStart,
        CommandTransportToEnd,
        CommandRedToStart,
        CommandRedToEnd,
        CommandBlueToStart,
        CommandBlueToEnd,
        CommandNavigateMorph,
        CommandNextPreset,
        CommandPreviousPreset,
        CommandShowFilterTab,
        CommandShowResynthesisTab,
        CommandTourPrevious,
        CommandTourNext,
        CommandSetAdditiveMode,
        CommandSetFilterMode,
        CommandShowPresetBrowser,
        CommandDeselectAll,
        CommandConnectSelected,
        CommandToggleYellowLink,
        CommandToggleRedLink,
        CommandToggleBlueLink,
        CommandSelectTool,
        CommandPencilTool,
        CommandAxeTool,
        CommandTimeDimension,
        CommandRedDimension,
        CommandBlueDimension,
        CommandToggleMagnitudeDraw,
        CommandToggleWaveOverlay,
        CommandEscape,
        CommandCycleTool,
        CommandDebugDumpPreset,
        CommandDebugPrintMesh
    };

	KeyboardInputHandler(SingletonRepo* repo);

	void init() override;
	bool keyPressed(const KeyPress& key, Component* component) override;
	void setFocusedInteractor(Interactor* interactor, bool);
	Interactor* getCurrentInteractor();
    ApplicationCommandManager& getCommandManager();

    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(Array<CommandID>& commands) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

private:
    bool performCommand(CommandID commandID, const KeyPress& key, Component* component);
    void finishSelectionEdit(bool editedSomething, bool updateBlue);
    void updateLinkedVertexHighlights();

	Ref<Mesh> mesh;
	Ref<PlaybackPanel> position;
	Ref<WaveformInter3D> waveInter3D;
	Ref<WaveformInter2D> wave2DInteractor;
	Ref<Waveform3D> surface;
	Ref<Waveform2D> wave2D;
	Ref<MainPanel> main;
	Ref<Console> console;

//	Panel* currentPanel;
    ApplicationCommandManager commandManager;
	bool isMeshInteractor;
	Interactor* currentInteractor;
};
