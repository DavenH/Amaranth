#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class Directories : public SingletonAccessor {
public:
	explicit Directories(SingletonRepo* repo);
	~Directories() override = default;

	void init() override;

	void setMeshDir(const String& dir) 			{ meshDir 	 = dir; 			}
	void setHomeUrl(const String& url)			{ homeUrl 	 = url; 			}
	const String& getHomeUrl() 					{ return homeUrl; 				}

	void setLastPresetDir(const String& dir) 	{ lastPresetDirectory = dir; 	}
    void setLastWaveDirectory(const String& str){ lastWaveDirectory = str; 		}
    void setLoadedWave(const String& str) 		{ loadedWave = str; 			}

	[[nodiscard]] const String& getLoadedWavePath() const	 { return loadedWave; 			}
    [[nodiscard]] const String& getLastWaveDirectory() const { return lastWaveDirectory; 	}
	[[nodiscard]] const String& getLastPresetDir() const	 { return lastPresetDirectory; 	}

	[[nodiscard]] String getMeshDir() const;
	[[nodiscard]] String getPresetDir() const;
	[[nodiscard]] String getUserPresetDir() const;
	[[nodiscard]] String getUserMeshDir() const;
	[[nodiscard]] String getTutorialDir() const;

private:
	String loadedWave;
	String lastWaveDirectory;
	String lastPresetDirectory;

	String wavDir;
	String meshDir;
	String homeUrl;
	String contentDir;
};