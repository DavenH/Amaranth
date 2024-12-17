
/**
 * Utility class that lets you assign a pointer once, then use this as that pointer
 */
#pragma once
template<class T>
class Ref {
public:
	Ref() : object(0)			{}
	Ref(T* t) : object(t) 		{}
	~Ref() 						= default;
	Ref(const Ref& ref)			{ object = ref.object; }

	bool isNull()				{ return object == 0; }

	operator const T*() const 	{ return object; 	}
	operator T*()				{ return object; 	}
	T& operator*()				{ return *object; 	}
	T* operator->()				{ return object; 	}
	const T* operator->() const	{ return object;	}
	T* get()					{ return object;	}

    Ref& operator=(T* pointer) {
        if (object == 0)
            object = pointer;
        else {
            object = pointer;
        }

		return *this;
	}

private:
	T* object;
};
