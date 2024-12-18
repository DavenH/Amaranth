#pragma once

#include <Design/Updating/Updateable.h>
#include <Design/Updating/Updater.h>

class DynamicDetailUpdateable : public Updateable::Listener
{
public:
    void preUpdateHook(int updateType) {
        if (updateType == ReduceDetail) {
            detailIsReduced = true;
        } else if (updateType == RestoreDetail) {
            detailIsReduced = false;
        }
    }

    bool isDetailReduced() { return detailIsReduced; }

protected:

    bool detailIsReduced;
};
