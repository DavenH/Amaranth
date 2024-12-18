#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class SpectDelegate :
        public SingletonAccessor
    ,	public Updateable {
public:
    void performUpdate(UpdateType updateType) override;

    SpectDelegate(SingletonRepo* repo);
    ~SpectDelegate() override;
};