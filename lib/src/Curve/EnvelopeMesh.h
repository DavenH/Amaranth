#pragma once

#include <set>
#include "Mesh.h"
#include "../Obj/MorphPosition.h"

using std::set;

class EnvelopeMesh :
        public Mesh {
public:
    explicit EnvelopeMesh(const String& name);
    float getPositionOfCubeAt(VertCube* line, MorphPosition pos);
    void setSustainToRightmost();
    void setSustainToLast();
    void destroy() override;

    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    set<VertCube*> loopCubes;
    set<VertCube*> sustainCubes;

private:
    void clampSharpness(VertCube* line);
};
