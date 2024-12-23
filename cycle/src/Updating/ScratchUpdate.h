#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class ScratchUpdate :
        public SingletonAccessor
    , 	public Updateable {
public:
    explicit ScratchUpdate(SingletonRepo* repo);

    void performUpdate(UpdateType updateType) override;
};