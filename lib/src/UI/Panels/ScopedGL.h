#pragma once

class ScopedDisable {
public:
    explicit ScopedDisable(int glConstant, bool active = true);
    virtual ~ScopedDisable();

private:
    int glConstant;
    bool active;
};

class ScopedEnable {
public:
    explicit ScopedEnable(int glConstant, bool active = true);
    virtual ~ScopedEnable();

private:
    int glConstant;
    bool active;
};

class ScopedEnableClientState {
public:
    explicit ScopedEnableClientState(int glElement, bool active = true);
    virtual ~ScopedEnableClientState();

private:
    int glElement;
    bool active;
};


class ScopedElement {
public:
    explicit ScopedElement(int glElement);
    virtual ~ScopedElement();

private:
    int glElement;
};
