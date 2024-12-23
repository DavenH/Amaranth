#pragma once
#include <vector>
#include "JuceHeader.h"
#include "../../Obj/Ref.h"
using namespace juce;

using std::vector;

class CommonGL;
class SingletonRepo;

class OpenGLBase {
public:
    OpenGLBase(OpenGLRenderer* renderer, Component* component);
    void detach();
    void attach();
    void printErrors(SingletonRepo* repo);

    OpenGLContext context;

protected:
    void clearAndOrtho(int width, int height);

    bool smoothLines;
    Ref<CommonGL> commonGL;

    friend class CommonGL;
private:
    Ref<Component> component;
    Ref<OpenGLRenderer> renderer;
};