#pragma once
class ScopedBooleanSwitcher {
private:
    bool& value;

public:
    ScopedBooleanSwitcher(bool& value) : value(value) {
        this->value = true;
    }

    ~ScopedBooleanSwitcher() {
        value = false;
    }

private:
    ScopedBooleanSwitcher& operator=(const ScopedBooleanSwitcher& sbs);
};
