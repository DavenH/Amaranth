#ifndef IVALIDATOR_H_
#define IVALIDATOR_H_

class IValidator {
public:
	virtual bool isDemoMode() = 0;
	virtual bool isBetaMode() = 0;
};

#endif
