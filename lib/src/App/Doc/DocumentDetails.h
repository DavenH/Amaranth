#pragma once

#include <vector>
#include "Savable.h"
#include "JuceHeader.h"

using std::vector;
using namespace juce;

class DocumentDetails :
        public Savable
{
public:
    static int sortColumn;

    enum DocumentColumn {
        Name = 1,
        Author,
        Pack,
        Revision,
        Date,
        Modified,
        Rating,
        Size,
        Version,
        Tag,
        UploadCol,
        DismissCol,

        NumColumns
    };

    DocumentDetails();
    ~DocumentDetails() override = default;

    void setAuthor(const String& author)        { this->author          = author;       }
    void setPack(const String& pack)            { this->pack            = pack;         }
    void setName(const String& name)            { this->name            = name;         }
    void setRating(double rating)               { this->rating          = rating;       }
    void setRevision(int revision)              { this->revision        = revision;     }
    void setSizeBytes(int sizeBytes)            { this->sizeBytes       = sizeBytes;    }
    void setTags(const StringArray& tags)       { this->tags            = tags;         }
    void setFilename(const String& filename)    { this->filename        = filename;     }
    void setDateMillis(int64 dateMillis)        { this->dateMillis      = dateMillis;   }
    void setModifiedMillis(int64 millis)        { this->modifiedMillis  = millis;       }
    void setIsRemote(bool isRemote)             { this->remote          = isRemote;     }
    void setIsWave(bool isWave)                 { this->isWav           = isWave;       }
    void setProductVersion(double v)            { this->productVersion  = v;            }
    void setNewlyDownloaded(bool is)            { this->newlyDownloaded = is;           }

    [[nodiscard]] bool                  isRemote() const        { return remote;            }
    [[nodiscard]] bool                  isWave() const          { return isWav;             }
    [[nodiscard]] bool                  isNew() const           { return newlyDownloaded;   }
    [[nodiscard]] int                   getRevision() const     { return revision;          }
    [[nodiscard]] int                   getSizeBytes() const    { return sizeBytes;         }
    [[nodiscard]] int64                 getDateMillis() const   { return dateMillis;        }
    [[nodiscard]] int64                 getModifMillis() const  { return modifiedMillis;    }
    [[nodiscard]] const String&         getAuthor() const       { return author;            }
    [[nodiscard]] const String&         getPack() const         { return pack;              }
    [[nodiscard]] const String&         getName() const         { return name;              }
    [[nodiscard]] const String&         getFilename() const     { return filename;          }
    [[nodiscard]] const StringArray&    getTags() const         { return tags;              }
    [[nodiscard]] float                 getRating() const       { return static_cast<float>(rating); }
    [[nodiscard]] double                getProductVersion() const { return productVersion;  }
    [[nodiscard]] String getKey() const { return name + "." + author + "." + pack; }
    bool& getSuppressDateFlag()         { return suppressDate;      }
    bool& getSuppressRevFlag()          { return suppressRevision;  }

    void reset();
    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;
    bool operator<(const DocumentDetails& details) const;
    bool operator==(const DocumentDetails& deets) const;
    DocumentDetails& operator=(const DocumentDetails& other);

    static void setSortColumn(DocumentColumn column) { sortColumn = column; }

private:
    bool remote, isWav, newlyDownloaded;
    bool suppressDate, suppressRevision;

    int sizeBytes, revision;
    int64 dateMillis, modifiedMillis;
    double rating, productVersion;

    String name, author, pack, filename;
    StringArray tags;

    JUCE_LEAK_DETECTOR(DocumentDetails)
};
