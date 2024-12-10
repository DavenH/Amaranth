#ifndef _EFFECTGUIREGISTRY_H_
#define _EFFECTGUIREGISTRY_H_

#include <App/SingletonAccessor.h>
#include <App/Doc/Savable.h>
#include <Obj/Ref.h>

class EffectGuiRegistry :
	public Savable,
	public SingletonAccessor
{
public:
	EffectGuiRegistry(SingletonRepo* main);
	virtual ~EffectGuiRegistry();

	bool readXML(const XmlElement* topElement);
	void writeXML(XmlElement* topElement) const;
};

#endif
