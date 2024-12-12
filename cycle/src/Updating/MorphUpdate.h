#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class MorphUpdate :
		public SingletonAccessor
	, 	public Updateable {
public:
	MorphUpdate(SingletonRepo* repo);

	~MorphUpdate() override;

	void performUpdate(int updateType) override;
};
