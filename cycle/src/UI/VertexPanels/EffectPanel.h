#pragma once
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
    ~EffectPanel() override = default;

    void init() override;
    void performUpdate(UpdateType updateType) override;
    bool isMeshEnabled() const;
    bool doCreateVertex() override;
    bool addNewCube(float startTime, float x, float y, float curve) override;

    float getDragMovementScale();

    /* Effect callbacks */
    virtual bool isEffectEnabled() const = 0;

protected:
    FXRasterizer localRasterizer;

private:
};
