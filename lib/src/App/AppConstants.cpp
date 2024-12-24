#include "AppConstants.h"

AppConstants::AppConstants(SingletonRepo* repo) : SingletonAccessor(repo, "AppConstants"){
}

int AppConstants::getAppConstant(int key) const {
    if(values.contains(key)) {
        return values[key];
    }

    return -1;
}

double AppConstants::getRealAppConstant(int key) const {
    if (realValues.contains(key)) {
        return realValues[key];
    }

    return -1.0;
}

String AppConstants::getStringAppConstant(int key) const {
    if(strValues.contains(key)) {
        return strValues[key];
    }

    return {};
}
