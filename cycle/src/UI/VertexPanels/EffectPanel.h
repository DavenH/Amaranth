#ifndef _effectvertexpanel_h
#define _effectvertexpanel_h

#include <algorithm>
#include <Curve/FXRasterizer.h>
#include <UI/Panels/OpenGLPanel.h>
#include <UI/Panels/Panel2D.h>
#include <Inter/Interactor2D.h>

class EffectPanel :
		public Panel2D
	,	public Interactor2D
{
public:
	EffectPanel(SingletonRepo* repo, const String& name, bool haveVertZoom);
	virtual ~EffectPanel();

	void performUpdate(int updateType);
	bool isMeshEnabled() const;
	bool doCreateVertex();
	bool addNewCube(float startTime, float x, float y, float curve = 0);

	float getDragMovementScale();

	/* Effect callbacks */
	virtual bool isEffectEnabled() const = 0;

protected:
	FXRasterizer localRasterizer;

private:
};

#endif
