#pragma once

#include <App/SingletonRepo.h>
#include <Inter/MorphPositioner.h>
#include <UI/IConsole.h>

namespace CycleV2 {

class TrimeshPanelEnvironment {
public:
    TrimeshPanelEnvironment();

    SingletonRepo& getRepo() { return repo; }
    MorphPositioner& getMorphPositioner() { return morphPositioner; }

    void setMorphPosition(const MorphPosition& position, int primaryAxis);

private:
    class NullConsole : public IConsole {
    public:
        explicit NullConsole(SingletonRepo* repo);

        void write(const String&, int) override {}
        void setKeys(const String&) override {}
        void setMouseUsage(const MouseUsage&) override {}
        void reset() override {}
    };

    class NodeMorphPositioner : public MorphPositioner {
    public:
        void setPosition(const MorphPosition& position, int primaryAxis);

        MorphPosition getMorphPosition() override { return morph; }
        MorphPosition getOffsetPosition(bool) override { return { 0.f, 0.f, 0.f }; }
        int getPrimaryDimension() override { return primaryDimension; }
        float getYellow() override { return morph.time.getCurrentValue(); }
        float getRed() override { return morph.red.getCurrentValue(); }
        float getBlue() override { return morph.blue.getCurrentValue(); }
        float getValue(int dim) override;

    private:
        MorphPosition morph { 0.5f, 0.5f, 0.5f };
        int primaryDimension {};
    };

    SingletonRepo repo;
    NullConsole console;
    NodeMorphPositioner morphPositioner;
};

}
