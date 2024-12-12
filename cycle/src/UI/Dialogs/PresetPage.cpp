#include <algorithm>

#include <App/Doc/Document.h>
#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>
#include <UI/Widgets/CalloutUtils.h>
#include <Util/Arithmetic.h>
#include <Util/MicroTimer.h>
#include <UI/Widgets/SearchField.h>

#include "PresetPage.h"

#include "../Widgets/RatingSlider.h"
#include "../Panels/GeneralControls.h"
#include "../Panels/MainPanel.h"
#include "../../App/Dialogs.h"
#include "../../App/Directories.h"
#include "../../App/FileManager.h"
#include "../../Util/CycleEnums.h"


PresetPage::PresetPage(SingletonRepo* repo) : 
		ComponentMovementWatcher(this)
	,	SingletonAccessor(repo, "PresetPage")

	,	wave				(0, 5, this, repo, "Show samples")
	,	verts				(2, 5, this, repo, "Show presets")
	,	netIcon				(7, 8, this, repo, "Show community presets you don't have")
	,	prevIcon			(2, 8, this, repo, "Previous Preset")
	,	nextIcon			(3, 8, this, repo, "Next Preset")

	,	tagsField			({}, {})

	,	uploadThread		(repo)
	,	downloadDeetsThread	(repo)
	,	downloadPresetThread(repo)

	,	pendingPresetIndex	(-1)
	,	currPresetFiltIndex	(-1)
	,	spinAnimating		(false)
	,	pendingSettings		(false)
	,	spinAngle			(0.f)
	,	wavLabel			({}, "Dir: ")
	,	subfolderLabel		({}, "Subfolder: ")
{
	tableListBox = std::make_unique<TableListBox>("Presets", this);

	setListBox(tableListBox.get());
	tableListBox->setRowHeight(21);

	MiscGraphics& mg = getObj(MiscGraphics);
	progressImage 	 = mg.getIcon(8, 6);

	TableHeaderComponent& header = tableListBox->getHeader();

	int invisibleFlag = TableHeaderComponent::defaultFlags & ~TableHeaderComponent::visible;

	header.addColumn("Preset Name", DocumentDetails::Name, 	 	230, 100, 300);
	header.addColumn("Author", 		DocumentDetails::Author, 	210, 100, 300);
	header.addColumn("Pack", 		DocumentDetails::Pack, 	 	100, 80,  250);
	header.addColumn("Revision", 	DocumentDetails::Revision, 	90,  45,  100,  invisibleFlag);
	header.addColumn("Date", 		DocumentDetails::Date, 	 	140, 80,  135);
	header.addColumn("Modified", 	DocumentDetails::Modified, 	140, 80,  135, 	invisibleFlag);
	header.addColumn("Tag", 		DocumentDetails::Tag, 		100, 80,  200);
	header.addColumn("Ver.", 		DocumentDetails::Version, 	40,  35,  50, 	invisibleFlag);
	header.addColumn("Size", 		DocumentDetails::Size, 	 	80,  60,  100, 	invisibleFlag);
	header.addColumn("Rating", 		DocumentDetails::Rating, 	125, 100, 200);
	header.addColumn("Shr", 		DocumentDetails::UploadCol, 30,  30,  30, 	TableHeaderComponent::notResizableOrSortable);
	header.addColumn("Rmv", 		DocumentDetails::DismissCol,30,  30,  30, 	TableHeaderComponent::notResizableOrSortable);

	header.setStretchToFitActive(true);

	search = std::make_unique<SearchField<DocumentDetails>>(this, repo, String());
	addAndMakeVisible(tableListBox.get());
	addAndMakeVisible(search.get());
	addAndMakeVisible(&tagsField);
	addChildComponent(&wavControls);

	folderImage 	= PNGImageFormat::loadFrom(Images::folder_png, Images::folder_pngSize);
	wavFolderButton = std::make_unique<IconButton>(folderImage, repo);
	upButton.reset(getLookAndFeel().createFileBrowserGoUpButton());

	dismissImage 	= Image(Image::ARGB, 24, 24, true);
	Graphics g(dismissImage);

	g.setOpacity(0.5f);
	g.drawImageTransformed(getObj(MiscGraphics).getIcon(0, 6),
	                       AffineTransform::rotation(IPP_PI4).translated(12, -6));

	wavControls.addAndMakeVisible(wavFolderButton.get());
	wavControls.addAndMakeVisible(upButton.get());
	wavControls.addAndMakeVisible(&subfolderBox);
	wavControls.addAndMakeVisible(&wavDirectory);
	wavControls.addAndMakeVisible(&wavLabel);
	wavControls.addAndMakeVisible(&subfolderLabel);

	upButton->addListener(this);
	wavFolderButton->addListener(this);
	subfolderBox.addListener(this);
	wavDirectory.setEditable(true, false, false);
	wavDirectory.addListener(this);
	wavDirectory.setColour(Label::outlineColourId, Colour::greyLevel(0.14));

	Label* labels[] = { &tagsField, /* &commentLabel */ };
	for(int i = 0; i < numElementsInArray(labels); ++i)
	{
		labels[i]->setColour(Label::textColourId, Colour::greyLevel(0.65f));
		labels[i]->setJustificationType(Justification::centredRight);
		labels[i]->setColour(Label::backgroundColourId, Colour::greyLevel(0.1f));
		labels[i]->setEditable(true, false, true);
		labels[i]->addListener(this);
	}

	Component* selectTemp[] = { &wave, &verts, &netIcon };

	CalloutUtils::addRetractableCallout(selectCO, selectPO, repo, 0, 5, selectTemp,
										numElementsInArray(selectTemp), this, true);

	Component* navTemp[] = { &prevIcon, &nextIcon };

	CalloutUtils::addRetractableCallout(navCO, navPO, repo, 0, 5, navTemp,
										numElementsInArray(navTemp), this, true);

	verts.setHighlit(true);
}

