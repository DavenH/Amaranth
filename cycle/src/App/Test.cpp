#include <App/Doc/Document.h>
#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>

class Test : SingletonAccessor
{
	void someting()
	{
		Document& doc = getObj(Document);
	}
};
