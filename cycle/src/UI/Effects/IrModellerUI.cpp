#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Algo/FFT.h>
#include <Algo/AutoModeller.h>
#include <Binary/Gradients.h>
#include <Audio/Multisample.h>
#include <Audio/PitchedSample.h>
#include <Obj/Ref.h>
#include <UI/Widgets/CalloutUtils.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/Texture.h>
#include <UI/Panels/ZoomPanel.h>
#include <Util/Arithmetic.h>
#include <Util/Util.h>

#include "IrModellerUI.h"

#include <Util/StatusChecker.h>

#include "../Dialogs/PresetPage.h"
#include "../Panels/PlaybackPanel.h"
#include "../VertexPanels/DeformerPanel.h"
#include "../VertexPanels/Waveform3D.h"

#include "../CycleDefs.h"
#include "../../App/CycleTour.h"
#include "../../App/Dialogs.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Curve/GraphicRasterizer.h"

IrModellerUI::IrModellerUI(SingletonRepo* repo) :
		EffectPanel(repo, "IrModellerUI", true)
	,	SingletonAccessor(repo, "IrModellerUI")
	,	ParameterGroup::Worker(repo, "IrModellerUI")
	,	openWave		(4, 5, this, repo, "Load Audio Impulse (.wav)")
	,	removeWave		(3, 5, this, repo, "Unload Audio Impulse")
	,	modelWave		(5, 1, this, repo, "Model Audio Impulse")

	,	impulseMode		(3, 4, this, repo, "Impulse Mode")
	,	freqMode		(5, 5, this, repo, "Filter Mode")
	,	attkZoomIcon	(6, 2, this, repo, "Zoom to zero")
	,	zoomOutIcon		(6, 3, this, repo, "Zoom out")
	,	title			(repo, "IMPULSE MODELLER")
	,	bufSizeLabel	({}, "Buf size")
	,	isEnabled		(false)
	,	spacer2(2)
	,	spacer8(8)
{
	updateSource 			= UpdateSources::SourceIrModeller;
	layerType 				= LayerGroups::GroupIrModeller;
	doesDrawMouseHint 		= true;
	vertsAreWaveApplicable 	= true;

	zoomPanel->tendZoomToBottom	= false;
	zoomPanel->tendZoomToLeft	= false;
	zoomPanel->tendZoomToTop	= false;

	bgPaddingLeft = getRealConstant(IrModellerPadding);

	Image alum = PNGImageFormat::loadFrom(Gradients::burntalum_png, Gradients::burntalum_pngSize);
	gradient.read(alum, false, true);
	gradient.multiplyAlpha(0.4f);

	lengthSlider 	= new HSlider(repo, "len", "Impulse Response Length");
	gainSlider 		= new HSlider(repo, "gain", "Postamp");
	hpSlider		= new HSlider(repo, "hp", "Highpass filter");
	hpSlider->setValue(0, dontSendNotification);

	Slider* sliders[] 	= { lengthSlider, gainSlider, hpSlider };

	using namespace Ops;
	StringFunction delayStr(StringFunction(1).max(0.15).pow(2.0).mul(4.0));
	StringFunction decibel30(StringFunction(0).mul(2.0).sub(1.).mul(30.0));

	lengthSlider->setStringFunctions(StringFunction(0).mul(7.f).add(7.f),
	                                 StringFunction(0).mul(7.f).add(7.f).flr().pow(2.f).withPostString(" samples"));

	// StringFunction::MulAddFlrPow).setArgs(7.f, 7.f, 2.f).withPostString(" samples"));

	gainSlider->setStringFunctions(decibel30, decibel30.withPostString(" dB"));

	for(auto& slider : sliders)	{
		paramGroup->addSlider(slider);
	}

	this->interactor = this;
	lengthSlider->setValue(IrModeller::calcKnobValue(256), dontSendNotification);

	paramGroup->listenToKnobs();
	createNameImage("IR Modeller");
}