void PresetPage::init() {
	presetDirUpdated();
	readPresetSettings();
}

void PresetPage::addToRevisionMap(const Array<DocumentDetails, CriticalSection>& array) {
	for (int i = 0; i < (int) array.size(); ++i) {
		DocumentDetails deets = array.getReference(i);

		int code = deets.getKey().hashCode();
		dldedRevisionMap.set(code, deets.getRevision());
	}
}

void PresetPage::addToRevisionMap(const DocumentDetails& deets) {
	int code = deets.getKey().hashCode();
	dldedRevisionMap.set(code, deets.getRevision());

	tableListBox->updateContent();
}

/*
 * Populate downloaded deets array to avoid showing
 */
void PresetPage::presetDirUpdated() {
	localDetails.clear();

	addFilesToArray(localDetails, getObj(Directories).getPresetDir(), getStrConstant(DocumentExt));
	addFilesToArray(localDetails, getObj(Directories).getUserPresetDir(), getStrConstant(DocumentExt));
	addToRevisionMap(localDetails);
}

PresetPage::~PresetPage() {
	killThreads();

	deleteAndZero(selectPO);
	deleteAndZero(selectCO);

	deleteAndZero(navPO);
	deleteAndZero(navCO);
}

void PresetPage::refresh() {
	allItems.clear();

	if (verts.isHighlit()) {
		addFilesToDefaultArray(getObj(Directories).getPresetDir(), getStrConstant(DocumentExt));
		addFilesToDefaultArray(getObj(Directories).getUserPresetDir(), getStrConstant(DocumentExt));
	}

	if (wave.isHighlit()) {
		String exts[] = { "wav", "ogg", "aif", "aiff", "flac" };

		for (int i = 0; i < numElementsInArray(exts); ++i)
			addFilesToDefaultArray(wavDirectory.getText(false), exts[i]);
	}

	if (netIcon.isHighlit()) {
	  #ifndef DEMO_VERSION
		resetList();
		addItems(remoteDetails);
	  #else
		showMsg("Sharing presets is not a feature of this demo");
	  #endif
	}

	search->refresh();
}

void PresetPage::addFilesToDefaultArray(const String& path, const String& pattern) {
	addFilesToArray(allItems, path, pattern);
}

void PresetPage::addFilesToArray(Array<DocumentDetails, CriticalSection>& list,
                                 const String& path, const String& extension) {
	bool isWave = false;

	if (String("wav.flac.mp3.aiff.ogg").containsIgnoreCase(extension))
		isWave = true;

	File dir(path);

	Array<File> allResults;
	dir.findChildFiles(allResults, File::findFiles, false, "*." + extension);

	for(auto& file : allResults) {
		DocumentDetails details;

		details.setName(file.getFileNameWithoutExtension());
		details.setRevision(-1);

		int64 millis = file.getCreationTime().toMilliseconds();
		details.setDateMillis(millis);

		if (extension == getStrConstant(DocumentExt)) {
			std::unique_ptr<InputStream> stream(file.createInputStream());

			if (Document::readHeader(stream.get(), details, getConstant(DocMagicCode))) {
				int code = details.getKey().hashCode();

				if (dismissedSet.find(code) != dismissedSet.end()) {
					continue;
				}

				if (ratingsMap.contains(code)) {
					details.setRating(ratingsMap[code]);
				}
			} else {
				continue;
			}
		} else if (isWave) {
			details.setIsWave(true);
		}

		details.setFilename(file.getFullPathName());
		details.setSizeBytes((int) file.getSize());

		list.add(details);
	}

	resetList();

	if (getNumRows() > 0) {
		currPresetFiltIndex = 0;
	}
}

int PresetPage::getNumRows() {
	return getFilteredItems().size();
}

void PresetPage::setWavDirectory(const String& dir) {
	wavDirectory.setText(dir, dontSendNotification);
}

void PresetPage::paintRowBackground(Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) {
	const DocumentDetails& details = getFilteredItems().getReference(rowNumber);

	ColourGradient gradient;

	gradient.isRadial 	= false;
	gradient.point1 	= Point<float>(0, 0);
	gradient.point2 	= Point<float>(0, height);

	bool isNew 	= false;
	int code 	= details.getKey().hashCode();

	if(newlyDownloaded.contains(code))
		isNew = true;

	bool isOdd 			= rowNumber & 1;
	float selectedLevel = 0.4f;
	float oddLevel 		= 0.18f;
	float evenLevel 	= 0.16f;
	float middleDiff 	= rowIsSelected ? -0.02f : isOdd ? 0.01f : -0.01f;
	float brightness 	= rowIsSelected ? selectedLevel : isOdd ? oddLevel : evenLevel;
	float saturation 	= isNew ? 0.6f : details.isRemote() ? 0.3f : details.isWave() ? 0.2f : 0.f;
	float hue 			= isNew ? 0.0 : 0.6f;

	if(Time::currentTimeMillis() - details.getDateMillis() < 2 * 86400 * 1000)
		saturation += 0.2f;

	gradient.addColour(0.0f, Colour::fromHSV(hue, saturation, brightness, 1.f));
	gradient.addColour(0.5f, Colour::fromHSV(hue, saturation, brightness + middleDiff, 1.f));
	gradient.addColour(1.0f, Colour::fromHSV(hue, saturation, brightness, 1.f));

	g.setGradientFill(gradient);
	g.fillAll();
}

