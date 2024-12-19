#pragma once
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>

class EnvelopeDelegate :
        public SingletonAccessor
    ,	public Updateable {
public:
    explicit EnvelopeDelegate(SingletonRepo* repo);
    void performUpdate(UpdateType updateType) override;

    ~EnvelopeDelegate() override;
};