void IrModellerUI::init() {
	irModeller = &getObj(SynthAudioSource).getIrModeller();

	selector = std::make_unique<MeshSelector<Mesh>>(repo.get(), this, String("ir"), true, false, true);

	Component* wavArr[] = { &openWave, 		&removeWave, &modelWave };
	Component* zoomArr[]= { &attkZoomIcon, 	&zoomOutIcon };

	panelControls = std::make_unique<PanelControls>(repo, this, this, nullptr, "Impulse Modeller");
	panelControls->addEnablementIcon();

	CalloutUtils::addRetractableCallout(zoomCO, zoomPO, repo, 6, 2, zoomArr, numElementsInArray(zoomArr), panelControls.get(), false);
	CalloutUtils::addRetractableCallout(waveCO, wavePO, repo, 0, 5, wavArr, numElementsInArray(wavArr), panelControls.get(), false);

	panelControls->addLeftItem(zoomCO.get(), true);
	panelControls->addRightItem(waveCO.get(), true);
	panelControls->addSlider(lengthSlider);
	panelControls->addSlider(gainSlider);
	panelControls->addSlider(hpSlider);
	panelControls->addMeshSelector(selector.get());

	rasterizer->setMesh(getMesh());
	irModeller->setMesh(rasterizer->getMesh());

	bufSizeLabel.setFont(*getObj(MiscGraphics).getSilkscreen());
	bufSizeLabel.setColour(Label::textColourId, Colour::greyLevel(0.55f));
	bufSizeLabel.setEditable(false, false);
	bufSizeLabel.setBorderSize(BorderSize<int>());
}

void IrModellerUI::initControls() {
}

void IrModellerUI::setCurrentMesh(Mesh* mesh) {
	panelControls->enableCurrent.setHighlit(isEnabled);

	rasterizer->cleanUp();
	rasterizer->setMesh(mesh);
	irModeller->setMesh(mesh);

	getObj(EditWatcher).setHaveEditedWithoutUndo(true);
	getObj(MeshLibrary).setCurrentMesh(layerType, mesh);

	clearSelectedAndCurrent();
	triggerRefreshUpdate();
}

void IrModellerUI::previewMesh(Mesh* mesh) {
	ScopedLock sl(renderLock);

	setMeshAndUpdate(mesh);
}

void IrModellerUI::previewMeshEnded(Mesh* oldMesh) {
	ScopedLock sl(renderLock);

	setMeshAndUpdate(oldMesh);
}

void IrModellerUI::doGlobalUIUpdate(bool force) {
	Interactor::doGlobalUIUpdate(force);
}

void IrModellerUI::reduceDetail() {
	Interactor::reduceDetail();
}

void IrModellerUI::restoreDetail() {
	Interactor::restoreDetail();
}

bool IrModellerUI::paramTriggersAggregateUpdate(int knobIndex) {
	return !paramGroup->updatingAllSliders;
}

void IrModellerUI::setMeshAndUpdate(Mesh* mesh, bool doRepaint) {
	rasterizer->cleanUp();
	rasterizer->setMesh(mesh);
	irModeller->setMesh(mesh);
	irModeller->setPendingAction(IrModeller::rasterize);

	if(doRepaint) {
		repaint();
	}
}

Mesh* IrModellerUI::getCurrentMesh() {
	return Interactor::getMesh();
}

void IrModellerUI::preDraw() {
	Buffer<float> mags 		= irModeller->getMagnitudes();
	Buffer<float> impulse 	= irModeller->getGraphicImpulse();

	if(mags.size() == 0) {
		return;
	}

	vector<Color>& grd = gradient.getColours();

	ScopedAlloc<Ipp32f> reducedMags, accum;

	if (mags.size() > 512) {
		accum.resize(512);
		reducedMags.resize(512);
		int ratio = mags.size() / reducedMags.size();

		Buffer a = accum;
		Buffer reduced = reducedMags;
		a.zero();

		for (int i = 0; i < ratio; ++i) {
			reduced.downsampleFrom(mags, -1, i);
			a.add(reduced);
		}

		a.mul(1 / float(ratio));

		mags = a;
	}

	ScopedAlloc<Ipp16s> indices(mags.size());

	status(ippsConvert_32f16s_Sfs(mags, indices, mags.size(), ippRndZero, -9));

	int left 		= 0;
	int bottom 		= 0;
	int top 		= getHeight();
//	int increment 	= jmax(1, int(mags.size() / height));
	int innerLeft 	= sx(getConstant(IrModellerPadding));

	gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
	gfx->fillRect(left, top, innerLeft, bottom, false);
	gfx->setCurrentColour(0.2f, 0.2f, 0.2f);
	gfx->disableSmoothing();
	gfx->drawLine(innerLeft, top, innerLeft, bottom, false);

	int sizeY = mags.size(); // / increment;
	yBuffer.ensureSize(sizeY);

	Buffer<float> yScale = yBuffer.withSize(sizeY);

	yScale.ramp(0.f, 1.f / float(yScale.size() - 1));
	Arithmetic::applyLogMapping(yScale, getConstant(LogTension));
	jassert(yScale[yScale.size() - 1] <= 1.f);

	applyNoZoomScaleY(yScale);

	float firstX = sx(getConstant(IrModellerPadding));
	float lastX  = sx(1.f);

	vector<Color> colors;

	for(int i = 0; i < yScale.size(); ++i) {
		colors.push_back(grd[indices[i]]);
	}

	gfx->drawVerticalGradient(firstX, lastX, yScale, colors);

	int sizeX = impulse.size();
	prepareBuffers(sizeX);

	impulse.copyTo(xy.y);
	Arithmetic::unpolarize(xy.y);

	xy.x.ramp(getConstant(IrModellerPadding), (1.f - getConstant(IrModellerPadding)) / (float) sizeX);

	gfx->enableSmoothing();
	gfx->setCurrentColour(0.8f, 0.4f, 0.5f, 0.4f);
	gfx->drawLineStrip(xy, true, true);
}

