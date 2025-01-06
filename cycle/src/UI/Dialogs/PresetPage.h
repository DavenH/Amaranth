#pragma once

#include <set>
#include <vector>
#include "JuceHeader.h"
#include <UI/Widgets/IconButton.h>
#include <Util/FilterableList.h>
#include <App/Doc/DocumentDetails.h>
#include <App/SingletonAccessor.h>

template<class DocumentDetails> class SearchField;

using std::vector;
using std::set;

class RetractableCallout;
class PulloutComponent;

class PresetPage :
		public MultiTimer
	,	public juce::Component
	,	public TableListBoxModel
	,	public FilterableList<DocumentDetails>
	,	public ComponentMovementWatcher
	,	public TextEditor::Listener
	,	public Button::Listener
	,	public Label::Listener
	,	public ComboBox::Listener
	, 	public SingletonAccessor
	, 	public AsyncUpdater
{

public:
	enum ButtonEnum { PrevButton, NextButton };
	enum TimerIds 	{ SpinAnimId, DownloadPresetsId, DelayLoadId };

	PresetPage(SingletonRepo* repo);

	~PresetPage() override;

	void init() override;

	Array<DocumentDetails, CriticalSection>& getAllItems();
	bool containsString(DocumentDetails& details, const String& str) override;
	bool keyPressed(const KeyPress& key) override;
	bool removeDetailsFrom(Array<DocumentDetails, CriticalSection>& list, int code);
	Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component* existingComponentToUpdate) override;
	File getPresetSettingsFile();
	IconButton* createIconButton(const DocumentDetails& details, int rowNumber, int columnId);

	int getColumnAutoSizeWidth (int columnId) override;
	int getIndexOfSelected() const;
	int getNumRows() override;

	String downloadPreset(DocumentDetails& deets);
	String getProgramName(int index) const;
	String getWavDirectory() const { return wavDirectory.getText(); }
	String populateRemoteNames();

	void addFilesToArray(Array<DocumentDetails, CriticalSection>& list, const String& path, const String& extension);
	void addFilesToDefaultArray(const String& path, const String& extension);
	void addItem(const DocumentDetails& details);
	void addItems(const Array<DocumentDetails, CriticalSection>& deets);
	void addToRevisionMap(const Array<DocumentDetails, CriticalSection>& array);
	void addToRevisionMap(const DocumentDetails& deets);

	void backgroundClicked(const MouseEvent&) override;
	void buttonClicked(Button* button) override;
	void cellClicked (int rowNumber, int columnId, const MouseEvent& e) override;
	void cellDoubleClicked (int rowNumber, int columnId, const MouseEvent& e) override;
	void cleanup();
	void clearSearch();
	void comboBoxChanged (ComboBox* comboBoxThatHasChanged) override;
    void componentMovedOrResized (bool wasMoved, bool wasResized) override {}
    void componentPeerChanged() override {}
    void componentVisibilityChanged() override;
	void doChangeUpdate() override;
	void downloadCommunityDocumentDetails();
	void grabSearchFocus();
	void handleAsyncUpdate() override;
	void killThreads();
	void labelTextChanged (Label* labelThatHasChanged) override;
	void loadItem(const DocumentDetails& details, bool ignoreSave = false);
	void loadItem(int itemIndex, bool ignoreSave = false);
	void loadPendingItem();
	void paint(Graphics& g) override;
	void paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
	void paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
	void presetDirUpdated();
	void ratingChanged(float rating, int row);
	void readPresetSettings();
	void refresh();
	void resetListAndLoadItem(int itemIndex);
	void resized() override;
	void returnKeyPressed (int lastRowSelected) override;
	void selectedRowsChanged (int lastRowSelected) override;
	void setUploadResponse(const String& response);
	void setWavDirectory(const String& dir);
	void sortOrderChanged (int newSortColumnId, bool isForwards) override;
	void startSpinAnimation();
	void stopSpinAnimation();
	void timerCallback(int id) override;
	void triggerAsyncLoad(int index);
	void triggerButtonClick(ButtonEnum button);
	void triggerLoadItem() override;
	void updateCurrentPreset(const String& name);
	void updateNetItems(const Array<DocumentDetails, CriticalSection>& deets);
	void updatePresetIndex();
	void updateTags(int index);
	void writePresetSettings();

private:
	class UploadThread : public Thread, public SingletonAccessor {
	public:
		explicit UploadThread(SingletonRepo* repo) : Thread("UploadThread"), SingletonAccessor(repo, "UploadThread") {
		}

		void run() override;

		void setFile(const File& file) { uploadFile = file; }

	private:
		File uploadFile;
	};

	class DownloadDetailsThread : public Thread, public SingletonAccessor {
	public:
		explicit DownloadDetailsThread(SingletonRepo* repo) : Thread("DownloadDetailsThread"),
		                                             SingletonAccessor(repo, "DownloadDetailsThread") {
		}

		void run() override;

	private:
	};

	class DownloadPresetThread : public Thread, public SingletonAccessor {
	public:
		explicit DownloadPresetThread(SingletonRepo* repo) : Thread("DownloadPresetThread"),
		                                            SingletonAccessor(repo, "DownloadPresetThread") {
		}

		void run() override;

		void setDetails(const DocumentDetails& deets) { this->deets = deets; }
		DocumentDetails& getDetails() { return deets; }

	private:
		DocumentDetails deets;
	};

	bool haveKeyAndEqualRevision(int code, int revision);

	bool spinAnimating;
	bool pendingSettings;

	int pendingPresetIndex;
	int currPresetFiltIndex;

	float spinAngle;

	std::unique_ptr<TableListBox> tableListBox;
	std::unique_ptr<SearchField<DocumentDetails>> search;
	std::unique_ptr<Button> upButton;
	std::unique_ptr<IconButton> wavFolderButton;

	DocumentDetails pendingDeetsToLoad;

	Label tagsLabel;
	Label tagsField;
	Label wavDirectory;
	Label wavLabel;
	Label subfolderLabel;

	IconButton verts;
	IconButton wave;
	IconButton netIcon;
	IconButton prevIcon;
	IconButton nextIcon;

	Image progressImage, folderImage, dismissImage;

	ComboBox subfolderBox;
	Component wavControls;

	PulloutComponent* 	selectPO;
	PulloutComponent* 	navPO;
	RetractableCallout* selectCO;
	RetractableCallout* navCO;

	UploadThread uploadThread;
	DownloadDetailsThread downloadDeetsThread;
	DownloadPresetThread downloadPresetThread;

	Array<File> subfolders;
	Array<int> newlyDownloaded;
	Array<DocumentDetails, CriticalSection> remoteDetails;
	Array<DocumentDetails, CriticalSection> localDetails;

	set<int> dismissedSet;
	set<int> downloadedSet;
	HashMap<int, float> ratingsMap;
	HashMap<int, int> dldedRevisionMap;

	friend class DownloadDetailsThread;
};