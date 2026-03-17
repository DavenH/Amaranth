#pragma once

#include "Panel3D.h"
#include "GLSurfaceCache.h"
#include "OpenGLBase.h"
#include "PanelOwner.h"
#include "../../App/SingletonAccessor.h"

using std::vector;

class CommonGL;
class GLPanelRenderer;
class Interactor3D;

class OpenGLPanel3D :
        public OpenGLBase
    ,   public PanelOwner<Panel3D>
    ,   public Panel3D::ContextHelper
    ,   public OpenGLRenderer
    ,   public Component
    ,   public virtual SingletonAccessor {
public:
    OpenGLPanel3D(SingletonRepo* repo, Panel3D* panel3D, Panel3D::DataRetriever* retriever);
    ~OpenGLPanel3D() override;

    void init() override;
    void resized() override;
    void drawCircle();
    void initRender();

    // opengl crap
    void activateContext();
    void deactivateContext();

    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;

    // panel3d renderer
    void clear() override;
    void activate() override { activateContext(); }
    void deactivate() override { deactivateContext(); }

protected:

    static const int vertsPerPolygon = 2;
    static const int bytesPerFloat = sizeof(float);

    bool usePixels;

    GLSurfaceCache surfaceCache;
    StringArray extensions;
    Ref<Panel3D::DataRetriever> dataRetriever;
    std::unique_ptr<GLPanelRenderer> panelRenderer;

    void populateExtensions();
    bool isSupported(const String& extension);

    JUCE_LEAK_DETECTOR(OpenGLPanel3D);
};