void IrModellerUI::postCurveDraw() {
	if (irModeller->isWavLoaded()) {
		PitchedSample& wav = irModeller->getWrapper();

		Buffer<float> channel = wav.audio.left;

		int chanSize = channel.size();
		int nextPow2 = Arithmetic::getNextPow2((float) chanSize);

		prepareBuffers(chanSize, chanSize);

		Buffer<float> alpha = cBuffer.withSize(chanSize);

		float ix = (1.f - getConstant(IrModellerPadding)) / (float) nextPow2;
		xy.x.ramp(getConstant(IrModellerPadding), ix);
		channel.copyTo(xy.y);

		prepareAlpha(xy.y, alpha, 0.3f);
		Arithmetic::unpolarize(xy.y);

		applyScaleX(xy.x);
		applyScaleY(xy.y);

		Color red(0.7f, 0.3f, 0.5f, 0.3f);
		drawCurvesFrom(xy, alpha, red, red);
	}

	int left = 0;
	int innerLeft = sx(getConstant(IrModellerPadding));
	int bottom = 0;
	int top = getHeight();

	gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
	gfx->drawRect(left, top, innerLeft, bottom, false);
}

bool IrModellerUI::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
	bool didChange = irModeller->doParamChange(knobIndex, knobValue, doFurtherUpdate);

	didChange &= doFurtherUpdate;

	if (didChange) {
		if(knobIndex == IrModeller::Length) {
			pendingScaleUpdate = true;
		}

		irModeller->setPendingAction(IrModeller::rasterize);
	}

	return didChange;
}

void IrModellerUI::updateDspSync() {
	irModeller->setMesh(rasterizer->getMesh());
	irModeller->setPendingAction(IrModeller::rasterize);
}

void IrModellerUI::buttonClicked(Button* button) {

	progressMark

	if (button == &openWave) {
		getObj(Dialogs).showOpenWaveDialog(&irModeller->getWrapper(), "impulses",
		                                   DialogActions::LoadImpulse, Dialogs::LoadIRWave);
	} else if (button == &modelWave) {
		getObj(Dialogs).showOpenWaveDialog(&irModeller->getWrapper(), "impulses",
		                                   DialogActions::LoadImpulse, Dialogs::ModelIRWave);
	} else if (button == &removeWave) {
		irModeller->setPendingAction(IrModeller::unloadWav);
	}

	if (button == &attkZoomIcon) {
		panel->getZoomPanel()->zoomToAttack();
	}

	else if (button == &zoomOutIcon) {
		panel->getZoomPanel()->zoomToFull();
	} else if (button == &panelControls->enableCurrent) {
		isEnabled ^= true;
		panelControls->enableCurrent.setHighlit(isEnabled);

		if (isEnabled) {
			irModeller->setPendingAction(IrModeller::rasterize);
		}

		forceNextUIUpdate = true;
		triggerRefreshUpdate();
	} else if (button == &impulseMode || button == &freqMode) {
		if (button == &impulseMode) {
		}
	}

	if(button != &panelControls->enableCurrent) {
		repaint();
	}
}

void IrModellerUI::panelResized() {
	//	progressMark

	Panel::panelResized();
}

