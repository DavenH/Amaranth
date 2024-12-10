#include "VersionConsiliator.h"

void VersionConsiliator::transform(XmlElement* element) {
    XmlElement* meshesElement = element->getChildByName("AllMeshes");

    if (meshesElement != nullptr) {
//		names[TimeLayer] 		= "Time";
//		names[FreqLayer] 		= "Freq";
//		names[PhaseLayer] 		= "Phase";
//		names[GuideLayer] 		= "Guide";
//		names[EnvLayer] 		= "Env";
//		names[ScratchLayer]		= "Scratch";
//		names[WaveShaperLayer]  = "WaveShaper";
//		names[TubeModelLayer] 	= "TubeModel";
//		names[OtherLayer] 		= "Other";

		element->removeChildElement(meshesElement, false);
	}
}

/*
	<AllMeshes>
	 	<TimeLayer TimeLayerSize=4>

	 	</TimeLayer>

	 	<FreqLayer FreqLayerSize=4>

	 	</FreqLayer>

	 	<PhaseLayer PhaseLayerSize=4>

	 	</PhaseLayer>
	 	...
	 	<EnvLayer>

	 	</EnvLayer>
	</AllMeshes>




*/

void read(XmlElement* element) {
    // src
    String names[] = {
            "Time", "Freq", "Phase", "Guide",
            "Env", "Scratch", "WaveShaper", "TubeModel", "Other"
    };

	// src
	String envNames[] = {
			"Volume", "Pitch", "Speed", "WavePitch"
	};

	// src
	XmlElement* allMeshesElem = element->getChildByName("AllMeshes");	// 2

	// src
	if(allMeshesElem == nullptr)
		return;

	// src
	XmlElement* meshLibraryElem = new XmlElement("MeshLibrary");

    // src
    for (int i = 0; i < numElementsInArray(names); ++i) {
        // src
        String layerName = names[i] + "Layer";
        XmlElement* srcLayerElem 	= allMeshesElem->getChildByName(layerName);

        if (srcLayerElem == nullptr) {
            // add empty mesh of correct type to
            continue;
		}

		// src
		int layerSize = srcLayerElem->getIntAttribute(layerName + "Size");

		// dst
		XmlElement* dstGroupElem = new XmlElement("Group");

		if(i == 4) // env
			layerSize = jmin(layerSize, (int) 4);

		// dst
        dstGroupElem->setAttribute("mesh-type", (int) i);

        for (int j = 0; j < layerSize; ++j) {
            // dst
            XmlElement* dstLayerElem = new XmlElement("Layer");

			String meshName;

			// src
            if (i == 4) {
                String envName = j < numElementsInArray(envNames) ? envNames[j] : String::empty;
                meshName = names[i] + envName + "Mesh";
            } else {
                meshName = names[i] + "Mesh" + String(j);
            }

			// src
			XmlElement* currentMeshElem = srcLayerElem->getChildByName(meshName);

			// can I really do this? Or do I need to deep copy?
			// I suspect that these are not reference counted -
			// so could result in double deletion.
			dstLayerElem = currentMeshElem;

			Mesh* mesh = createMesh(i);

			mesh->readXML(currentMeshElem);
			mesh->writeXML(dstLayerElem);

//			layerElem->setAttribute("active", 		active);
//			layerElem->setAttribute("fine-tune", 	fineTune);
//			layerElem->setAttribute("gain", 		gain);
//			layerElem->setAttribute("pan", 			pan);
//			layerElem->setAttribute("range", 		range);
//			layerElem->setAttribute("mode", 		mode);
//			layerElem->setAttribute("scratch-chan", scratchChan);

			// dst
			dstGroupElem->addChildElement(dstLayerElem);
		}

		meshLibraryElem->addChildElement(dstGroupElem);
	}

	element->replaceChildElement(allMeshesElem, meshLibraryElem);


	return true;
}


void read2() {
    for (int i = 0; i < (int) layers.size(); ++i) {
        XmlElement* layersElem = meshes->getChildByName(names[i] + "Layer");
        Array < Mesh * > &layer = layers.getReference(i);

		int layerSize = layersElem->getIntAttribute(names[i] + "LayerSize");

		if(types[i] == EnvLayer)
			layerSize = jmin(layerSize, (int) numEnvelopesTotal);

		layer.resize(layerSize);

		for(int j = 0; j < layerSize; ++j)
		{
			Mesh* mesh = createMeshForType(types[i], j);
			layer.set(j, mesh);

//			dout << "created mesh called " << mesh->getName() << "\n";

            XmlElement* currentMeshElem = layersElem->getChildByName(mesh->getName());

            if (currentMeshElem != nullptr) {
                if (!layer[j]->readXML(currentMeshElem)) {
                    dout << "error reading " << mesh->getName() << "\n";
                    mesh->destroy();
                    delete mesh;

					layer.set(j, nullptr);
					continue;
				}
			}
		}
	}

	validateDeformers();

	// want to do this after all settings have been read from xml
	pushMeshesToRasterizers();
}


void VersionConsiliator::fixScratchMeshes() {
    EnvelopeMesh* scratchMesh = getScratchMesh(0);
    jassert(scratchMesh);

//	dout << "Scratch mesh present: " << (scratchMesh ? "yes" : "no") << "\n";

    if (getObject(Preset).getVersionValue() < editionSplit(1.1, 1.0)) {
//		dout << "Preset is < version 1.1\n";

        EnvelopeMesh* envLayerScratch = getEnvMesh(ScratchMesh);

        if (envLayerScratch->verts.size() > 0) {
            scratchMesh->destroy();

			scratchMesh->verts 			= envLayerScratch->verts;
			scratchMesh->lines 			= envLayerScratch->lines;
			scratchMesh->sustainLines 	= envLayerScratch->sustainLines;
			scratchMesh->loopLines 		= envLayerScratch->loopLines;
			scratchMesh->active 		= envLayerScratch->active;

			delete envLayerScratch;
			layers.getReference(EnvLayer).set(ScratchMesh, nullptr);
		}
	}
}