void PresetPage::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) {
	const DocumentDetails& details = getFilteredItems().getReference(rowNumber);

	g.setColour(Colour::greyLevel(0.64f));

	const StringArray& tags = details.getTags();

	String text = columnId == DocumentDetails::Name ? details.getName() :
				  columnId == DocumentDetails::Pack ? details.getPack() :
				  columnId == DocumentDetails::Tag ? (tags.size() == 0 ? String() : tags[0]) : details.getAuthor();

	switch(columnId) {
		case DocumentDetails::Name:
		case DocumentDetails::Pack:
		case DocumentDetails::Author:
		case DocumentDetails::Tag:
		{
			if(columnId == DocumentDetails::Author && details.isWave())
				break;

			g.setFont(14);

			getObj(MiscGraphics).drawShadowedText(g, text, 5, 15, FontOptions(14), 0.65f);
			break;
		}

		case DocumentDetails::Size: {
			float bytes = details.getSizeBytes() / 1024.f;
			String multStr;
			int precision = 2;

			if (bytes > 1024.f) {
				bytes /= 1024.f;
				multStr = "MB";
			} else {
				multStr = "KB";

				if (bytes > 10.f)
					precision = 1;
			}

			g.setFont(12);
			g.drawText(String(bytes, precision), 5, 0, width - 35, height, Justification::centredRight, true);
			g.setColour(Colour::greyLevel(0.6f));
			g.drawText(multStr, 5, 0, width - 15, height, Justification::centredRight, true);

			break;
		}

		case DocumentDetails::Rating: {
			break;
		}

		case DocumentDetails::Revision: {
			int revision = details.getRevision();

			g.setFont(12);
			if(revision >= 0) {
				g.drawText(String(details.getRevision()), 10, 0, width, height, Justification::centredLeft, true);
			}

			break;
		}

		case DocumentDetails::Date:
		case DocumentDetails::Modified: {
			g.setFont(10);

			Time time(columnId == DocumentDetails::Date ? details.getDateMillis() : details.getModifMillis());
			getObj(MiscGraphics).drawShadowedText(g, time.formatted(L"%d \u00b7 %m \u00b7 %y"), 5, 13, Font(10), 0.65f);

			break;
		}

		case DocumentDetails::Version: {
			g.setFont(12);
			g.drawText(String(details.getProductVersion(), 1), 5, 0, width, height, Justification::centredLeft, true);
		}

		case DocumentDetails::UploadCol: {
			break;
		}
	}

	g.setColour(Colour::greyLevel(0.f).withAlpha(0.25f));
	g.drawVerticalLine(width - 1, 0.f, height);
}

void PresetPage::cellClicked(int rowNumber, int columnId, const MouseEvent & e)
{
	currPresetFiltIndex = rowNumber;

	updateTags(rowNumber);
}

void PresetPage::cellDoubleClicked(int rowNumber, int columnId, const MouseEvent & e)
{
	if(columnId == 6)
		return;

	if(netIcon.isHighlit())
		return;

	loadItem(rowNumber);
	cleanup();
}

void PresetPage::loadItem(int itemIndex, bool ignoreSave)
{
	std::cout << "loading preset item" << itemIndex << ", ignore save: " << ignoreSave << "\n";

	jassert(isPositiveAndBelow(itemIndex, getNumRows()));

	if(itemIndex >= getNumRows())
		return;

	const DocumentDetails& details = getFilteredItems().getReference(itemIndex);

	currPresetFiltIndex = itemIndex;

	loadItem(details);
}

void PresetPage::loadItem(const DocumentDetails& details, bool ignoreSave) {
	const String& filename = details.getFilename();
	const File file(filename);

	if (filename.endsWith(getStrConstant(DocumentExt))) {
		jassert(file.existsAsFile());

		if (ignoreSave || !getObj(EditWatcher).getHaveEdited()) {
			getObj(FileManager).openPreset(file);

			showImportant(details.getName());
			getObj(IConsole).setKeys(details.getAuthor());

	#if PLUGIN_MODE
			getObj(PluginProcessor).updateHostDisplay();
	#endif
		} else {
			pendingDeetsToLoad = details;
			getObj(Dialogs).promptForSaveApplicably(Dialogs::LoadPresetItem);
		}
	}

	else if (String(".wav.ogg.flac.aif.aiff").containsIgnoreCase(file.getFileExtension())) {
		getObj(FileManager).openWave(file, Dialogs::DialogSource);
	}
}

void PresetPage::loadPendingItem() {
	loadItem(pendingDeetsToLoad, true);
}

void PresetPage::resetListAndLoadItem(int itemIndex) {
	clearSearch();
	loadItem(itemIndex);
}

void PresetPage::backgroundClicked(const MouseEvent& e) {
}

void PresetPage::sortOrderChanged(int newSortColumnId, bool isForwards) {
	DocumentDetails::sortColumn = newSortColumnId;

	setSortDirection(isForwards);
	search->refresh();

	currPresetFiltIndex = 0;
	updateTags(currPresetFiltIndex);
	tableListBox->updateContent();
}

int PresetPage::getColumnAutoSizeWidth(int columnId) {
	return 150;
}

void PresetPage::returnKeyPressed(int lastRowSelected) {
	if (currPresetFiltIndex >= 0 && currPresetFiltIndex < getNumRows()) {
		loadItem(currPresetFiltIndex);
		cleanup();
	}
}

void PresetPage::paint(Graphics& g) {
	g.fillAll(Colour::greyLevel(0.14f));

	if (spinAnimating) {
		Point<int> r = selectCO->getPosition();
		r.addXY(selectCO->getExpandedSize() + 15, 12);

		AffineTransform transform = AffineTransform::translation(-12, -12).rotated(spinAngle).translated(r.getX(), r.getY());
		g.setColour(Colours::white.withAlpha(0.8f));
		g.drawImageTransformed(progressImage, transform, true);
	}

	g.setColour(Colour::greyLevel(0.65f));

	getObj(MiscGraphics).drawJustifiedText(g, "VIEW", 	wave, 			netIcon, 		true, selectCO);
	getObj(MiscGraphics).drawJustifiedText(g, "NAV", 	prevIcon, 		nextIcon, 		true, navCO);
	getObj(MiscGraphics).drawJustifiedText(g, "TAGS", 	Rectangle<int>(0, 0, tagsField.getWidth(), 0), true, &tagsField);
}

