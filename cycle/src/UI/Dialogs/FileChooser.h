#ifndef _FileChooserDialog_h
#define _FileChooserDialog_h

#include <App/Doc/DocumentDetails.h>
#include "FileChooserContent.h"
#include "JuceHeader.h"

class FileChooserDialog :
		public ResizableWindow
	,	public Button::Listener
	,	public FileBrowserListener
{
public:
	FileChooserDialog (const String& title,
					   const String& instructions,
					   FileBrowserComponent& browserComponent,
					   bool warnAboutOverwritingExistingFiles,
					   const Colour& backgroundColour,
					   const DocumentDetails& details);

    ~FileChooserDialog();

    bool show (int width = 0, int height = 0);
    bool showAt (int x, int y, int width, int height);
    void centreWithDefaultSize (Component* componentToCentreAround = 0);

    enum ColourIds
    {
        titleTextColourId      = 0x1000850};
    /**< The colour to use to draw the box's title. */
    void buttonClicked(Button *button);
    void closeButtonPressed();
    void selectionChanged();
    void browserRootChanged(const File& newRoot);
    void fileClicked(const File & file, const MouseEvent & e);
    void fileDoubleClicked(const File & file);
    const String& getAuthorBoxContent() const { return authorBoxContent; }
    const String& getTagsBoxContent() const { return tagsBoxContent; }
    const String& getPackBoxContent() const { return packBoxContent; }
//    const String& getCommentBoxContent() const { return commentBoxContent; }

//    void paint(Graphics& g);

private:
    ContentComponent* content;
    const bool warnAboutOverwritingExistingFiles;
    String tagsBoxContent;
    String authorBoxContent;
    String packBoxContent;
//    String commentBoxContent;

    void okButtonPressed();
    void createNewFolder();
    void createNewFolderConfirmed (const String& name);

    static void okToOverwriteFileCallback (int result, FileChooserDialog*);
    static void createNewFolderCallback (int result, FileChooserDialog*, Component::SafePointer<AlertWindow>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileChooserDialog);
};

#endif
