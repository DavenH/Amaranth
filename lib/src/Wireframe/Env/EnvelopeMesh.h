#pragma once

#include <set>
#include "../Mesh.h"
#include "../../Wireframe/Interpolator/Trilinear/MorphPosition.h"

using std::set;

class EnvelopeMesh :
        public Mesh {
public:
    explicit EnvelopeMesh(const String& name);
    float getPositionOfCubeAt(TrilinearCube* line, MorphPosition pos);
    void setSustainToRightmost();
    void setSustainToLast();
    void destroy() override;

    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    set<TrilinearCube*> loopCubes;
    set<TrilinearCube*> sustainCubes;

private:
    void clampSharpness(TrilinearCube* line);
};
