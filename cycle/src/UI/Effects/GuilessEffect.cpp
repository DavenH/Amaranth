#include <App/EditWatcher.h>
#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>
#include <UI/Widgets/Knob.h>
#include <Util/ScopedBooleanSwitcher.h>

#include "GuilessEffect.h"

#include <Design/Updating/Updater.h>
#include <Util/Util.h>

#include "../../Audio/Effects/AudioEffect.h"
#include "../CycleGraphicsUtils.h"

GuilessEffect::GuilessEffect(const String& name, const String& displayName, int numParams,
                             SingletonRepo* repo, Effect* effect, int source) :
        SingletonAccessor(repo, name)
    ,	Worker(repo, name)
    , 	effect			(effect)
    ,	fxEnum			(fxEnum)
    ,	enabled			(false)
    ,	updateSource	(source)
    ,	minTitleSize	(50)
    ,	font			(15, Font::bold)
    ,	enableButton	(5, 5, this, repo, "Enable effect")
    ,	title			(repo, displayName.toUpperCase())
{
    addAndMakeVisible(&enableButton);
    addAndMakeVisible(&title);

    title.setColorHSV(0, 0.35f);
}

void GuilessEffect::init()
{
    paramGroup->listenToKnobs();
    paramGroup->addKnobsTo(this);
}


void GuilessEffect::paint(Graphics & g)
{
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    auto& mg = getObj(MiscGraphics);
    Font* silkscreen = mg.getSilkscreen();

    Rectangle<int> gradBounds(getLocalBounds().removeFromRight(100));

    {
        g.setGradientFill(ColourGradient(Colour::greyLevel(0.25f),
                          gradBounds.getRight() - 10, gradBounds.getY(),
                          Colour::greyLevel(0.25f).withAlpha(0.f),
                          gradBounds.getX(), gradBounds.getY(), true));

        g.fillRect(gradBounds);
    }

    Array<int> knobIdcs = getApplicableKnobs();

    g.setColour(Colour::greyLevel(0.235f));
    for(int idx : knobIdcs) {
        Slider* knob = paramGroup->getKnob<Knob>(idx);
        String text  = getKnobName(idx);

        int width 	 = knob->getRight() - knob->getX();
        int strWidth = silkscreen->getStringWidth(text);
        int x 		 = knob->getX() + (width - strWidth) / 2;
        int y 		 = knob->getBottom() + 2;

        MiscGraphics::drawShadowedText(g, text, x, y, *silkscreen);
    }

    Array<Rectangle<int> > rects = getOutlinableRects();

    g.setColour(Colour::greyLevel(0.05f));
    for(auto rect : rects) {
        Path strokePath;
        strokePath.addRoundedRectangle(rect.getX(), rect.getY(), rect.getWidth() + 1, rect.getHeight() + 1, 2.f);
        strokePath.applyTransform(AffineTransform::translation(-0.5f, -0.5f));

        g.strokePath(strokePath, PathStrokeType(1.f));
    }
}

Array<Rectangle<int> > GuilessEffect::getOutlinableRects() {
    return {};
}

Array<int> GuilessEffect::getApplicableKnobs() {
    Array<int> a;

    for (int i = 0; i < paramGroup->getNumParams(); ++i)
        a.add(i);

    return a;
}

void GuilessEffect::setEffectEnabled(bool enabled, bool sendUIUpdate, bool sendDspUpdate) {
    if (!Util::assignAndWereDifferent(this->enabled, (int) enabled)) {
        return;
    }

    effectEnablementChanged(sendUIUpdate, sendDspUpdate);
    enableButton.setHighlit(this->enabled);
}

void GuilessEffect::buttonClicked(Button* button) {
    if (button == &enableButton) {
        setEffectEnabled(!enabled, true, true);
    }
}

void GuilessEffect::restoreDetail() {
    getObj(Updater).update(updateSource, RestoreDetail);
}

void GuilessEffect::reduceDetail() {
    getObj(Updater).update(updateSource, ReduceDetail);
}

void GuilessEffect::doGlobalUIUpdate(bool force) {
    getObj(Updater).update(updateSource);
}

void GuilessEffect::writeXML(XmlElement* registryElem) const {
  #ifndef DEMO_VERSION
    auto* effectElem = new XmlElement(getEffectName());

    paramGroup->writeKnobXML(effectElem);

    effectElem->setAttribute("enabled", enabled);
    registryElem->addChildElement(effectElem);
  #endif
}

bool GuilessEffect::readXML(const XmlElement* element) {
    XmlElement* effectElem = element->getChildByName(getEffectName());

    if (effectElem != nullptr) {
        paramGroup->readKnobXML(effectElem);
        setEffectEnabled(effectElem->getBoolAttribute("enabled"), false, true);
        repaint();
    } else {
        ScopedBooleanSwitcher sbs(paramGroup->updatingAllSliders);

        for (int i = 0; i < paramGroup->getNumParams(); ++i)
            paramGroup->setKnobValue(i, 0.5, false);
    }

    return true;
}

void GuilessEffect::resized() {
    bool tiny = getWidth() < 250;
    bool big = getWidth() > 400;
    int vertPad = (getHeight() - 36) / 3;
    int rightSize = jmax(minTitleSize, 5 + title.getFullSize());

    Rectangle<int> r = getLocalBounds();
    Rectangle<int> right = r.removeFromRight(rightSize);
    right.removeFromRight(5);
    right.removeFromLeft(-5);

    right.removeFromTop(vertPad);
    title.setBounds(right.removeFromTop(12).removeFromRight(title.getFullSize()));

    right.removeFromBottom(jmin(5, vertPad));
    enableButton.setBounds(right.removeFromRight(24).removeFromBottom(24));
    right.removeFromRight(4);

    setExtraTitleElements(right);
    setExtraRightElements(r);

    Array<int> knobIdcs = getApplicableKnobs();

    int minSize = jmin(getWidth(), getHeight());
    int num = knobIdcs.size();
    int knobSize = jmin(int(minSize * 0.7f), (r.getWidth() - (big ? 30 : !tiny ? 20 : 5)) / num);
    int knobSpacing = (r.getWidth() - num * knobSize) / (num + 1);

    r.reduce(0, jmax(0, (r.getHeight() - knobSize) / 2));
    layoutKnobs(r, knobIdcs, knobSize, knobSpacing);
}

void GuilessEffect::layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing) {
    for (int knobIdc : knobIdcs) {
        paramGroup->getKnob<Slider>(knobIdc)->setBounds(rect.removeFromLeft(knobSize));
        rect.removeFromLeft(knobSpacing);
    }
}

void GuilessEffect::effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate) {
    if (sendUIUpdate) {
        paramGroup->forceNextUIUpdate = true;
        paramGroup->triggerRefreshUpdate();
    }
}

bool GuilessEffect::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    return effect->paramChanged(knobIndex, knobValue, doFurtherUpdate);
}

bool GuilessEffect::shouldTriggerGlobalUpdate(Slider* slider) {
    return enabled;
}

Component* GuilessEffect::getComponent(int which) {
    if (isPositiveAndBelow(which, paramGroup->getNumParams()))
        return paramGroup->getKnob<Slider>(which);

    return nullptr;
}

String GuilessEffect::getKnobName(int index) const {
    return {};
}