void PresetPage::resized() {
	Rectangle<int> bounds 	 = getLocalBounds().reduced(4, 8);
	Rectangle<int> topBounds = bounds.removeFromTop(40).reduced(0, 5);

	topBounds.removeFromLeft(15);
	navCO->setBounds(topBounds.removeFromLeft(navCO->getExpandedSize()).translated(0, 3));

	topBounds.removeFromLeft(15);
	selectCO->setBounds(topBounds.removeFromLeft(selectCO->getExpandedSize()).translated(0, 3));

	float tagPortion 	= 0.3f, searchPortion = 0.35f;
	float spacePortion 	= 0.5f - searchPortion * 0.5f - tagPortion;

	tagsField.setBounds(topBounds.removeFromRight(getWidth() * tagPortion));

	topBounds.removeFromRight(getWidth() * spacePortion);
	search->setBounds(topBounds.removeFromRight(getWidth() * searchPortion));

	if (wave.isHighlit()) {
		Rectangle<int> r = bounds.removeFromTop(32);
		wavControls.setBounds(r);

		r = r.withPosition(0, 0).reduced(3, 4);
		wavFolderButton->setBounds(r.removeFromRight(24));
		r.removeFromRight(3);

		upButton->setBounds(r.removeFromRight(30));
		r.removeFromRight(10);

		subfolderBox.setBounds(r.removeFromRight(jmin(220, int(getWidth() * 0.2))));
		r.removeFromLeft(1);
		subfolderLabel.setBounds(r.removeFromRight(85));

		wavLabel.setBounds(r.removeFromLeft(40));
		r.removeFromLeft(5);
		wavDirectory.setBounds(r);
	}

	tableListBox->setBounds(bounds);

	int scrollbarWidth = tableListBox->getViewport()->isVerticalScrollBarShown() ? 20 : 0;
	tableListBox->getHeader().resizeAllColumnsToFit(bounds.getWidth() - scrollbarWidth);
}

bool PresetPage::containsString(DocumentDetails& details, const String& str) {
	if(details.getName().containsIgnoreCase(str)) {
		return true;
	}

	if(details.getAuthor().containsIgnoreCase(str)) {
		return true;
	}

	if(details.getPack().containsIgnoreCase(str)) {
		return true;
	}

	const StringArray& tags = details.getTags();
	for(int i = 0; i < tags.size(); ++i) {
		if (tags[i].containsIgnoreCase(str)) {
			return true;
		}
	}

	return false;
}

void PresetPage::updateTags(int index)
{
	if(index < 0) {
		return;
	}

	const Array<DocumentDetails, CriticalSection>& filtered = getFilteredItems();

	if (!filtered.size() == 0 && index < filtered.size()) {
		const DocumentDetails& details = filtered[index];
		const StringArray& tags = details.getTags();

		tagsField.setText(tags.joinIntoString(", "), dontSendNotification);
	}
}

void PresetPage::doChangeUpdate()
{
	repaint();

	int size = getFilteredItems().size();

	if (size > 0) {
		currPresetFiltIndex = jmin(currPresetFiltIndex, size - 1);
		updateTags(currPresetFiltIndex);
	} else {
		currPresetFiltIndex = -1;
	}
}

void PresetPage::selectedRowsChanged(int lastRowSelected)
{
	updateTags(lastRowSelected);

	currPresetFiltIndex = lastRowSelected;
}

Component* PresetPage::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                               Component* existingComponentToUpdate) {
	if (rowNumber >= getFilteredItems().size()) {
		jassertfalse;
		return 0;
	}

	const DocumentDetails& details = getFilteredItems().getReference(rowNumber);

	if (details.isWave()) {
		delete existingComponentToUpdate;
		existingComponentToUpdate = 0;
	}

	if (existingComponentToUpdate != 0) {
		if (columnId == DocumentDetails::Rating) {
			RatingSlider* slider = dynamic_cast<RatingSlider*>(existingComponentToUpdate);

			if (slider) {
				int code = details.getKey().hashCode();

				slider->setRating(ratingsMap[code]);
				slider->setRow(rowNumber);
			} else {
				jassertfalse;
			}

			return existingComponentToUpdate;
		}
		if (columnId == DocumentDetails::UploadCol) {
			delete existingComponentToUpdate;

			return createIconButton(details, rowNumber, columnId);
		}

		if (columnId == DocumentDetails::DismissCol) {
			delete existingComponentToUpdate;

			return createIconButton(details, rowNumber, columnId);
		}
	} else {
		bool isPreset = details.getFilename().endsWith(getStrConstant(DocumentExt));

		if (isPreset) {
			int code = details.getKey().hashCode();

			if (columnId == DocumentDetails::Rating) {
				return new RatingSlider(ratingsMap[code], rowNumber, this);
			}

			if (columnId == DocumentDetails::UploadCol) {
				return createIconButton(details, rowNumber, columnId);
			}
			if (columnId == DocumentDetails::DismissCol) {
				return createIconButton(details, rowNumber, columnId);
			}
		}
	}

	return 0;
}

