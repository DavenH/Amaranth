#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class MorphUpdate :
        public SingletonAccessor
    , 	public Updateable {
public:
    explicit MorphUpdate(SingletonRepo* repo);

    void performUpdate(UpdateType updateType) override;
};
