#ifndef UI_CYCLEGRAPHICSUTILS_H_
#define UI_CYCLEGRAPHICSUTILS_H_

#include <App/SingletonAccessor.h>

class CycleGraphicsUtils :
		public SingletonAccessor {
public:
	CycleGraphicsUtils(SingletonRepo* repo);
	virtual ~CycleGraphicsUtils();

	void fillBlackground(Component* component, Graphics& g);

private:

	Image blackground;
};

#endif
