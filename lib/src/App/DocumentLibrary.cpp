#include "DocumentLibrary.h"
#include "AppConstants.h"
#include "SingletonRepo.h"
#include "Doc/Document.h"
#include "../Definitions.h"
#include "../Util/Util.h"

DocumentLibrary::DocumentLibrary(SingletonRepo* repo) :
        SingletonAccessor	(repo, "DocumentLibrary")
    ,	pendingPresetIndex	(NullPresetIndex)
    ,	currentPresetIndex	(NullPresetIndex)
    ,	shouldOpenDefault	(formatSplit(false, true)) {
}

void DocumentLibrary::handleAsyncUpdate() {
    load(pendingPresetIndex);
    pendingPresetIndex = NullPresetIndex;
}

void DocumentLibrary::init() {
    readDocuments(getStrConstant(DocumentsDir));
}

void DocumentLibrary::readDocuments(const String& dir) {
    Array<File> files;

    File directory(dir);
    directory.findChildFiles(files, File::findFiles, false, "*." + getStrConstant(DocumentExt));

    allDocs.clear();

    for (auto& file : files) {
        DocumentDetails details;

        int64 millis = file.getCreationTime().toMilliseconds();

        details.setDateMillis	(millis);
        details.setFilename		(file.getFullPathName());
        details.setName			(file.getFileNameWithoutExtension());
        details.setRevision		(-1);
        details.setSizeBytes	((int) file.getSize());

        std::unique_ptr<InputStream> stream(file.createInputStream());

        if (Document::readHeader(stream.get(), details, getConstant(DocMagicCode))) {
            int code = details.getKey().hashCode();

            if (dismissedSet.find(code) != dismissedSet.end()) {
                continue;
            }

            if(ratingsMap.contains(code)) {
                details.setRating(ratingsMap[code]);
            }

            allDocs.add(details);
        }
    }
}

void DocumentLibrary::load(int index) {
    if (isPositiveAndBelow(index, allDocs.size())) {
        currentPresetIndex = index;
        DocumentDetails doc = allDocs[index];
        File file(doc.getFilename());

        getObj(Document).open(file.getFullPathName());
    }
}

void DocumentLibrary::triggerAsyncLoad(int index) {
    pendingPresetIndex = index;
    triggerAsyncUpdate();
}

String DocumentLibrary::getProgramName(int index) {
    if (isPositiveAndBelow(index, allDocs.size())) {
        return allDocs.getReference(index).getName();
    }

    return {};
}

bool DocumentLibrary::readSettingsFile() {
    String filename(getStrConstant(DocSettingsDir));

    File file(filename);
    XmlDocument settingsDoc(file);
    std::unique_ptr<XmlElement> elem = settingsDoc.getDocumentElement(false);

    if (elem == nullptr || !file.existsAsFile()) {
        for (auto &allDoc : allDocs) {
            int code = allDoc.getKey().hashCode();

            if(allDoc.getRating() > 0.f) {
                ratingsMap.set(code, allDoc.getRating());
            }
        }

        settingsArePending = true;
    } else {
        if (XmlElement* ratingsElem = elem->getChildByName("Ratings")) {
            for(auto ratingElem : ratingsElem->getChildWithTagNameIterator("Rating")) {
                int code 	= ratingElem->getIntAttribute("code", 0);
                float value = ratingElem->getDoubleAttribute("value", 0);

                if(code != 0) {
                    ratingsMap.set(code, value);
                }
            }
        }

        if (XmlElement* dismissed = elem->getChildByName("Dismissed")) {
            for(auto dissedElem : dismissed->getChildWithTagNameIterator("Dismissal")) {
                int code = dissedElem->getIntAttribute("code", 0);

                if(code != 0) {
                    dismissedSet.insert(code);
                }
            }
        }

        if (XmlElement* downloaded = elem->getChildByName("DownloadedPresets")) {
            for(auto dldElem : downloaded->getChildWithTagNameIterator("Download")) {
                int code = dldElem->getIntAttribute("code", 0);

                if(code != 0) {
                    downloadedSet.insert(code);
                }
            }
        }
    }

    return true;
}

void DocumentLibrary::writeSettingsFile() {
    if (Util::assignAndWereDifferent(settingsArePending, false)) {
        std::unique_ptr<XmlElement> docElem(new XmlElement("PresetSettings"));

        {
            auto* ratingsElem = new XmlElement("Ratings");
            HashMap<int, float>::Iterator iter(ratingsMap);

            while (iter.next()) {
                auto* rating = new XmlElement("Rating");

                rating->setAttribute("code",  iter.getKey());
                rating->setAttribute("value", iter.getValue());
                ratingsElem->addChildElement(rating);
            }

            docElem->addChildElement(ratingsElem);
        }

        {
            auto* dismissed = new XmlElement("Dismissed");

            for (int it : dismissedSet) {
                auto* dissed = new XmlElement("Dismissal");

                dissed->setAttribute("code", it);
                dismissed->addChildElement(dissed);
            }

            docElem->addChildElement(dismissed);
        }

        {
            auto* downloaded = new XmlElement("DownloadedPresets");

            for (int it : downloadedSet) {
                auto* dldElem = new XmlElement("Download");

                dldElem->setAttribute("code", it);
                downloaded->addChildElement(dldElem);
            }

            docElem->addChildElement(downloaded);
        }

        String docString = docElem->toString();
        String filename(getStrConstant(DocSettingsDir));

        if (filename.isEmpty()) {
            jassertfalse;
            return;
        }

        File file(filename);

        if(file.existsAsFile())
            (void) file.deleteFile();

        if(! file.create().wasOk())
            return;

        std::unique_ptr<FileOutputStream> fileStream = file.createOutputStream();

        if (fileStream != nullptr) {
            fileStream->writeString(docString);
            fileStream->flush();
        }
    }
}
