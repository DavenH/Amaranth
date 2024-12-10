#ifndef DIRECTORIES_H_
#define DIRECTORIES_H_

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"

class Directories : public SingletonAccessor {
public:
	Directories(SingletonRepo* repo);
	virtual ~Directories() {}

	void init();

	String getMeshDir();
	String getPresetDir();
	String getUserPresetDir();
	String getUserMeshDir();
	String getTutorialDir();

//	String getAudioTrackFilename();

//	void setContentDir(const String& dir);
	void setMeshDir(const String& dir) 			{ meshDir 	 = dir; 			}
	void setHomeUrl(const String& url)			{ homeUrl 	 = url; 			}
	const String& getHomeUrl() 					{ return homeUrl; 				}

	void setLastPresetDir(const String& dir) 	{ lastPresetDirectory = dir; 	}
    void setLastWaveDirectory(const String& str){ lastWaveDirectory = str; 		}
    void setLoadedWave(const String& str) 		{ loadedWave = str; 			}

	const String& getLoadedWavePath() const		{ return loadedWave; 			}
    const String& getLastWaveDirectory() const 	{ return lastWaveDirectory; 	}
	const String& getLastPresetDir() const		{ return lastPresetDirectory; 	}

private:
	String loadedWave;
	String lastWaveDirectory;
	String lastPresetDirectory;

	String wavDir;
	String meshDir;
	String homeUrl;
	String contentDir;
};

#endif
