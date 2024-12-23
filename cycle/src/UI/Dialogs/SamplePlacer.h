#pragma once

#include <vector>
#include <UI/Widgets/IconButton.h>
#include "JuceHeader.h"
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

using std::vector;

class SingletonRepo;
class SamplePlacer;
class SampleDragger;

class SamplePair : public Component {
public:
    explicit SamplePair(SamplePlacer* placer);
    void paint(Graphics& g) override;
    void split(float portion, bool horz);
    void resized() override;
    void setPortion(float portion) { this->portion = portion; }

    enum { border = 1 };

    bool 			horz;
    float 			portion{};
    File 			file;
    Rectangle<int> 	rect;

    Ref<SamplePlacer> placer;

    std::unique_ptr<SamplePair> a;
    std::unique_ptr<SamplePair> b;
    std::unique_ptr<SampleDragger> dragger;
};

class SampleDragger : public Component {
public:
    SampleDragger(SamplePair* pair, bool horz) : pair(pair),
                                                 horz(horz),
                                                 startHeightA(0), startHeightB(0), startWidthA(0), startWidthB(0) {
    }

    void update(int diff) {
        float newRatio = horz ?
                (startHeightA + diff) / float(startHeightA + startHeightB) :
                (startWidthA + diff) / float(startWidthA + startWidthB);

        pair->setPortion(newRatio);
        pair->resized();
    }

    bool horz;
    Ref<SamplePair> pair;
    float startHeightA, startHeightB, startWidthA, startWidthB;
};


class SamplePlacer :
        public Component
    ,	public Button::Listener
    , 	public SingletonAccessor
{
public:
    explicit SamplePlacer(SingletonRepo* repo);

    void cut();
    void mouseDown(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    void buttonClicked (Button* button) override;
    void paint(Graphics& g) override;
    void resized() override;

    Image folderImage;
private:
    friend class SamplePair;

    bool horzCut;

    IconButton horzButton, vertButton;

    SamplePair pair;
    Point<float> xy;
};

class SamplePlacerPanel : public Component, public SingletonAccessor
{
public:
    explicit SamplePlacerPanel(SingletonRepo* repo);
    void resized() override;

private:
    SamplePlacer samplePlacer;
};
