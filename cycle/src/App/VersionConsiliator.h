#ifndef APP_VERSIONCONSILIATOR_H_
#define APP_VERSIONCONSILIATOR_H_

#include <App/SingletonAccessor.h>

class VersionConsiliator :
		public SingletonAccessor
{
public:
	void transform(XmlElement* element);

private:
};


#endif
