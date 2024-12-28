#pragma once

#include "Panel3D.h"
#include "OpenGLBase.h"
#include "PanelOwner.h"
#include "Texture.h"
#include "../../App/SingletonAccessor.h"

using std::vector;

class CommonGL;
class Interactor3D;

class OpenGLPanel3D :
        public OpenGLBase
    ,   public PanelOwner<Panel3D>
    ,   public Panel3D::Renderer
    ,   public OpenGLRenderer
    ,   public Component
    ,   public virtual SingletonAccessor {
public:
    OpenGLPanel3D(SingletonRepo* repo, Panel3D* panel3D, Panel3D::DataRetriever* retriever);
    ~OpenGLPanel3D() override;

    void init() override;
    void drawSurfaceColumn(int x) override;
    void drawCurvesAndSurfaces();
    void resized() override;
    void drawCircle();
    void initRender();

    // opengl crap
    void activateContext();
    void deactivateContext();

    static void disableClientArrays();
    static void enableClientArrays();
    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;
    void textureBakeFinished() override;

    // panel3d renderer
    void clear() override;
    void activate() override { activateContext(); }
    void deactivate() override { deactivateContext(); }

    CriticalSection& getGridLock() override         { return columnLock; }
    Buffer<float> getColumnArray() override         { return dataRetriever->getColumnArray(); }
    const vector<Column>& getColumns() override     { return dataRetriever->getColumns(); }

protected:

    static const int vertsPerPolygon = 2;
    static const int bytesPerFloat = sizeof(float);

    bool usePixels;

    TextureGL backTex;
    StringArray extensions;
    CriticalSection columnLock;
    Ref<Panel3D::DataRetriever> dataRetriever;

    void populateExtensions();
    bool isSupported(const String& extension);

    JUCE_LEAK_DETECTOR(OpenGLPanel3D);
};
