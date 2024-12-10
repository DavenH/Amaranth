#ifndef SAMPLEPLACER_H_
#define SAMPLEPLACER_H_

#include <vector>
#include <UI/Widgets/IconButton.h>
#include "JuceHeader.h"
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

using std::vector;

class SingletonRepo;
class SamplePlacer;
class SampleDragger;

class SamplePair : public Component
{
public:
	enum { border = 1 };

	bool 			horz;
	float 			portion;
	File 			file;
	Rectangle<int> 	rect;

	Ref<SamplePlacer> placer;

	ScopedPointer<SamplePair> a;
	ScopedPointer<SamplePair> b;
	ScopedPointer<SampleDragger> dragger;

	SamplePair(SamplePlacer* placer);
	void paint(Graphics& g);
	void split(float portion, bool horz);
	void resized();
	void setPortion(float portion) { this->portion = portion; }
};

class SampleDragger : public Component
{
public:
	SampleDragger(SamplePair* pair, bool horz) :
		pair(pair),
		horz(horz),
		startHeightA(0), startHeightB(0), startWidthA(0), startWidthB(0)
	{}

	void update(int diff)
	{
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
	SamplePlacer(SingletonRepo* repo);
	virtual ~SamplePlacer();

	void cut();
	void mouseDown(const MouseEvent& e);
	void mouseMove(const MouseEvent& e);
    void buttonClicked (Button* button);
	void paint(Graphics& g);
	void resized();

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
	SamplePlacerPanel(SingletonRepo* repo);
	void resized();

private:
	SamplePlacer samplePlacer;
};


#endif
