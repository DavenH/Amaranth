#ifndef UPDATING_DYNAMICDETAILUPDATER_H_
#define UPDATING_DYNAMICDETAILUPDATER_H_

#include <Design/Updating/Updateable.h>
#include <Design/Updating/Updater.h>

class DynamicDetailUpdateable : public Updateable::Listener
{
public:
    void preUpdateHook(int updateType) {
        if (updateType == UpdateType::ReduceDetail) {
            detailIsReduced = true;
        } else if (updateType == UpdateType::RestoreDetail) {
            detailIsReduced = false;
        }
    }

    bool isDetailReduced() { return detailIsReduced; }

protected:

	bool detailIsReduced;
};


#endif