IconButton* PresetPage::createIconButton(const DocumentDetails& details, int rowNumber, int column) {
	int code = details.getKey().hashCode();
	bool isDownloaded = downloadedSet.find(code) != downloadedSet.end();

	String alias = getObj(Settings).getProperty("AuthorAlias", {});

	switch (column) {
		case DocumentDetails::UploadCol: {
			String hoverMsg;
			bool isAtLatestRevision = false;

			if (details.getPack().equalsIgnoreCase("Factory") || isDownloaded) {
				return nullptr;
			}

			if (details.isRemote()) {
				if (dldedRevisionMap.contains(code)) {
					if (dldedRevisionMap[code] < details.getRevision())
						hoverMsg = "Update " + details.getName() + " to revision " + String(details.getRevision());
					else {
						hoverMsg = details.getName() + " already at latest revision";
						isAtLatestRevision = true;
					}
				} else {
					hoverMsg = "Download " + details.getName();
				}
			} else {
				hoverMsg = "Upload " + details.getName();
			}

			int iconY = isAtLatestRevision ? 7 : 8;
			int iconX = isAtLatestRevision ? 5 : details.isRemote() || details.getAuthor().equalsIgnoreCase(alias) ?  1 : 0;
			IconButton* button = new IconButton(iconX, iconY, this, repo, hoverMsg);

			button->getProperties().set("isAtLatest", isAtLatestRevision);
			button->getProperties().set("rowNumber", rowNumber);
			button->getProperties().set("column", DocumentDetails::UploadCol);

			return button;
		}

		case DocumentDetails::DismissCol: {
			if (!details.isRemote() && !isDownloaded) {
				return nullptr;
			}

			std::unique_ptr<IconButton> button = std::make_unique<IconButton>(dismissImage, repo);
			button->setMessages("Dismiss " + details.getName(), {});
			button->addListener(this);
			button->getProperties().set("rowNumber", rowNumber);
			button->getProperties().set("column", DocumentDetails::DismissCol);

			return button.release();
		}
		default: break;
	}

	return nullptr;
}

void PresetPage::ratingChanged(float rating, int row) {
	Array<DocumentDetails, CriticalSection>& detailsList = getFilteredItemsMutable();
	const String& currentlyLoadedPreset = getObj(FileManager).getCurrentPresetName();

	if (row < detailsList.size()) {
		DocumentDetails& details = detailsList.getReference(row);

		int globalDeetsIndex = allItems.indexOf(details);
		if (globalDeetsIndex == -1) {
			jassertfalse;
			return;
		}

		DocumentDetails& deets = allItems.getReference(globalDeetsIndex);

		int code = deets.getKey().hashCode();
		ratingsMap.set(code, rating);

		pendingSettings = true;

		/*
		details.setRating(rating);
		deets.setRating(rating);

		if(deets.getFilename().equalsIgnoreCase(currentlyLoadedPreset))
		{
			getObj(Document).getDetails().setRating(deets.getRating());
			getObj(EditWatcher).setHaveEditedWithoutUndo(true);
		}
		else
		{
			int previousRevision = deets.getRevision();
			deets.setRevision(previousRevision - 1);

			// increments revision automatically
			getObj(Document).saveHeaderValidated(deets);
			deets.setRevision(previousRevision);
		}
		*/
	}
}

void PresetPage::updateNetItems(const Array<DocumentDetails, CriticalSection>& deets) {
	netIcon.setPendingItems(remoteDetails.size());
	getObj(GeneralControls).setNumCommunityPresets(remoteDetails.size());
}

void PresetPage::addItems(const Array<DocumentDetails, CriticalSection>& deets) {
	allItems.addArray(deets);

	resetList();

	currPresetFiltIndex = getFilteredItems().size() > 0 ? 0 : -1;
	listBox->selectRow(0, false, true);
}

void PresetPage::addItem(const DocumentDetails& details) {
	allItems.add(details);

	resetList();

	currPresetFiltIndex = getFilteredItems().indexOf(details);

	if (currPresetFiltIndex < 0) {
		jassertfalse;
		currPresetFiltIndex = 0;
	}
}

int PresetPage::getIndexOfSelected() {
	return currPresetFiltIndex;
}

String PresetPage::getProgramName(int index) const {
	const Array<DocumentDetails, CriticalSection>& detailsList = getFilteredItems();

	if (isPositiveAndBelow(index, detailsList.size())) {
		return detailsList.getReference(index).getName();
	}

	return {};
}

void PresetPage::labelTextChanged(Label* label) {
	if (label == &tagsField) {
		Array<DocumentDetails, CriticalSection>& detailsList = getFilteredItemsMutable();
		const String& currentlyLoadedPreset = getObj(FileManager).getCurrentPresetName();

		if (!isPositiveAndBelow(currPresetFiltIndex, detailsList.size()))
			return;

		DocumentDetails& details = detailsList.getReference(currPresetFiltIndex);

		int globalDeetsIndex = allItems.indexOf(details);

		if (globalDeetsIndex == -1) {
			jassertfalse;
			return;
		}

		DocumentDetails& deets = allItems.getReference(globalDeetsIndex);

		String tagsString = label->getText(true);
		tagsString = tagsString.removeCharacters("\t. ?");

		StringArray strings;
		strings.addTokens(tagsString, ",", {});

		deets.setTags(strings); // update the underlying data
		details.setTags(strings); // update the viewed details

		if (deets.getFilename().equalsIgnoreCase(currentlyLoadedPreset)) {
			getObj(Document).getDetails() = deets;
			getObj(EditWatcher).setHaveEditedWithoutUndo(true);
		} else {
			int previousRevision = deets.getRevision();
			deets.setRevision(previousRevision - 1);

			// increments revision automatically
			getObj(Document).saveHeaderValidated(deets);
			deets.setRevision(previousRevision);
		}
	} else if (label == &wavDirectory) {
		subfolders.clear();

		File dir(label->getText(false));
		dir.findChildFiles(subfolders, File::findDirectories, false);

		subfolderBox.clear(dontSendNotification);

		for (int i = 0; i < subfolders.size(); ++i)
			subfolderBox.addItem(subfolders[i].getFileName(), i + 1);

		refresh();
	}
}

