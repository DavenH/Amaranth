#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class MorphUpdate :
		public SingletonAccessor
	, 	public Updateable {
public:
	MorphUpdate(SingletonRepo* repo);
	virtual ~MorphUpdate();

	void performUpdate(int updateType);
};