String IrModellerUI::getKnobName(int index) const {
	switch (index) {
		case IrModeller::Length: 	return "Size";
		case IrModeller::Postamp: 	return "Post";
		case IrModeller::Highpass: 	return "HighPass";
		default:
			break;
	}

	return {};
}

void IrModellerUI::showCoordinates() {
	float x = (state.currentMouse.x - getConstant(IrModellerPadding)) / (1.f - getConstant(IrModellerPadding));
	int sampleNum = IrModeller::calcLength(paramGroup->getKnobValue(IrModeller::Length)) * x;

	float y = (state.currentMouse.y - 0.5f) * 2.f;
	String dbString = (y == 0) ? String(L"\t-\u221edB") :
								 String(NumberUtils::toDecibels(y), 2) + String(" dB");

	String coords = String("smp: ") + String(sampleNum) + String(", ") + dbString;
	showMsg(coords);
}

void IrModellerUI::writeXML(XmlElement* registryElem) const {
	auto* tubeElem = new XmlElement(panelName);

	paramGroup->writeKnobXML(tubeElem);
	tubeElem->setAttribute("enabled", isEffectEnabled());
	tubeElem->setAttribute("waveLoaded", irModeller->isWavLoaded());
	tubeElem->setAttribute("wavePath", waveImpulsePath);

	registryElem->addChildElement(tubeElem);
}

bool IrModellerUI::readXML(const XmlElement* registryElem) {
//	ScopedLock sl(renderLock);

	XmlElement* tubeElem = registryElem->getChildByName(panelName);

	if (tubeElem) {
		paramGroup->setKnobValue(IrModeller::Highpass, 0, false, false);
		isEnabled = tubeElem->getBoolAttribute("enabled");

		paramGroup->readKnobXML(tubeElem);

		bool waveLoaded = tubeElem->getBoolAttribute("waveLoaded", false);
		if (waveLoaded) {
			String path = tubeElem->getStringAttribute("wavePath");

			if (File(path).existsAsFile()) {
				int result = irModeller->getWrapper().load(path);
				if (result >= 0) {
					irModeller->doPostWaveLoad();
				} else {
					std::cout << "Failed to load impulse response file: " << path << "\n";
				}
			} else {
				std::cout << "Failed to load impulse response file: " << path << "\n";
			}
		}
	}

	panelControls->enableCurrent.setHighlit(isEnabled);

	return true;
}

bool IrModellerUI::shouldTriggerGlobalUpdate(Slider* slider) {
	return isEffectEnabled();
}

void IrModellerUI::setEffectEnabled(bool is) {
	if (Util::assignAndWereDifferent(isEnabled, is)) {
		panelControls->enableCurrent.setHighlit(isEnabled);
		triggerRefreshUpdate();
	}
}

void IrModellerUI::doLocalUIUpdate() {
	repaint();
}

void IrModellerUI::enterClientLock() {
	getObj(SynthAudioSource).getLock().enter();
	renderLock.enter();
}

void IrModellerUI::exitClientLock() {
	renderLock.exit();
	getObj(SynthAudioSource).getLock().exit();
}

void IrModellerUI::modelLoadedWave()
{
	PitchedSample& wav = irModeller->getWrapper();

	if(wav.audio.size() < 64) {
		return;
	}

	irModeller->trimWave();

	Buffer<float> channel = wav.audio.left;

	int size = channel.size();
	channel.mul(0.7f);

	int greaterPow2 = Arithmetic::getNextPow2((float) size);
	float scaleRatio = size / float(greaterPow2);
	CriticalSection& audioLock = getObj(SynthAudioSource).getLock();
	Mesh* mesh = getMesh();

	float padding = getRealConstant(IrModellerPadding);
	AutoModeller modeller;
	modeller.modelToInteractor(channel, this, false, padding, 0.1f);

	{
		ScopedLock sl(audioLock);
		ScopedLock sl2(vertexLock);

		mesh->getVerts().push_back(new Vertex(padding - 0.0001f, 0.5f));
		mesh->getVerts().push_back(new Vertex(padding - 0.01f, 	0.5f));

		for (auto& vert: mesh->getVerts()) {
			float& val = vert->values[dims.x];
			val = (val - getRealConstant(IrModellerPadding)) * scaleRatio + getRealConstant(IrModellerPadding);
		}
	}

	// resizes buffers
	paramGroup->setKnobValue(IrModeller::Length, irModeller->calcKnobValue(greaterPow2), false);
	clearSelectedAndCurrent();

	irModeller->audioFileModelled();

	flag(DidMeshChange) = true;
	refresh();	// xxx need both??

	getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}

