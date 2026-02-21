#pragma once

#include <App/SingletonAccessor.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>
#include <UI/Layout/IDynamicSizeComponent.h>
#include <UI/MiscGraphics.h>
#include <Util/Util.h>

#include <utility>

#include "HoverSelector.h"
#include "../../Panels/Console.h"
#include "../../../App/Directories.h"
#include "../../../UI/Widgets/Controls/MeshSelectionClient.h"

using namespace juce;

template<class MeshType> class MeshSelectionClient;
template<class MeshType> class MeshSelector;

template<class MeshType>
class SaveItem :
        public juce::Component
    ,	public Button::Listener
    ,	public Label::Listener
    ,	public Timer
    , 	public SingletonAccessor {
public:
    SaveItem(SingletonRepo* repo, String  extension, MeshSelector<MeshType>* selector) :
            SingletonAccessor(repo, "SaveItem")
        ,	extension	(std::move(extension))
        ,	selector	(selector)
        ,	saveButton	("Save")
        ,	nameEditor	("Save editor")
        ,	folderEditor("Folder editor") {
        saveButton.addListener(this);

        addAndMakeVisible(&saveButton);
        addAndMakeVisible(&nameEditor);
        addAndMakeVisible(&folderEditor);

        Label* editors[] = { &nameEditor, &folderEditor };

        for (auto& editor : editors) {
            editor->setColour(Label::textColourId, 		Colour::greyLevel(0.95f));
            editor->setColour(Label::outlineColourId, 	Colour(180, 190, 240));
            editor->setWantsKeyboardFocus(true);
        }

        nameEditor.addListener(this);
        nameEditor.setEditable(true);
        nameEditor.setInterceptsMouseClicks(true, false);

        folderEditor.addListener(this);
        folderEditor.setEditable(true);
        folderEditor.setInterceptsMouseClicks(true, false);

        savePath = getObj(Directories).getUserMeshDir() + extension + File::getSeparatorChar();
    }

    void paint(Graphics& g) override {
        MiscGraphics& mg = getObj(MiscGraphics);
        Font font(*mg.getSilkscreen());
        g.setFont(font);

        getObj(MiscGraphics).drawShadowedText(g, "folder",	folderBounds.getX(), folderBounds.getY() + 2, font, 0.75f);
        getObj(MiscGraphics).drawShadowedText(g, "name", 	nameBounds.getX(), nameBounds.getY() + 2, font, 0.75f);
    }

    void setFolder(const String& str) {
        folderEditor.setText(str, dontSendNotification);
    }

    void resized() override {
        Rectangle r(getLocalBounds());
        r.reduce(2, 8);
        Rectangle<int> r2 = r.removeFromRight(50);

        r2.removeFromTop(8);
        saveButton.setBounds(r2);
        r.removeFromRight(4);

        r2 = r.removeFromRight(r.getWidth() / 2);
        nameBounds = r2.removeFromTop(8);
        r2.removeFromTop(2);
        nameEditor.setBounds(r2);

        r.removeFromRight(4);
        folderBounds = r.removeFromTop(8);
        r.removeFromTop(2);
        folderEditor.setBounds(r);
    }

    void buttonClicked(Button* button) override {
        jassert(button == &saveButton);

        if (! nameEditor.getText().containsNonWhitespaceChars()) {
            showImportant("Please enter a name");
            return;
        }

        String folderPath = savePath + getCurrentFolder();
        File folderFile(folderPath);

        if (! folderFile.exists()) {
            if (folderFile.createDirectory().failed()) {
                showCritical("Problem creating directory");
                return;
            }
        }

        const String& filename(folderPath + getCurrentFilename());
        if (! Util::saveXml(File(filename), selector->getCurrentClientMesh(), "MeshPreset")) {
            showCritical("Problem saving mesh");
        }

        Util::removeModalParent<CallOutBox>(this);
    }

    String getCurrentFilename() {
        String editorText = nameEditor.getText();

        if (! editorText.endsWith(extension)) {
            editorText += String(".") + extension;
}

        return editorText;
    }

    String getCurrentFolder() {
        const String& folderText = folderEditor.getText();

        String folder = folderText.isNotEmpty() ?
                (folderText.endsWithChar(File::getSeparatorChar()) ?
                    folderText :
                    folderText + File::getSeparatorChar()) : String();

        return folder;
    }

    void labelTextChanged(Label* editor) override {
        const String& currentFolder = getCurrentFolder();

        stopTimer();
        getObj(IConsole).write({}, IConsole::DefaultPriority);

        overwriteMessage = {};

        if (editor == &folderEditor) {
            if (!File(savePath + currentFolder).exists()) {
                startTimer(300);
                overwriteMessage = "Will create directory " + currentFolder;
            }
        } else if (editor == &nameEditor) {
            const String& currentFile = getCurrentFilename();

            if (File(savePath + currentFolder + currentFile).existsAsFile()) {
                startTimer(300);
                overwriteMessage = "Will overwrite " + currentFolder + currentFile;
            }
        }
    }

    void timerCallback() override {
        if (overwriteMessage.isNotEmpty()) {
            showImportant(overwriteMessage);
        }

        stopTimer();
    }

private:
    TextButton saveButton;
    Label nameEditor;
    Label folderEditor;
    Label folderLabel, nameLabel;
    Rectangle<int> folderBounds, nameBounds;
    Ref<MeshSelector<MeshType> > selector;

    String overwriteMessage;
    String savePath;
    String extension;
};

