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
    struct Diagnostics {
        bool attached{};
        bool pollSucceeded{};
        int width{};
        int height{};
        double renderingScale{};
        int renderCount{};
        int contextCreateCount{};
        int contextCloseCount{};
        int errorCount{};
        int lastErrorCode{};
        int polledErrorCode{};
        String lastErrorName;
        String polledErrorName;
        String lastErrorPhase;
    };

    OpenGLBase(OpenGLRenderer* renderer, Component* component);

    virtual ~OpenGLBase() = default;

    void detach();
    void attach();
    void printErrors(SingletonRepo* repo, const String& phase = {});
    Diagnostics getDiagnostics(bool pollNow, const String& phase = "diagnostic");

    OpenGLContext context;

protected:
    void noteContextCreated();
    void noteContextClosing();
    void noteRender();
    void clearAndOrtho(int width, int height);

    bool smoothLines;
    Ref<CommonGL> commonGL;

    friend class CommonGL;
private:
    int pollErrorNow(const String& phase);
    void recordError(int errorCode, const String& phase);

    Ref<Component> component;
    Ref<OpenGLRenderer> renderer;

    int renderCount{};
    int contextCreateCount{};
    int contextCloseCount{};
    int errorCount{};
    int lastErrorCode{};
    String lastErrorName;
    String lastErrorPhase;
};