void IrModellerUI::loadWaveFile() {
	irModeller->doPostWaveLoad();

	getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}

void IrModellerUI::deconvolve()
{
	PitchedSample* wav = getObj(Multisample).getCurrentSample();

	if(wav == nullptr) {
		return;
	}

	float position = getObj(PlaybackPanel).getProgress();

	if(wav->periods.size() < 2) {
		return;
	}

	Buffer<float> waveSource;
	Buffer<float> wavBuffer = wav->audio.left;

	int cycSize = 512;

	float invSize = 1 / float(wav->size());

	int i = 1;
	for (; i < (int) wav->periods.size(); ++i) {
		if (wav->periods[i].sampleOffset * invSize > position) {
			cycSize = wav->periods[i].sampleOffset - wav->periods[i - 1].sampleOffset;
			waveSource = wavBuffer.section(wav->periods[i - 1].sampleOffset, cycSize + cycSize / 4);

			break;
		}
	}

	if (waveSource.empty()) {
		showImportant("Audio file is too short");
		return;
	}

	if (waveSource.max() < 0.005) {
		showImportant("Audio file is silent at this position");
		return;
	}

	int pow2Size = NumberUtils::nextPower2((int) cycSize);

	int quarter = cycSize / 4;
	ScopedAlloc<float> fadeMem(quarter * 6 + cycSize);

	Buffer<float> fadeInUp 		= fadeMem.place(quarter);
	Buffer<float> fadeInDown 	= fadeMem.place(quarter);
	Buffer<float> fadeOutUp 	= fadeMem.place(quarter);
	Buffer<float> fadeOutDown 	= fadeMem.place(quarter);
	Buffer<float> quarterBuf 	= fadeMem.place(quarter);
	Buffer<float> fadedCyc 		= fadeMem.place(cycSize + quarter);

	fadeOutUp	.ramp().sqr();
	fadeOutDown	.subCRev(1.f, fadeOutUp);
	fadeInUp	.flip(fadeOutDown);
	fadeInDown	.subCRev(1.f, fadeInUp);

	waveSource	.copyTo(fadedCyc);
	fadedCyc	.mul(fadeInUp);
	quarterBuf	.mul(fadedCyc + cycSize, fadeInDown);
	fadedCyc	.add(quarterBuf);

	int hSize = pow2Size / 2;

	ScopedAlloc<Ipp32fc> freqBuffer(hSize);
	ScopedAlloc<Ipp32f> timeBuffer(pow2Size * 4);

	Buffer<float> 	paddedWav 	= timeBuffer.place(pow2Size);
	Buffer<float> 	synthRast 	= timeBuffer.place(pow2Size);
	Buffer<float>	impSignal	= timeBuffer.place(pow2Size);
	Buffer<float>	paddedMag	= timeBuffer.place(hSize);
	Buffer<float>	paddedPhs	= timeBuffer.place(hSize);
	Buffer<Ipp32fc> impComplex  = freqBuffer.withSize(hSize);

	paddedWav.zero();
	fadedCyc.withSize(cycSize).copyTo(paddedWav);

	Transform wavFFT;
	wavFFT.allocate(pow2Size);
	wavFFT.forward(paddedWav);
	Buffer<Ipp32fc> waveComplex = wavFFT.getComplex() + 1;	// +1 removes dc offset bytes

	synthRast.ramp(1, -2 / float(pow2Size));

	/*
	GraphicTimeRasterizer& timeRast = getObj(GraphicTimeRasterizer);
	MeshRasterizer::RenderState rendState;
	MeshRasterizer::ScopedRenderState scopeState(&timeRast, &rendState);

	timeRast.yellow = position;
	timeRast.setLowresCurves(false);
	timeRast.setCalcDepthDimensions(false);
	timeRast.setScalingMode(MeshRasterizer::Bipolar);
	timeRast.calcCrossPoints();

	if(! timeRast.isSampleable())
	{
		showMsg("A waveshape must be defined");
		return;
	}

	timeRast.samplePerfectly(1 / float(tSize), synthRast, 0);
	*/

	Transform fft;
	fft.allocate(pow2Size);
	fft.forward(synthRast);

	Buffer<Ipp32fc> synthComplex = fft.getComplex();

	jassert(synthComplex.size() == waveComplex.size() + 1);

	ippsThreshold_32fc_I(synthComplex, synthComplex.size(), 1e-6f, ippCmpLess);
	ippsDiv_32fc_A11(waveComplex, synthComplex, impComplex, waveComplex.size());

	impComplex.copyTo(fft.getComplex());
	fft.inverse(impSignal);

	impSignal.offset(cycSize).zero();
	impSignal.mul(0.5 / impSignal.max());

	{
		ScopedLock sl(vertexLock);

		clearSelectedAndCurrent();
	}

	AutoModeller modeller;
	modeller.modelToInteractor(impSignal, this, false, getConstant(IrModellerPadding), 0.1f);

	float scaleFactor = pow2Size / float(cycSize);
	Mesh* mesh = getMesh();

	for(auto& vert : mesh->getVerts()) {
		float& phase = vert->values[Vertex::Phase];
		phase = (phase - getConstant(IrModellerPadding)) * scaleFactor + getConstant(IrModellerPadding);
	}

	paramGroup->setKnobValue(IrModeller::Length, irModeller->calcKnobValue(pow2Size), false);
	irModeller->doPostDeconvolve(pow2Size);

	flag(DidMeshChange) = true;
	refresh();

	getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}

