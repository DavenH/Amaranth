#include "AppConstants.h"

int AppConstants::getAppConstant(int key) {
	if(values.contains(key))
		return values[key];

	return -1;
}

double AppConstants::getRealAppConstant(int key) {
    if (realValues.contains(key))
        return realValues[key];

    return -1.0;
}


String AppConstants::getStringAppConstant(int key) {
	if(strValues.contains(key))
		return strValues[key];

	return String();
}
