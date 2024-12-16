#pragma once

class IValidator {
public:
	virtual bool isDemoMode() = 0;
	virtual bool isBetaMode() = 0;
};
