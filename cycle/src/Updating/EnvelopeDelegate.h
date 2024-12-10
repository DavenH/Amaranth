#pragma once
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class EnvelopeDelegate :
		public SingletonAccessor
	,	public Updateable {
public:
	EnvelopeDelegate(SingletonRepo* repo);
	void performUpdate(int updateType);
	virtual ~EnvelopeDelegate();
};