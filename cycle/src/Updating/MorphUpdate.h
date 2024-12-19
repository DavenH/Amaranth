#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class MorphUpdate :
        public SingletonAccessor
    , 	public Updateable {
public:
    explicit MorphUpdate(SingletonRepo* repo);

    ~MorphUpdate() override;

    void performUpdate(UpdateType updateType) override;
};
