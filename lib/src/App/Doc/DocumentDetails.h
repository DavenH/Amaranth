#pragma once

#include <vector>
#include "Savable.h"
#include "JuceHeader.h"

using std::vector;
using namespace juce;

[[nodiscard]]
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

    void setAuthor(const String& author)		{ this->author 			= author; 		}
    void setPack(const String& pack)			{ this->pack 			= pack; 		}
    void setName(const String& name)			{ this->name 			= name; 		}
    void setRating(double rating)				{ this->rating 			= rating; 		}
    void setRevision(int revision)				{ this->revision 		= revision; 	}
    void setSizeBytes(int sizeBytes)			{ this->sizeBytes		= sizeBytes;	}
    void setTags(const StringArray& tags)		{ this->tags 			= tags; 		}
    void setFilename(const String& filename)	{ this->filename 		= filename; 	}
    void setDateMillis(int64 dateMillis)		{ this->dateMillis 		= dateMillis; 	}
    void setModifiedMillis(int64 millis)		{ this->modifiedMillis 	= millis; 		}
    void setIsRemote(bool isRemote)				{ this->remote 			= isRemote; 	}
    void setIsWave(bool isWave)					{ this->isWav 			= isWave; 		}
    void setProductVersion(double v)			{ this->productVersion 	= v; 			}
    void setNewlyDownloaded(bool is)			{ this->newlyDownloaded = is; 			}

    bool 				isRemote() const 		{ return remote; 			}
    bool 				isWave() const			{ return isWav; 			}
    bool				isNew() const			{ return newlyDownloaded; 	}
    int 				getRevision() const		{ return revision; 			}
    int 				getSizeBytes() const	{ return sizeBytes; 		}
    int64 				getDateMillis() const	{ return dateMillis; 		}
    int64 				getModifMillis() const	{ return modifiedMillis; 	}
    const String& 		getAuthor() const		{ return author; 			}
    const String& 		getPack() const			{ return pack; 				}
    const String& 		getName() const			{ return name; 				}
    const String& 		getFilename() const		{ return filename; 			}
    const StringArray& 	getTags() const			{ return tags; 				}
    float 				getRating() const		{ return static_cast<float>(rating); }
    double 				getProductVersion() const { return productVersion; 	}
    String getKey() const 						{ return name + "." + author + "." + pack; }
    bool& getSuppressDateFlag() 				{ return suppressDate; 		}
    bool& getSuppressRevFlag() 					{ return suppressRevision; 	}

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
