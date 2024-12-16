#pragma once

#include <App/MeshLibrary.h>
#include <App/SingletonAccessor.h>

class VersionConsiliator :
		public SingletonAccessor
	,	public MeshLibrary::Listener
{
public:
	explicit VersionConsiliator(SingletonRepo* repo);
	void transform(XmlElement* element);

private:
};