#pragma once

#include <vector>
#include <App/Doc/Savable.h>
#include <App/MeshLibrary.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/ParameterGroup.h>
#include "JuceHeader.h"
#include "../TourGuide.h"

using std::vector;

class SynthAudioSource;
class MorphPanel;

class ModMatrixPanel;

class ModMatrix :
        public Component
    ,	public TableListBoxModel
    , 	public SingletonAccessor
{
public:
    ModMatrix(SingletonRepo* repo, ModMatrixPanel* panel);
    ~ModMatrix() override = default;
    void resized() override;
    int getNumRows() override;
    void cellClicked (int rowNumber, int columnId, const MouseEvent& e) override {}

    Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                       Component* existingComponentToUpdate) override;

    void paintRowBackground (Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell (Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cycleDimensionMapping(int row, int col);
    int columnIdToIndex(int col);

    TableListBox& getListbox() { return *tableListBox; }

    class ColourCheckbox : public Component
    {
    public:
        ColourCheckbox(ModMatrix* matrix, int row, int col) :
                row(row)
            ,	col(col)
            ,	modMatrix(matrix)
        {
            setMouseCursor(MouseCursor::PointingHandCursor);
        }

        void paint(Graphics& g) override;

        void mouseDown(const MouseEvent &e) override {
            if (e.mods.isLeftButtonDown()) {
                modMatrix->cycleDimensionMapping(row, col);
            } else if (e.mods.isRightButtonDown()) {
                modMatrix->showDimensionsPopup(this);
            }

            repaint();
        }

        void mouseUp(const MouseEvent &e) override {
            repaint();
        }

        Ref<ModMatrix> modMatrix;
        int row, col;
    };

    void showDimensionsPopup(ColourCheckbox* checkbox);

    Ref<ModMatrixPanel> panel;

private:
    std::unique_ptr<TableListBox> tableListBox;

    JUCE_LEAK_DETECTOR(ModMatrix)
};

class ModMatrixPanel :
        public juce::Component
    ,	public Button::Listener
    ,	public ComboBox::Listener
    ,	public ScrollBar::Listener
    ,	public MeshLibrary::Listener
    ,	public ParameterGroup::Worker
    ,	public MessageListener
    , 	public SingletonAccessor
    ,	public TourGuide
    , 	public Savable
{
public:
    enum { gridSize = 36 };
    enum { NullDim = -1, YellowDim, RedDim, BlueDim };

    enum InputId
    {
            VoiceTime 		= 1
        ,	Velocity
        ,	InvVelocity
        ,	KeyScale
        ,	Aftertouch
        ,	MidiController 	= 100
        ,	ModWheel
        ,	Utility			= 200
    };

    enum OutputId
    {
            SetAllId 		= 50
        ,	TimeSurfId 		= 100
        ,	HarmMagId 		= 200
        ,	HarmPhsId 		= 300
        ,	VolEnvId 		= 400
        ,	PitchEnvId		= 450
        ,	ScratchEnvId 	= 500
        ,	NextId 			= 1000
        ,
    };

    enum OldOutputId
    {
            OldSetAllId 	= 50
        ,	OldTimeSurfId 	= 100
        ,	OldHarmMagId 	= 200
        ,	OldHarmPhsId 	= 300
        ,	OldVolEnvId 	= 400
        ,	OldPitchEnvId	= 401
        ,	OldScratchEnvId = 500
        ,	OldNextId 		= 1000
        ,
    };

    enum MenuIds
    {
            MeshTypes
        ,	WaveshapeTypes
        ,	SpectTypes
        ,	EnvTypes
        ,	VolumeEnvLayers
        , 	PitchEnvLayers
        ,	ScratchLayers
        ,	WaveshapeLayers
        ,	HarmMagLayers
        ,	HarmPhsLayers
    };

    /* ----------------------------------------------------------------------------- */

    struct Mapping {
        int in, out, dim;

        Mapping() : in(-1), out(-1), dim(-1) {}
        Mapping(int in, int out, int dim) : in(in), out(out), dim(dim) {}

        bool operator<(const Mapping& other) const {
            return in * 1000000 + out * 1000 + dim < other.in * 1000000 + other.out * 1000 + other.dim;
        }

        bool operator==(const Mapping& other) const {
            return in == other.in && out == other.out && dim == other.dim;
        }
    };

    struct HeaderElement
    {
        HeaderElement(): id(0), rowDim(0) {
        }

        HeaderElement(const String& name, int id) : name(name), id(id), rowDim(-1) {}
        String name;
        String shortName;
        int id;
        int rowDim;
    };

    /* ----------------------------------------------------------------------------- */

    explicit ModMatrixPanel(SingletonRepo* repo);

    bool isNotInInputs(int id);
    bool isNotInOutputs(int id);
    bool keyPressed(const KeyPress& press) override;
    bool shouldTriggerGlobalUpdate(Slider* slider) override;
    bool shouldTriggerLocalUpdate(Slider* slider) override;
    bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;

    float getUtilityValue(int utilityIndex);
    int getMappingOrder(int columnId);
    int indexOfMapping(int inputId, int outputId);

    Component* getComponent(int which) override;
    ModMatrix& getModMatrix() { return modMatrix; }
    PopupMenu getInputMenu();
    PopupMenu getOutputMenu(int source);

    String getDimString(int dim);
    String getInputName(int id);
    String getInputShortName(int id);
    String getOutputName(int id);

    void init() override;
    void initializeDefaults();
    void resized() override;
    void selfSize();

    void paint(Graphics& g) override;
    void addDestination(int id, bool update = true);
    void addInput(int id, bool input = true);
    void addToMenu(int id, PopupMenu& menu);
    void buttonClicked(Button* button) override;
    void comboBoxChanged(ComboBox* combobox) override;
    void cycleDimensionMapping(int row, int col);
    void getIds(int row, int col, int& intputId, int& outputId);
    void handleMessage (const Message& message) override;
    void layerAdded(int type, int index) override;
    void layerRemoved(int type, int index) override;
    void mappingChanged(int mappingIndex, int inputId, int outputId, int oldDim, int newDim);
    void route(float value, int inputId, int voiceIndex = -1);
    void scrollBarMoved(ScrollBar* bar, double newRange) override;
    void setMatrixCell(int inputId, int outputId, int dim);
    void setUtilityValue(int utilityIndex, float value);
    MeshLibrary::GroupLayerPair toLayerIndex(int outputId);

    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    void setPendingFocusGrab(bool val);

    Array<Mapping> mappings;
    Array<HeaderElement> inputs, outputs;

    Image lockImage;

private:
    void layerAddedOrRemoved(bool added, int type, int index);

    friend class ModMatrix;
    friend class ModMatrixViewPort;

    bool pendingFocusGrab;
    int dfltModMapping;

    Ref<SynthAudioSource> audio;
    Ref<MorphPanel> morphPanel;

    ModMatrix modMatrix;
    CriticalSection mappingLock;

    Label inputLabel, destLabel;
    Component utilityArea, sourceArea, destArea;
    TextButton addInputBtn, addDestBtn, closeButton;
};