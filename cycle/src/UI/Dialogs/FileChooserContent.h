#ifndef _FILECHOOSERCONTENT_H_
#define _FILECHOOSERCONTENT_H_

#include "JuceHeader.h"
#include <Definitions.h>

class ContentComponent  : public Component
{
public:
    ContentComponent (const String& name, const String& instructions_, FileBrowserComponent& chooserComponent_);
    void paint (Graphics& g);
    void resized();

    FileBrowserComponent& chooserComponent;
    TextButton okButton, cancelButton, newFolderButton;
    Label authorLabel, presetDetails, tagsLabel, packLabel, detailsLabel;

    TextEditor tagsBox, authorBox, packBox, detailsBox;

private:
    String instructions;
    TextLayout text;
};


#endif
