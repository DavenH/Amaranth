#pragma once
#include <set>
#include "Mesh.h"
#include "../Obj/MorphPosition.h"

using std::set;

class EnvelopeMesh :
        public Mesh {
public:
    EnvelopeMesh(const String& name);
    float getPositionOfCubeAt(VertCube* line, MorphPosition pos);
    void setSustainToRightmost();
    void setSustainToLast();
    void destroy();

    void writeXML(XmlElement* element) const;
    bool readXML(const XmlElement* element);

    set<VertCube*> loopCubes;
    set<VertCube*> sustainCubes;

private:
    void clampSharpness(VertCube* line);
};