void PresetPage::updateCurrentPreset(const String& name) {
	const Array<DocumentDetails, CriticalSection>& detailsList = getFilteredItems();

	for (int i = 0; i < detailsList.size(); ++i) {
		const DocumentDetails& deets = detailsList.getUnchecked(i);

		if (!deets.isWave() && deets.getName().equalsIgnoreCase(name)) {
			currPresetFiltIndex = i;
			listBox->selectRow(currPresetFiltIndex);
			break;
		}
	}
}

void PresetPage::setUploadResponse(const String& response) {
	showMsg(response);
}

void PresetPage::stopSpinAnimation() {
	spinAnimating = false;
	stopTimer(SpinAnimId);
	repaint();
}

void PresetPage::startSpinAnimation() {
	spinAnimating = true;
	spinAngle = 0.f;
	startTimer(SpinAnimId, 40);
}

void PresetPage::timerCallback(int id) {
	switch (id) {
		case SpinAnimId: {
			Rectangle<int> r = selectCO->getBounds().translated(50, 0);

			spinAngle += 0.3f;
			repaint(r);

			break;
		}

		case DownloadPresetsId: {
			downloadDeetsThread.startThread(Thread::Priority::low);
			stopTimer(id);
			break;
		}

		case DelayLoadId: {
			resetListAndLoadItem(pendingPresetIndex);
			stopTimer(id);
			break;
		}

		default:
			jassertfalse;
	}
}

void PresetPage::killThreads()
{
	if(downloadDeetsThread.isThreadRunning()) {
		downloadDeetsThread.stopThread(10);
	}

	if(uploadThread.isThreadRunning()) {
		uploadThread.stopThread(10);
	}

	if(downloadPresetThread.isThreadRunning()) {
		downloadPresetThread.stopThread(10);
	}
}

String PresetPage::downloadPreset(DocumentDetails& deets)
{
	URL url(getObj(Directories).getHomeUrl() + "/php/downloadPreset.php");
	String serial = getObj(Settings).getProperty("Serial");

	url = url.withParameter("serial", 		serial);
	url = url.withParameter("product_name", String(ProjectInfo::projectName));
	url = url.withParameter("product_code", getStrConstant(ProductCode));
	url = url.withParameter("preset_name", 	deets.getName());
	url = url.withParameter("author", 		deets.getAuthor());
	url = url.withParameter("pack", 		deets.getPack());
	url = url.withParameter("filename", 	deets.getFilename());

	MemoryBlock block;
	bool succeeded = url.readEntireBinaryStream(block, true);

	MemoryInputStream mis(block, false);
	if (!succeeded || mis.readInt() != getConstant(DocMagicCode)) {
		return "Couldn't download preset " + deets.getName();
	}

	String filenameWithoutExt = getObj(Directories).getUserPresetDir() + deets.getName();
	File saveFile(filenameWithoutExt + "." + getStrConstant(DocumentExt));

	int code = deets.getKey().hashCode();
	if (haveKeyAndEqualRevision(code, deets.getRevision())) {
		return "Latest revision already downloaded";
	}

	int redundancyCount = 2;
	while (saveFile.existsAsFile()) {
		saveFile = filenameWithoutExt + "_" + String(redundancyCount) + "." + getStrConstant(DocumentExt);
		++redundancyCount;
	}

	FileOutputStream fos(saveFile);
	fos.write(block.getData(), block.getSize());

	deets.setFilename(saveFile.getFullPathName());

	newlyDownloaded.add(code);
	downloadedSet.insert(code);
	allItems.add(deets);

	pendingSettings = true;

	{
		MessageManagerLock lock;

//		lastDownloaded = saveFile.getFullPathName();
		addToRevisionMap(deets);
	}

	return "Downloaded " + deets.getName() + " successfully";
}

void PresetPage::triggerLoadItem() {
	if (currPresetFiltIndex >= 0) {
		loadItem(currPresetFiltIndex);
		cleanup();
	}
}

void PresetPage::clearSearch() {
	search->setText({});
	resetList();
}

void PresetPage::updatePresetIndex() {
	DocumentDetails& deets = getObj(Document).getDetails();

	currPresetFiltIndex = getFilteredItems().indexOf(deets);
}

void PresetPage::componentVisibilityChanged() {
	listBox->scrollToEnsureRowIsOnscreen(currPresetFiltIndex);
}

bool PresetPage::haveKeyAndEqualRevision(int code, int revision) {
	return dldedRevisionMap.contains(code) ? dldedRevisionMap[code] >= revision : false;
}

bool PresetPage::removeDetailsFrom(Array<DocumentDetails, CriticalSection>& list, int code) {
	bool found = false;
	for (int i = 0; i < list.size(); ++i) {
		DocumentDetails& deets = list.getReference(i);
		int deetsCode = deets.getKey().hashCode();

		if (deetsCode == code) {
			list.remove(i);
			found = true;
			break;
		}
	}

	return found;
}

void PresetPage::triggerButtonClick(ButtonEnum button) {
	switch (button) {
		case PrevButton: buttonClicked(&prevIcon);
			break;
		case NextButton: buttonClicked(&nextIcon);
			break;
	}
}

