#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class Directories : public SingletonAccessor {
public:
	explicit Directories(SingletonRepo* repo);
	~Directories() override = default;

	void init() override;

	String getMeshDir();
	String getPresetDir();
	String getUserPresetDir();
	String getUserMeshDir();
	String getTutorialDir();

	void setMeshDir(const String& dir) 			{ meshDir 	 = dir; 			}
	void setHomeUrl(const String& url)			{ homeUrl 	 = url; 			}
	const String& getHomeUrl() 					{ return homeUrl; 				}

	void setLastPresetDir(const String& dir) 	{ lastPresetDirectory = dir; 	}
    void setLastWaveDirectory(const String& str){ lastWaveDirectory = str; 		}
    void setLoadedWave(const String& str) 		{ loadedWave = str; 			}

	[[nodiscard]] const String& getLoadedWavePath() const		{ return loadedWave; 			}
    [[nodiscard]] const String& getLastWaveDirectory() const 	{ return lastWaveDirectory; 	}
	[[nodiscard]] const String& getLastPresetDir() const		{ return lastPresetDirectory; 	}

private:
	String loadedWave;
	String lastWaveDirectory;
	String lastPresetDirectory;

	String wavDir;
	String meshDir;
	String homeUrl;
	String contentDir;
};