template<class MeshType>
class MeshSelector :
        public HoverSelector
    ,	public IDynamicSizeComponent
    ,	public MultiTimer {
public:
    enum
    {
        MeshSave = 100,
        MeshOpen,
        MeshCopy,
        MeshPaste,
        MeshDouble,
        Cancel
    };

    enum
    {
        PopulateTimer = 1,
        RevertTimer
    };

    MeshSelector(SingletonRepo* repo,
                 MeshSelectionClient<MeshType>* client,
                 String extension,
                 bool horz,
                 bool saveAtTop,
                 bool updateMeshVersions = false,
                 bool meshDoubleApplic = true) :
            HoverSelector	(repo, 4, 0, horz)
        ,	itemCount		(1)
        ,	ignoreMouseExit	(false)
        ,	meshDoubleApplic(meshDoubleApplic)
        ,	oldMesh			(nullptr)
        ,	client			(client)
        ,	extension		(std::move(extension))
        ,	saveAtTop		(saveAtTop)
        ,	updateMeshVersion(updateMeshVersions) {
        jassert(client != nullptr);

        startTimer(PopulateTimer, 1000);
    }

    void populateMenu() override {
        itemCount = 1;
        oldMesh 	= nullptr;

        String parentPath 		= getObj(Directories).getMeshDir() 	+ extension + File::getSeparatorChar();
        String parentUserPath 	= getObj(Directories).getUserMeshDir() + extension + File::getSeparatorChar();

        File parentFile 		= File(parentPath);
        File parentUserFile 	= File(parentUserPath);

        Array<File> categories;
        parentFile.findChildFiles(categories, File::findDirectories, false, "*");
        parentUserFile.findChildFiles(categories, File::findDirectories, false, "*");

        StringArray uniqueCategs;
        for (auto&& categorie : categories) {
            uniqueCategs.addIfNotAlreadyThere(categorie.getFileName());
        }

        uniqueCategs.sort(true);

        menu.clear();
        if (saveAtTop) {
            addNonSelectionItems(menu, saveAtTop);
        }

        destroyMeshes();
        callbacks.clear();

        addFilesInDirectory(parentFile, menu);
        addFilesInDirectory(parentUserFile, menu);

        for (const auto& category : uniqueCategs) {
            File categ(parentPath + category);
            File userCateg(parentUserPath + category);

            PopupMenu subMenu;
            bool containsAny = addFilesInDirectory(userCateg, subMenu);

            if (containsAny) {
                subMenu.addSeparator();
            }

            containsAny |= addFilesInDirectory(categ, subMenu);

            if (containsAny) {
                menu.addSubMenu(category, subMenu, true, Image(), false);
            }
        }

        if (! saveAtTop) {
            addNonSelectionItems(menu, saveAtTop);
        }
    }

    void addNonSelectionItems(PopupMenu& menu, bool atTop)
    {
        if (! atTop) {
            menu.addSeparator();
        }

        menu.addItem(MeshSave, "Save as...");

//		bool canCopyMesh = getObj(LayerManager).canPasteTo(client->getLayerType());

        menu.addItem(MeshCopy, 	"Copy",  true, false, Image());
        menu.addItem(MeshPaste, "Paste", true, false, Image());

        if (meshDoubleApplic) {
            menu.addItem(MeshDouble, "Double", true, false, Image());
        }

        if (atTop) {
            menu.addSeparator();
        }
    }

    bool addFilesInDirectory(const File& child, PopupMenu& subMenu) {
        if (!child.exists()) {
            return false;
        }

        Array<File> results;
        child.findChildFiles(results, File::findFiles, false, String("*.") + extension);

        if (results.size() == 0) {
            return false;
        }

        for (int i = 0; i < results.size(); ++i) {
            File& file 					= results.getReference(i);
            std::unique_ptr<InputStream> stream = file.createInputStream();

            jassert(file.existsAsFile() && stream != nullptr);

            GZIPDecompressorInputStream gzipStream(stream.get(), false);
            XmlDocument xmlDoc(gzipStream.readEntireStreamAsString());
            std::unique_ptr<XmlElement> meshElem = xmlDoc.getDocumentElement();

            if (meshElem == nullptr) {
                info("bad mesh\n");

                jassertfalse;
                continue;
            }

            auto* mesh = new MeshType(file.getFileNameWithoutExtension() + "Mesh");

            mesh->readXML(meshElem.get());

            if (updateMeshVersion) {
                mesh->updateToVersion(getRealConstant(ProductVersion));
            }

            for (auto& cube : mesh->getCubes()) {
                cube->resetProperties();
            }

            meshes.add(mesh);

            auto* callback = new SelectorCallback(itemCount, file.getFullPathName(), this);
            callbacks.add(callback);

            subMenu.addCustomItem(itemCount, *callback, 200, 30, true);

            ++itemCount;
        }

        return true;
    }

    ~MeshSelector() override {
        destroyMeshes();
    }

    void destroyMeshes() {
        for (int i = 0; i < (int) meshes.size(); ++i) {
            if (meshes[i]) {
                meshes[i]->destroy();
                delete meshes[i];

                meshes.set(i, nullptr);
            }
        }

        meshes.clear();
    }

    void itemWasSelected(int itemId) override {
        switch (itemId) {
            case MeshSave: {
                auto saveItem = std::make_unique<SaveItem<MeshType>>(repo, extension, this);
                saveItem->setFolder(client->getDefaultFolder());
                saveItem->setSize(180, 50);

                CallOutBox& box = CallOutBox::launchAsynchronously(
                    std::unique_ptr<juce::Component>(std::move(saveItem)),
                    juce::Component::getScreenBounds(),
                    nullptr
                );
                box.setArrowSize(8.f);

                break;
            }
            case MeshCopy: {
                getObj(MeshLibrary).copyToClipboard(client->getCurrentMesh(), client->getLayerType());
                break;
            }
            case MeshPaste: {
                client->enterClientLock();

                MeshType* mesh = client->getCurrentMesh();
                getObj(MeshLibrary).pasteFromClipboardTo(mesh, client->getLayerType());

                client->setCurrentMesh(mesh);
                client->exitClientLock();

                break;
            }

            case MeshDouble: {
                client->enterClientLock();
                client->doubleMesh();
                client->exitClientLock();
            }

            case Cancel: {
                revert();
                break;
            }
            default: {
                jassert(itemId > 0);

                if (itemId <= 0) {
                    return;
                }

                const String& filename = callbacks[itemId - 1]->getFilename();

                File file(filename);
                std::unique_ptr<InputStream> stream = file.createInputStream();
                GZIPDecompressorInputStream gzipStream(stream.release(), true);

                XmlDocument xmlDoc(gzipStream.readEntireStreamAsString());
                std::unique_ptr<XmlElement> meshElem = xmlDoc.getDocumentElement();

                auto* mesh = new MeshType(file.getFileNameWithoutExtension() + "Mesh");

                mesh->readXML(meshElem.get());

                if (updateMeshVersion) {
                    mesh->updateToVersion(getRealConstant(ProductVersion));
                }

                for (auto& cube : mesh->getCubes()) {
                    cube->resetProperties();
                }

                client->enterClientLock();
                client->setCurrentMesh(mesh);
                client->exitClientLock();

                if (oldMesh != nullptr) {
                    oldMesh->destroy();

                    delete oldMesh;
                    oldMesh = nullptr;
                }

                ignoreMouseExit = true;
            }
        }
    }

    bool itemIsSelection(int id) override {
        return ! (id == MeshSave || id == MeshOpen || id == MeshCopy || id == Cancel || id == MeshPaste);
    }

    void prepareForPopup() override {
        client->prepareForPopup();

        setOriginalMesh(client->getCurrentMesh());

        jassert(oldMesh != nullptr);
    }

    void revert() override {
        if (oldMesh != nullptr) {
            client->previewMeshEnded(oldMesh);
        }
    }

    void mouseOverItem(int itemIndex) override {
        ignoreMouseExit = false;
        stopTimer(RevertTimer);

        MeshType* mesh = meshes[itemIndex - 1];

        client->previewMesh(mesh);
    }

    void mouseLeftItem(int itemIndex) override {
        if (!ignoreMouseExit) {
            startTimer(RevertTimer, 50);
}
    }

    void timerCallback(int id) override {
        if (id == RevertTimer) {
            stopTimer(RevertTimer);
            revert();
        } else if (id == PopulateTimer) {
            stopTimer(PopulateTimer);
            populateMenu();
        }
    }

    void setBoundsDelegate(int x, int y, int w, int h) override { juce::Component::setBounds(x, y, w, h); 			}
    MeshType* getCurrentClientMesh()							{ return client->getCurrentMesh(); 	}
    void setOriginalMesh(MeshType* oldMesh)						{ this->oldMesh = oldMesh; 			}

    int getExpandedSize() const override						{ return 22; 						}
    int getCollapsedSize() const override						{ return 22; 						}
    int getYDelegate() override									{ return juce::Component::getY(); 					}
    int getXDelegate() override									{ return juce::Component::getX(); 					}
    bool isVisibleDlg() const override 							{ return juce::Component::isVisible(); 				}
    void setVisibleDlg(bool isVisible) override 				{ juce::Component::setVisible(isVisible); 			}
    Rectangle<int> getBoundsInParentDelegate() const override {
        return Rectangle<int>(juce::Component::getX(), juce::Component::getY(), 25, 25);
    }
private:
    int itemCount;
    bool ignoreMouseExit;
    bool saveAtTop;
    bool updateMeshVersion;
    bool meshDoubleApplic;

    String extension;
    OwnedArray<SelectorCallback> callbacks;

    Ref<MeshSelectionClient<MeshType>> client;
    Array<MeshType*> meshes;
    MeshType* oldMesh;

    JUCE_LEAK_DETECTOR(MeshSelector)
};