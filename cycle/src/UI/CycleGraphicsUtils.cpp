#include "CycleGraphicsUtils.h"
#include "../Binary/CycleImages.h"

CycleGraphicsUtils::CycleGraphicsUtils(SingletonRepo* repo) :
		SingletonAccessor(repo, "CycleGraphicsUtils") {
	blackground = PNGImageFormat::loadFrom(CycleImages::blackground_png, CycleImages::blackground_pngSize);
}

CycleGraphicsUtils::~CycleGraphicsUtils() {
}

void CycleGraphicsUtils::fillBlackground(Component* component, Graphics& g) {
    g.drawImage(blackground, 0, 0, component->getWidth(), component->getHeight(),
                0, 0, blackground.getWidth(), blackground.getHeight(), false);
}