void PresetPage::buttonClicked(Button* button) {
	if(button == &wave || button == &verts || button == &netIcon) {
		bool didAnything = false;
		didAnything |= wave.setHighlit(button == &wave);
		didAnything |= verts.setHighlit(button == &verts);
		didAnything |= netIcon.setHighlit(button == &netIcon);

		TableHeaderComponent& header = tableListBox->getHeader();

		bool isNowWave  = wave.isHighlit();
		bool isNet		= netIcon.isHighlit();

		header.setColumnVisible(DocumentDetails::Size, 	 	   isNowWave);
		header.setColumnVisible(DocumentDetails::Author, 	 ! isNowWave);
		header.setColumnVisible(DocumentDetails::Pack, 	 	 ! isNowWave);
		header.setColumnVisible(DocumentDetails::Revision, 	 ! isNowWave);
		header.setColumnVisible(DocumentDetails::Rating, 	 ! isNowWave && ! isNet);
		header.setColumnVisible(DocumentDetails::UploadCol,  ! isNowWave);
		header.setColumnVisible(DocumentDetails::Tag, 		 ! isNowWave);
		header.setColumnVisible(DocumentDetails::DismissCol, ! isNowWave);

		if (isNowWave) {
			if (wavDirectory.getText(false).isEmpty())
				wavDirectory.setText(getObj(Directories).getPresetDir(), dontSendNotification);

			wavControls.setVisible(true);

			resized();
		} else {
			wavControls.setVisible(false);
			resized();
		}

		if (didAnything) {
			refresh();
		}
	}

	else if(button == &nextIcon || button == &prevIcon)
	{
		int oldIndex = currPresetFiltIndex;
		int numRows = getNumRows();

		if (numRows > 0) {
			if (button == &nextIcon) {
				++currPresetFiltIndex;
			} else if (button == &prevIcon) {
				--currPresetFiltIndex;
			}

			currPresetFiltIndex = (numRows + currPresetFiltIndex) % numRows;

			if (oldIndex != currPresetFiltIndex) {
				std::cout << "Attempting to load preset index : " << currPresetFiltIndex << "\n";

				tableListBox->selectRow(currPresetFiltIndex);
				loadItem(currPresetFiltIndex);
			}
		}
	} else if (button == upButton.get()) {
		wavDirectory.setText(File(wavDirectory.getText()).getParentDirectory().getFullPathName(),
		                     sendNotificationAsync);
	} else if (button == wavFolderButton.get()) {
		getObj(Dialogs).showOpenWaveDialog(nullptr, {}, DialogActions::TrackPitchAction);
	} else {
	  #ifndef DEMO_VERSION
		int rowNumber = button->getProperties()["rowNumber"];
		bool isLatest = button->getProperties()["isAtLatest"];
		int column 	  = button->getProperties()["column"];

		jassert(isPositiveAndBelow(rowNumber, getNumRows()));
		const DocumentDetails& deets = getFilteredItemsMutable()[rowNumber];

		if (column == DocumentDetails::DismissCol) {
			int code = deets.getKey().hashCode();
			dismissedSet.insert(code);

			if (netIcon.isHighlit()) {
				removeDetailsFrom(remoteDetails, code);
				updateNetItems(remoteDetails);
			}

			refresh();

			pendingSettings = true;
		} else if (column == DocumentDetails::UploadCol) {
			if (deets.isRemote()) {
				String alias = getObj(Settings).getProperty("AuthorAlias", {});

				if (alias.equalsIgnoreCase(deets.getAuthor())) {
					return;
				}

				if (!isLatest) {
					downloadPresetThread.setDetails(deets);
					downloadPresetThread.startThread(Thread::Priority::low);
				} else {
					showMsg("Preset already downloaded!");
				}
			}
			else
			{
				File uploadFile(deets.getFilename());

				uploadThread.setFile(uploadFile);
				uploadThread.startThread(Thread::Priority::low);
			}
		}

      #else
		showMsg("Sharing presets is not a feature of this demo");
	  #endif
	}
}

String PresetPage::populateRemoteNames() {
	URL url(getObj(Directories).getHomeUrl() + "/php/presetList.php");

	String serial = getObj(Settings).getProperty("Serial");

	url = url.withParameter("serial", 		serial);
	url = url.withParameter("product_name", ProjectInfo::projectName);

	String page = url.readEntireTextStream(true);
	StringArray presetNames;

	presetNames.addLines(page);
	presetNames.removeEmptyStrings(true);

	remoteDetails.clear();

	for(auto& presetName : presetNames) {

		StringArray tokens;
		DocumentDetails deets;

		tokens.addTokens(presetName, ";", {});

		deets.setFilename(	tokens[0]);
		deets.setName(		tokens[1]);
		deets.setPack(		tokens[2]);
		deets.setAuthor(	tokens[3]);
		deets.setRevision(	tokens[4].getIntValue());
		deets.setDateMillis(tokens[5].getLargeIntValue());

		int code = deets.getKey().hashCode();
		if (dldedRevisionMap.contains(code)) {
			//			if(deets.getRevision() <= dldedRevisionMap[code])
			//			{
			continue;
			//			}
		}

		if (dismissedSet.find(code) != dismissedSet.end()) {
			continue;
		}

		String tags = tokens[6];
		StringArray tagsArr;
		tagsArr.addTokens(tags, ",", {});

		deets.setTags(tagsArr);
		deets.setRating(0);
		deets.setProductVersion(tokens[8].getDoubleValue());
		deets.setSizeBytes(tokens[7].getIntValue());
		deets.setIsRemote(true);

		remoteDetails.add(deets);
	}

	{
		MessageManagerLock lock;

		updateNetItems(remoteDetails);
	}

	if (remoteDetails.size() == 0) {
		return "No new presets or revisions";
	}

	return {};
}

void PresetPage::UploadThread::run()
{
	String serial = getObj(Settings).getProperty("Serial");

	URL uploadUrl(getObj(Directories).getHomeUrl() + "/php/uploadPreset.php");

	uploadUrl = uploadUrl.withParameter(	"serial", 		serial);
	uploadUrl = uploadUrl.withParameter(	"product_code", ProjectInfo::projectName);
	uploadUrl = uploadUrl.withFileToUpload(	"presets", 		uploadFile, "application/octet-stream");

	String response = uploadUrl.readEntireTextStream(true);

	if (response.isEmpty())
		response = "Could not connect to server"; {
		MessageManagerLock lock;
		showMsg(response);
	}
}

void PresetPage::downloadCommunityDocumentDetails() {
	startTimer(DownloadPresetsId, 1000);
}

