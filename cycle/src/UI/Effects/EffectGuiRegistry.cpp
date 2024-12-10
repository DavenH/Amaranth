#include <App/SingletonRepo.h>
#include "EffectGuiRegistry.h"
#include "ReverbUI.h"
#include "DelayUI.h"
#include "IrModellerUI.h"
#include "WaveshaperUI.h"
#include "UnisonUI.h"
#include "EqualizerUI.h"


EffectGuiRegistry::EffectGuiRegistry(SingletonRepo* repo) :
		SingletonAccessor(repo, "EffectGuiRegistry")
{
}


EffectGuiRegistry::~EffectGuiRegistry()
{
}


bool EffectGuiRegistry::readXML(const XmlElement *topElement)
{
	XmlElement* effectsElem = topElement->getChildByName("Effects");

	if(! effectsElem)
		return false;

	Savable* effects[] =
	{
			&getObj(IrModellerUI)
		,	&getObj(UnisonUI)
		,	&getObj(WaveshaperUI)
		,	&getObj(DelayUI)
		,	&getObj(ReverbUI)
		,	&getObj(EqualizerUI)
	};

	for(int i = 0; i < numElementsInArray(effects); ++i)
	{
		if(! effects[i]->readXML(effectsElem))
			return false;
	}

	return true;
}


void EffectGuiRegistry::writeXML(XmlElement *topElement) const
{
  #ifndef DEMO_VERSION
	XmlElement* effectsElem = new XmlElement("Effects");

	const Savable* effects[] =
	{
			&getObj(IrModellerUI)
		,	&getObj(UnisonUI)
		,	&getObj(WaveshaperUI)
		,	&getObj(DelayUI)
		,	&getObj(ReverbUI)
		,	&getObj(EqualizerUI)
	};

	for(int i = 0; i < numElementsInArray(effects); ++i)
		effects[i]->writeXML(effectsElem);

	topElement->addChildElement(effectsElem);
  #endif
}
