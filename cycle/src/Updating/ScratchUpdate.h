#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class ScratchUpdate :
		public SingletonAccessor
	, 	public Updateable {
public:
	explicit ScratchUpdate(SingletonRepo* repo);

	~ScratchUpdate() override;

	void performUpdate(int updateType) override;
};