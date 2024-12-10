#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class ScratchUpdate :
		public SingletonAccessor
	, 	public Updateable {
public:
	ScratchUpdate(SingletonRepo* repo);
	virtual ~ScratchUpdate();

	void performUpdate(int updateType);
};