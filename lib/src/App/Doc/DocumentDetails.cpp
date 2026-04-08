#include "DocumentDetails.h"
#include "PresetJson.h"
#include "../../Util/Util.h"

int DocumentDetails::sortColumn = DocumentDetails::Name;

DocumentDetails::DocumentDetails() :
        name            ("Untitled")
    ,   author          ("Anonymous")
    ,   pack            ("none")
    ,   rating          (0.0)
    ,   revision        (0)
    ,   sizeBytes       (0)
    ,   dateMillis      (0)
    ,   modifiedMillis  (0)
    ,   productVersion  (1)
    ,   remote          (false)
    ,   isWav           (false)
    ,   suppressDate    (false)
    ,   newlyDownloaded (false) {
}

bool DocumentDetails::readXML(const XmlElement* element) {
    if(element == nullptr) {
        return false;
    }

    const XmlElement* detailsElem = element->getChildByName("PresetDetails");

    if(detailsElem == nullptr)
        detailsElem = element;

    rating          = detailsElem->getDoubleAttribute("rating",         0);
    revision        = detailsElem->getIntAttribute   ("revision",       1);
    name            = detailsElem->getStringAttribute("name",           "Untitled");
    author          = detailsElem->getStringAttribute("author",         "Anonymous");
    pack            = detailsElem->getStringAttribute("pack",           "none");
    productVersion  = detailsElem->getDoubleAttribute("productVersion", productVersion);

    if(pack.isEmpty()) {
        pack = "none";
    }

    if(author.isEmpty()) {
        author = "Anonymous";
    }

    if(name.isEmpty()) {
        name = "Untitled";
    }

    String dateString = detailsElem->getStringAttribute("date", String(Time::currentTimeMillis()));
    dateMillis        = dateString.getLargeIntValue();

    String modifString = detailsElem->getStringAttribute("modified", String(Time::currentTimeMillis()));
    modifiedMillis     = modifString.getLargeIntValue();

    tags.clear();

    for(auto tagElem : detailsElem->getChildWithTagNameIterator("Tag")) {
        if (tagElem == nullptr) {
            return false;
        }

        String tag = tagElem->getStringAttribute("value", String());

        tags.add(tag.trim());
    }

    return true;
}


void DocumentDetails::writeXML(XmlElement* detailsElem) const {
//  XmlElement* detailsElem = new XmlElement("PresetDetails");

    detailsElem->setAttribute("name",   name.substring(0, 40));
    detailsElem->setAttribute("pack",   pack.substring(0, 40));
    detailsElem->setAttribute("author", author.substring(0, 50));

    if(! suppressRevision)
        detailsElem->setAttribute("revision", revision + 1);

    if (!suppressDate) {
        detailsElem->setAttribute("modified", String(Time::currentTimeMillis()));
        detailsElem->setAttribute("date", String(dateMillis));
    }

    detailsElem->setAttribute("productVersion", productVersion);

    for (int i = 0; i < jmin(8, tags.size()); ++i) {
        XmlElement* tagElem = new XmlElement("Tag");
        tagElem->setAttribute("value", tags[i].substring(0, 20));
        detailsElem->addChildElement(tagElem);
    }


//  element->addChildElement(detailsElem);
}

var DocumentDetails::writeJSON() const {
    auto json = PresetJson::object();

    json->setProperty("name", name);
    json->setProperty("author", author);
    json->setProperty("pack", pack);
    json->setProperty("rating", rating);
    json->setProperty("revision", revision);
    json->setProperty("sizeBytes", sizeBytes);
    json->setProperty("dateMillis", dateMillis);
    json->setProperty("modifiedMillis", modifiedMillis);
    json->setProperty("productVersion", productVersion);
    json->setProperty("remote", remote);
    json->setProperty("isWav", isWav);
    json->setProperty("newlyDownloaded", newlyDownloaded);
    json->setProperty("filename", filename);
    json->setProperty("tags", PresetJson::fromStringArray(tags));

    return PresetJson::toVar(json);
}

bool DocumentDetails::readJSON(const var& object) {
    if (PresetJson::getObject(object) == nullptr) {
        return false;
    }

    name            = PresetJson::stringProperty(object, "name", "Untitled");
    author          = PresetJson::stringProperty(object, "author", "Anonymous");
    pack            = PresetJson::stringProperty(object, "pack", "none");
    rating          = PresetJson::doubleProperty(object, "rating", 0.0);
    revision        = PresetJson::intProperty(object, "revision", 0);
    sizeBytes       = PresetJson::intProperty(object, "sizeBytes", 0);
    dateMillis      = int64(PresetJson::doubleProperty(object, "dateMillis", 0));
    modifiedMillis  = int64(PresetJson::doubleProperty(object, "modifiedMillis", 0));
    productVersion  = PresetJson::doubleProperty(object, "productVersion", 1.0);
    remote          = PresetJson::boolProperty(object, "remote", false);
    isWav           = PresetJson::boolProperty(object, "isWav", false);
    newlyDownloaded = PresetJson::boolProperty(object, "newlyDownloaded", false);
    filename        = PresetJson::stringProperty(object, "filename");
    tags            = PresetJson::stringArray(PresetJson::property(object, "tags"));

    return true;
}

void DocumentDetails::reset() {
    name            = "Untitled";
    author          = "Anonymous";
    pack            = "none";
    tags            = StringArray();
    rating          = 0.0;
    revision        = 0;
    sizeBytes       = 0;
    dateMillis      = 0;
    modifiedMillis  = 0;
    productVersion  = 1.0;
    remote          = false;
    isWav           = false;
    newlyDownloaded = false;
}

bool DocumentDetails::operator<(const DocumentDetails& details) const {
    if (isWav && sortColumn == Name) {
        return Util::pitchAwareComparison(name, details.getName()) < 0;
    }

    switch (sortColumn) {
        case Name:      return name.compareIgnoreCase(details.getName()) < 0;
        case Author:    return author.compareIgnoreCase(details.getAuthor()) < 0;
        case Pack:      return pack.compareIgnoreCase(details.getPack()) < 0;
        case Rating:    return rating           < details.getRating();
        case Revision:  return revision         < details.getRevision();
        case Date:      return dateMillis       < details.getDateMillis();
        case Modified:  return modifiedMillis   < details.getModifMillis();
        case Size:      return sizeBytes        < details.getSizeBytes();
        case Version:   return productVersion   < details.getProductVersion();
        case Tag: {
            if(tags.size() == 0 && details.tags.size() == 0) {
                return name.compareIgnoreCase(details.getName()) < 0;
            }

            if(tags.size() > 0 && details.tags.size() > 0) {
                return tags[0].compareIgnoreCase(details.tags[0]) < 0;
            }
            return tags.size() > details.tags.size();
        }

        default:
            break;
    }

    return false;
}

DocumentDetails& DocumentDetails::operator=(const DocumentDetails& other) {
    author          = other.author;
    dateMillis      = other.dateMillis;
    filename        = other.filename;
    isWav           = other.isWav;
    modifiedMillis  = other.modifiedMillis;
    name            = other.name;
    newlyDownloaded = other.newlyDownloaded;
    pack            = other.pack;
    productVersion  = other.productVersion;
    rating          = other.rating;
    remote          = other.remote;
    revision        = other.revision;
    sizeBytes       = other.sizeBytes;
    tags            = other.tags;

    return *this;
}

bool DocumentDetails::operator==(const DocumentDetails& deets) const {
    return  name    == deets.name   &&
            author  == deets.author &&
            pack    == deets.pack   &&
            remote  == deets.remote;
}
