#include <algorithm>
#include "FXRasterizer.h"

namespace {
    String describeFxMesh(Mesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        return "mesh=" + String::toHexString((pointer_sized_int) mesh)
            + " verts=" + String(mesh->getNumVerts())
            + " cubes=" + String(mesh->getNumCubes());
    }

    String describeFxIntercepts(const vector<Intercept>& icpts) {
        if (icpts.empty()) {
            return "icpts=0";
        }

        float minX = icpts.front().x;
        float maxX = icpts.front().x;
        float minY = icpts.front().y;
        float maxY = icpts.front().y;
        String parts;

        for (int i = 0; i < (int) icpts.size(); ++i) {
            const Intercept& icpt = icpts[i];
            minX = jmin(minX, icpt.x);
            maxX = jmax(maxX, icpt.x);
            minY = jmin(minY, icpt.y);
            maxY = jmax(maxY, icpt.y);

            if (i < 6) {
                if (parts.isNotEmpty()) {
                    parts += ", ";
                }

                parts += "#" + String(i)
                    + "(" + String(icpt.x, 4)
                    + "," + String(icpt.y, 4)
                    + " shp=" + String(icpt.shp, 4)
                    + ")";
            }
        }

        return "icpts=" + String((int) icpts.size())
            + " x=[" + String(minX, 4) + "," + String(maxX, 4) + "]"
            + " y=[" + String(minY, 4) + "," + String(maxY, 4) + "]"
            + " first={" + parts + "}";
    }
}

FXRasterizer::FXRasterizer(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name),
        MeshRasterizer(name) {
    cyclic = false;
    calcDepthDims = false;

    dims.x = Vertex::Phase;
    dims.y = Vertex::Amp;
}

void FXRasterizer::calcCrossPoints() {
    if (mesh == nullptr || mesh->getNumVerts() == 0) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup empty " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    DBG(MeshRasterizer::getName() + "::calcCrossPoints begin " + describeFxMesh(mesh));

    icpts.clear();
    /*
        mesh pointer is junk at this point.

        We need to ensure that when the meshlibrary creates a mesh it gets populated everywhere that needs it.
        What if we have listeners to a group? Gets notified whenever we load a mesh.

        MeshSelectionClient kind of does this. It's a set of callbacks around mesh assignment, if not lifecycle.


        FXRasterizer::calcCrossPoints() FXRasterizer.cpp:36
        MeshRasterizer::performUpdate(UpdateType) MeshRasterizer.cpp:1187
        IrModeller::rasterizeImpulse(Buffer<…>, FXRasterizer &, bool) IrModeller.cpp:176
        IrModeller::rasterizeGraphicImpulse() IrModeller.cpp:139
        IrModeller::setImpulseLength(IrModeller::ConvState &, int) IrModeller.cpp:332
        IrModeller::setGraphicImpulseLength(int) IrModeller.cpp:341
        IrModeller::setPendingAction(IrModeller::PendingUpdate, int) IrModeller.cpp:350
        IrModeller::doParamChange(int, double, bool) IrModeller.cpp:483
        IrModellerUI::updateDsp(int, double, bool) IrModellerUI.cpp:325
        ParameterGroup::setKnobValue(int, double, bool, bool) ParameterGroup.cpp:75
        ParameterGroup::readKnobJSON(const juce::var &) ParameterGroup.cpp:153
        IrModellerUI::readJSON(const juce::var &) IrModellerUI.cpp:477
        EffectGuiRegistry::readJSON(const juce::var &) EffectGuiRegistry.cpp:73
        Document::applyJsonRoot(const juce::var &) Document.cpp:112
        Document::open(juce::InputStream *) Document.cpp:221
        Document::open(const juce::String &) Document.cpp:130
        FileManager::openCurrentPreset() FileManager.cpp:137
        FileManager::openPreset(const juce::File &) FileManager.cpp:102
        FileManager::openFactoryPreset(const juce::String &) FileManager.cpp:94
        FileManager::openDefaultPreset() FileManager.cpp:312
        MainAppWindow::openFile(const juce::String &) MainAppWindow.cpp:37
     */
    for(auto vert : mesh->getVerts()) {
        float* values = vert->values;
        Intercept icpt(values[dims.x], values[dims.y], 0, values[Vertex::Curve]);

        if(scalingType) {
            icpt.y = 2.f * icpt.y - 1.f;
        }

        // time and phase are x and y in this context
        icpts.push_back(icpt);
    }

    if (icpts.empty()) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup no-intercepts " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    std::sort(icpts.begin(), icpts.end());
    restrictIntercepts(icpts);

    DBG(MeshRasterizer::getName() + "::calcCrossPoints intercepts " + describeFxIntercepts(icpts));

    curves.clear();

    if (icpts.size() < 2) {
        DBG(MeshRasterizer::getName() + "::calcCrossPoints cleanup too-few-intercepts count=" + String((int) icpts.size())
            + " " + describeFxMesh(mesh));
        cleanUp();
        return;
    }

    padIcpts(icpts, curves);
    updateCurves();

    unsampleable = false;
    DBG(MeshRasterizer::getName() + "::calcCrossPoints ready icpts=" + String((int) icpts.size())
        + " curves=" + String((int) curves.size())
        + " waveX=" + String(waveX.size())
        + " waveY=" + String(waveY.size())
        + " " + describeFxMesh(mesh));
}

void FXRasterizer::padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) {
    paddingSize = 1;
    int end = icpts.size() - 1;

    Intercept front1(-1.0f, icpts[0].y);
    Intercept front2(-0.5f, icpts[0].y);

    Intercept back1(1.5f, icpts[end].y);
    Intercept back2(2.0f, icpts[end].y);

    for(auto& curve : curves) {
        curve.destruct();
    }

    curves.clear();
    curves.reserve(icpts.size() + 2);
    curves.emplace_back(front1, front2, icpts[0]);
    curves.emplace_back(front2, icpts[0], icpts[1]);

    for(int i = 0; i < (int) icpts.size() - 2; ++i) {
        curves.emplace_back(icpts[i], icpts[i + 1], icpts[i + 2]);
    }

    curves.emplace_back(icpts[end - 1], icpts[end], back1);
    curves.emplace_back(icpts[end], back1, back2);

    for(auto& curve : curves) {
        curve.construct();
    }
}

void FXRasterizer::setMesh(Mesh* newMesh) {
    DBG(MeshRasterizer::getName() + "::setMesh " + describeFxMesh(newMesh));
    mesh = newMesh;
}

void FXRasterizer::cleanUp() {
    waveX.nullify();
    waveY.nullify();

    curves.clear();
    icpts.clear();
    unsampleable = true;

    DBG(MeshRasterizer::getName() + "::cleanUp");
}

int FXRasterizer::getNumDims() {
    return 1;
}

bool FXRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumVerts() > 1;
}
