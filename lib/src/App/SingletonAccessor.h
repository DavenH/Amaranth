#pragma once

#include <utility>
#include "../Obj/Ref.h"
#include "JuceHeader.h"

using namespace juce;

class SingletonRepo;

class SingletonAccessor {
public:
	virtual ~SingletonAccessor() = default;

	SingletonAccessor(SingletonRepo* repo, String name)
			: initOrder(-1), name(std::move(name)), repo(repo) {
	}

	SingletonAccessor& operator=(const SingletonAccessor& other) = default;

	SingletonAccessor(const SingletonAccessor& client)	{ operator=(client); }
	SingletonRepo* getSingletonRepo() 					{ return repo;	 }
	int getInitOrder() const 							{ return initOrder;  }
	void setInitOrder(int order)						{ initOrder = order; }
	virtual const String& getName() 					{ return name;		 }

	virtual void init() {}
	virtual void reset() {}

protected:
	int initOrder{};
	String name;
	Ref<SingletonRepo> repo;
};