void PresetPage::DownloadDetailsThread::run() {
	PresetPage* page = &getObj(PresetPage);
	page->startSpinAnimation();

	String response = page->populateRemoteNames(); {
		MessageManagerLock lock;
		showMsg(response);
		page->stopSpinAnimation();
	}
}

void PresetPage::DownloadPresetThread::run() {
	PresetPage* page = &getObj(PresetPage);
	page->startSpinAnimation();

	String response = page->downloadPreset(deets); {
		MessageManagerLock lock;
		showMsg(response);

		page->stopSpinAnimation();
		page->triggerAsyncUpdate();
	}
}

void PresetPage::handleAsyncUpdate() {
	DocumentDetails& deets = downloadPresetThread.getDetails();

	loadItem(deets);
}

Array<DocumentDetails, CriticalSection>& PresetPage::getAllItems() {
	return allItems;
}

void PresetPage::grabSearchFocus() {
	search->grabKeyboardFocus();
	search->repaint();
}

bool PresetPage::keyPressed(const KeyPress& key) {
	int code = key.getKeyCode();
	juce_wchar ch = key.getTextCharacter();

	if (ch >= '1' && ch <= '9') {
		int num = ch - '1';
		if (num < getFilteredItems().size()) {
			listBox->selectRow(num, true, true);
			listBox->scrollToEnsureRowIsOnscreen(num);
		}
	}

	if (code == KeyPress::returnKey) {
		returnKeyPressed(currPresetFiltIndex);
	} else if (code == KeyPress::escapeKey) {
		cleanup();
	}

	return true;
}

void PresetPage::cleanup() {
	getParentComponent()->exitModalState(1);
	getObj(MainPanel).resetTabToWaveform();
}

void PresetPage::comboBoxChanged(ComboBox* box) {
	if (box == &subfolderBox) {
		int id = box->getSelectedId();
		int index = id - 1;

		if (isPositiveAndBelow(index, subfolders.size())) {
			File file = subfolders[index];
			wavDirectory.setText(file.getFullPathName(), sendNotificationAsync);
		}
	}
}

void PresetPage::triggerAsyncLoad(int index) {
	pendingPresetIndex = index;

	startTimer(DelayLoadId, 20);
}

#include <Util/Util.h>

File PresetPage::getPresetSettingsFile()
{
	String appDataDir = File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName();

	// TODO use Directories?
  #ifdef JUCE_WINDOWS
	String settingsPath = appDataDir + "/Amaranth Audio/" +ProjectInfo::projectName +"/presets.xml";
  #else
	String settingsPath = appDataDir + "/Preferences/com.amaranthaudio." + ProjectInfo::projectName + ".presets.xml";
  #endif

	return File(settingsPath);
}

void PresetPage::readPresetSettings() {
	File settingsFile = getPresetSettingsFile();
	XmlDocument presetDoc(settingsFile);
	std::unique_ptr<XmlElement> elem = presetDoc.getDocumentElement(false);

	if (!settingsFile.existsAsFile()) {
		for (int i = 0; i < allItems.size(); ++i) {
			DocumentDetails& deets = allItems.getReference(i);
			int code = deets.getKey().hashCode();

			if (deets.getRating() > 0.f) {
				ratingsMap.set(code, deets.getRating());
			}
		}

		pendingSettings = true;
	}

	if (elem != nullptr) {
		if (XmlElement* ratingsElem = elem->getChildByName("Ratings")) {
			forEachXmlChildElementWithTagName(*ratingsElem, ratingElem, "Rating") {
				int code = ratingElem->getIntAttribute("code", 0);
				float value = ratingElem->getDoubleAttribute("value", 0);

				if (code != 0)
					ratingsMap.set(code, value);
			}
		}

		if (XmlElement* dismissed = elem->getChildByName("Dismissed")) {
			forEachXmlChildElementWithTagName(*dismissed, dissedElem, "Dismissal") {
				int code = dissedElem->getIntAttribute("code", 0);

				if (code != 0)
					dismissedSet.insert(code);
			}
		}

		if (XmlElement* downloaded = elem->getChildByName("DownloadedPresets")) {
			forEachXmlChildElementWithTagName(*downloaded, dldElem, "Download") {
				int code = dldElem->getIntAttribute("code", 0);

				if (code != 0)
					downloadedSet.insert(code);
			}
		}
	}
}

void PresetPage::writePresetSettings() {
	if (Util::assignAndWereDifferent(pendingSettings, false)) {
		File settingsFile = getPresetSettingsFile();

		XmlElement* ratingsElem = new XmlElement("Ratings");

		HashMap<int, float>::Iterator iter(ratingsMap);
		while(iter.next()) {
			XmlElement* rating = new XmlElement("Rating");
			rating->setAttribute("code",  iter.getKey());
			rating->setAttribute("value", iter.getValue());

			ratingsElem->addChildElement(rating);
		}

		XmlElement* dismissed = new XmlElement("Dismissed");

		std::set<int>::iterator it = dismissedSet.begin();
		while(it != dismissedSet.end()) {
			XmlElement* dissed = new XmlElement("Dismissal");
			dissed->setAttribute("code", *it);

			dismissed->addChildElement(dissed);
			++it;
		}

		XmlElement* downloaded = new XmlElement("DownloadedPresets");
		it = downloadedSet.begin();

		while(it != downloadedSet.end()) {
			XmlElement* dldElem = new XmlElement("Download");
			dldElem->setAttribute("code", *it);

			downloaded->addChildElement(dldElem);
			++it;
		}

		XmlElement document("PresetSettings");
		document.addChildElement(ratingsElem);
		document.addChildElement(dismissed);
		document.addChildElement(downloaded);

		if (settingsFile.deleteFile()) {
			std::unique_ptr fileStream(settingsFile.createOutputStream());

			if (fileStream != nullptr) {
				String docString = document.toString();
				fileStream->writeString(docString);
				fileStream->flush();
			}
		}
	}
}
