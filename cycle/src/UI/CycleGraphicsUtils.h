#pragma once

#include <App/SingletonAccessor.h>
#include <Binary/CycleImages.h>

class CycleGraphicsUtils :
		public SingletonAccessor {
public:
	explicit CycleGraphicsUtils(SingletonRepo* repo) : SingletonAccessor(repo, "CycleGraphicsUtils"),
	                                                   blackground(PNGImageFormat::loadFrom(
		                                                   CycleImages::blackground_png,
		                                                   CycleImages::blackground_pngSize))
{
}

	~CycleGraphicsUtils() override;

	void fillBlackground(Component* component, Graphics& g) {
		g.drawImage(blackground, 0, 0, component->getWidth(), component->getHeight(),
			0, 0, blackground.getWidth(), blackground.getHeight(), false);
	}

private:

	Image blackground;
};
