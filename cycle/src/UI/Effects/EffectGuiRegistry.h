#pragma once

#include <App/SingletonAccessor.h>
#include <App/Doc/Savable.h>

class EffectGuiRegistry :
	public Savable,
	public SingletonAccessor
{
public:
	explicit EffectGuiRegistry(SingletonRepo* main);

	~EffectGuiRegistry() override;

	bool readXML(const XmlElement* topElement) override;
	void writeXML(XmlElement* topElement) const override;
};
