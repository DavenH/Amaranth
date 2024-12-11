#pragma once

#include <App/SingletonAccessor.h>

class VersionConsiliator :
		public SingletonAccessor
{
public:
	void transform(XmlElement* element);

private:
};