void IrModellerUI::doZoomAction(int action) {
	ZoomPanel* zoomPanel = getZoomPanel();

	if (action == ZoomPanel::ZoomToAttack) {
		zoomPanel->rect.x = getConstant(IrModellerPadding);
		zoomPanel->rect.w *= 0.2f;
	} else if (action == ZoomPanel::ZoomToFull) {
		zoomPanel->rect.x = 0;
		zoomPanel->rect.w = 1.f;
	}

	zoomPanel->panelZoomChanged(false);
}

void IrModellerUI::doubleMesh() {
	getCurrentMesh()->twin(getConstant(IrModellerPadding), 0);
	postUpdateMessage();
}

Component* IrModellerUI::getComponent(int which) {
	switch (which) {
		case CycleTour::TargImpLength: 		return lengthSlider;
		case CycleTour::TargImpGain:		return gainSlider;

		case CycleTour::TargImpHP:			return hpSlider;
		case CycleTour::TargImpZoom:		return zoomCO.get();

		case CycleTour::TargImpLoadWav:		return &openWave;
		case CycleTour::TargImpUnloadWav:	return &removeWave;
		case CycleTour::TargImpModelWav:	return &modelWave;
		default:
			break;
	}

	return nullptr;
}

void IrModellerUI::createScales() {
	vector<Rectangle<float> > newScales;

	auto& mg = getObj(MiscGraphics);
	int fontScale 	 = getSetting(PointSizeScale);
	Font& font 		 = *mg.getAppropriateFont(fontScale);
	float alpha 	 = fontScale == ScaleSizes::ScaleSmall ? 0.5f : 0.65f;

	scalesImage = Image(Image::ARGB, 256, 16, true);
	Graphics g(scalesImage);
	g.setFont(font);

	int position = 0;
	int size = IrModeller::calcLength(paramGroup->getKnobValue(IrModeller::Length));

	for (float vertMajorLine: vertMajorLines) {
		float x = (vertMajorLine - getConstant(IrModellerPadding)) / (1.f - getConstant(IrModellerPadding));
		int samples = roundToInt(size * x);

		String text;
		if(samples > 800) {
			text = String(roundToInt(samples * 0.001f)) + "k";
		} else {
			text = String(samples);
		}

		int width = font.getStringWidth(text) + 1;
		MiscGraphics::drawShadowedText(g, text, position + 1, font.getHeight(), font, alpha);
		newScales.push_back(Rectangle<float>(position, 0, width, font.getHeight()));

		position += width + 2;
	}

	ScopedLock sl(renderLock);
	scales = newScales;
}

void IrModellerUI::drawScales() {
	float fontOffset = 7 + jmin(11.f, getSetting(PointSizeScale) * 4.f);
	gfx->setCurrentColour(Color(1));

	for (int i = 0; i < (int) scales.size(); ++i) {
		Rectangle<float>& rect = scales[i];
		float x = vertMajorLines[i];

		scalesTex->rect = Rectangle<float>(sx(x), getHeight() - fontOffset, rect.getWidth(), rect.getHeight());
		gfx->drawSubTexture(scalesTex, rect);
	}
}
