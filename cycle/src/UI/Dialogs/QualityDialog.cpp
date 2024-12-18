#include <App/SingletonRepo.h>
#include <App/EditWatcher.h>
#include "QualityDialog.h"
#include <App/Settings.h>
#include <Util/Util.h>
#include "../../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../../Util/CycleEnums.h"
#include "Algo/Resampling.h"

QualityDialog::QualityDialog(SingletonRepo* repo) :
        SingletonAccessor(repo, "QualityDialog")
    ,	rltmTitle("", 		"Realtime")
    ,	rendTitle("", 		"Render")
    ,	rltmLabel("", 		"Oversample factor for grains")
    ,	rltmHqLabel("", 	"Resampling algorithm")
    ,	paramSmoothLbl("", 	"Modulation Parameter Smoothing")
{
    rltmOvsp	.addItem("None", 	OversampRltm1x	);
    rltmOvsp	.addItem("2x", 		OversampRltm2x	);
    rltmOvsp	.addItem("4x", 		OversampRltm4x	);
    rltmOvsp	.addItem("16x", 	OversampRltm16x	);

    rendOvsp	.addItem("None", 	OversampRend1x	);
    rendOvsp	.addItem("2x", 		OversampRend2x	);
    rendOvsp	.addItem("4x", 		OversampRend4x	);
    rendOvsp	.addItem("16x", 	OversampRend16x	);

    ctrlFreqCmbo.addItem("40Hz", 	ControlFreq1024	);
    ctrlFreqCmbo.addItem("180Hz", 	ControlFreq256	);
    ctrlFreqCmbo.addItem("500Hz", 	ControlFreq64	);
    ctrlFreqCmbo.addItem("3000Hz", 	ControlFreq16	);

    rltmAlgoCmbo.addItem("Linear", 	ResampAlgoRltmLinear );
    rltmAlgoCmbo.addItem("Hermite", ResampAlgoRltmHermite);
//	rltmAlgoCmbo.addItem("BSpline", ResampAlgoRltmBspline);
    rltmAlgoCmbo.addItem("Sinc", 	ResampAlgoRltmSinc	 );

    rendAlgoCmbo.addItem("Linear", 	ResampAlgoRendLinear );
    rendAlgoCmbo.addItem("Hermite", ResampAlgoRendHermite);
//	rendAlgoCmbo.addItem("BSpline", ResampAlgoRendBspline);
    rendAlgoCmbo.addItem("Sinc", 	ResampAlgoRendSinc	 );

    addAndMakeVisible(&rltmTitle	);
    addAndMakeVisible(&rltmOvsp		);
    addAndMakeVisible(&rltmAlgoCmbo	);
    addAndMakeVisible(&rltmHqLabel	);
    addAndMakeVisible(&rltmLabel	);

    addAndMakeVisible(&rendTitle	);
    addAndMakeVisible(&rendOvsp		);
    addAndMakeVisible(&rendAlgoCmbo	);

    addAndMakeVisible(&ctrlFreqCmbo	);
    addAndMakeVisible(&ctrlFreqLbl	);

    addAndMakeVisible(&paramSmoothLbl);
    addAndMakeVisible(&useSmooth	);

    rltmOvsp	.addListener(this);
    rendOvsp	.addListener(this);
    ctrlFreqCmbo.addListener(this);
    rltmAlgoCmbo.addListener(this);
    rendAlgoCmbo.addListener(this);
//	useCache	.addListener(this);
    useSmooth	.addListener(this);

    rltmTitle	.setFont(18);
    rendTitle	.setFont(18);
    rltmHqLabel	.setMinimumHorizontalScale(0.9f);

    setSize(500, 350);
}

void QualityDialog::comboBoxChanged(ComboBox* box) {
    int item = box->getSelectedId();

    if (item >= OversampRltm1x && item <= OversampRend16x) {
        if (item >= OversampRltm1x && item <= OversampRltm16x) {
            int oldFactor = getDocSetting(OversampleFactorRltm);

            int factor = -1;
            switch (item) {
                case OversampRltm1x: 	factor = 1; 	break;
                case OversampRltm2x: 	factor = 2; 	break;
                case OversampRltm4x: 	factor = 4; 	break;
                case OversampRltm16x: 	factor = 16; 	break;
                default:
                    break;
            }

            getDocSetting(OversampleFactorRltm) = factor;
            getDocSetting(SubsampleRltm) 		= (factor > 1);

            int& rendFactor = getDocSetting(OversampleFactorRend);
            if (rendFactor < factor) {
                rendFactor = factor;
                getDocSetting(SubsampleRend) = (factor > 1);
            }

            if (oldFactor == 1 != factor == 1) {
                onlyPlug(getObj(PluginProcessor).updateLatency());
            }
        }

        if (item >= OversampRend1x && item <= OversampRend16x) {
            char factor = -1;
            switch (item) {
                case OversampRend1x: 	factor = 1; 	break;
                case OversampRend2x: 	factor = 2; 	break;
                case OversampRend4x: 	factor = 4; 	break;
                case OversampRend16x: 	factor = 16; 	break;
                default:
                    break;
            }

            if(Util::assignAndWereDifferent(getDocSetting(OversampleFactorRend), factor)) {
                getObj(EditWatcher).setHaveEditedWithoutUndo(true);
            }

            getDocSetting(SubsampleRend) = (factor > 1);
        }

        onlyPlug(getObj(PluginProcessor).updateLatency());
    }

    else if(item >= ControlFreq16 && item <= ControlFreq1024)
    {
        char newFreqOrder = -1;

        switch(item)
        {
            case ControlFreq16: 	newFreqOrder = 4; 	break;
            case ControlFreq64: 	newFreqOrder = 6; 	break;
            case ControlFreq256:	newFreqOrder = 8;	break;
            case ControlFreq1024:	newFreqOrder = 10;	break;
            default:				jassertfalse;
        }

        if (Util::assignAndWereDifferent(getDocSetting(ControlFreq), newFreqOrder)) {
            getObj(SynthAudioSource).controlFreqChanged();
            getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        }
    } else if (item >= ResampAlgoRltmLinear && item <= ResampAlgoRltmSinc) {
        char newAlgo = Resampling::Linear;

        switch(item)
        {
            case ResampAlgoRltmLinear: 	newAlgo = Resampling::Linear;	break;
            case ResampAlgoRltmHermite: newAlgo = Resampling::Hermite;	break;
            case ResampAlgoRltmBspline: newAlgo = Resampling::BSpline;	break;
            case ResampAlgoRltmSinc: 	newAlgo = Resampling::Sinc;		break;
            default:
                break;
        }

        if (Util::assignAndWereDifferent(getDocSetting(ResamplingAlgoRltm), newAlgo)) {
            getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        }
    } else if (item >= ResampAlgoRendLinear && item <= ResampAlgoRendSinc) {
        int newAlgo = Resampling::Linear;

        switch(item) {
            case ResampAlgoRendLinear: 	newAlgo = Resampling::Linear;	break;
            case ResampAlgoRendHermite: newAlgo = Resampling::Hermite;	break;
            case ResampAlgoRendBspline: newAlgo = Resampling::BSpline;	break;
            case ResampAlgoRendSinc: 	newAlgo = Resampling::Sinc;		break;
            default:
                break;
        }

        if(Util::assignAndWereDifferent(getDocSetting(ResamplingAlgoRend), newAlgo)) {
            getObj(EditWatcher).setHaveEditedWithoutUndo(true);
        }
    }

    updateSelections();
}

void QualityDialog::updateSelections()
{
    int ovspRltmId;
    switch(getDocSetting(OversampleFactorRltm))
    {
        case 1:		ovspRltmId = OversampRltm1x;	break;
        case 2:		ovspRltmId = OversampRltm2x;	break;
        case 4:		ovspRltmId = OversampRltm4x;	break;
        case 16:	ovspRltmId = OversampRltm16x;	break;
        default:	ovspRltmId = OversampRltm1x;
    }

    int ovspRendId;
    switch(getDocSetting(OversampleFactorRend))
    {
        case 1:		ovspRendId = OversampRend1x;	break;
        case 2:		ovspRendId = OversampRend2x;	break;
        case 4:		ovspRendId = OversampRend4x;	break;
        case 16:	ovspRendId = OversampRend16x;	break;
        default:	ovspRendId = OversampRend1x;
    }

    rltmOvsp.setSelectedId(ovspRltmId);
    rendOvsp.setSelectedId(ovspRendId);

    int controlId;
    switch(getDocSetting(ControlFreq)) {
        case 4: 	controlId = ControlFreq16;		break;
        case 6: 	controlId = ControlFreq64;		break;
        case 8: 	controlId = ControlFreq256;		break;
        case 10:	controlId = ControlFreq1024;	break;
        default: 	controlId = ControlFreq256;
    }
    ctrlFreqCmbo.setSelectedId(controlId, dontSendNotification);

    int resampAlgoRltm = ResampAlgoRltmLinear;
    switch(getDocSetting(ResamplingAlgoRltm)) {
        case Resampling::Linear:	resampAlgoRltm = ResampAlgoRltmLinear;	break;
        case Resampling::Hermite:	resampAlgoRltm = ResampAlgoRltmHermite;	break;
        case Resampling::BSpline:	resampAlgoRltm = ResampAlgoRltmBspline;	break;
        case Resampling::Sinc:		resampAlgoRltm = ResampAlgoRltmSinc;	break;
        default:
            break;
    }

    int resampAlgoRend = ResampAlgoRendLinear;
    switch(getDocSetting(ResamplingAlgoRend)) {
        case Resampling::Linear:	resampAlgoRend = ResampAlgoRendLinear;	break;
        case Resampling::Hermite:	resampAlgoRend = ResampAlgoRendHermite;	break;
        case Resampling::BSpline:	resampAlgoRend = ResampAlgoRendBspline;	break;
        case Resampling::Sinc:		resampAlgoRend = ResampAlgoRendSinc;	break;
        default:
            break;
    }

    rltmAlgoCmbo.setSelectedId(resampAlgoRltm);
    rendAlgoCmbo.setSelectedId(resampAlgoRend);

    useSmooth.setToggleState(getDocSetting(ParameterSmoothing) == 1, dontSendNotification);
}

void QualityDialog::buttonClicked(Button* button) {
    if (button == &useSmooth) {
        getDocSetting(ParameterSmoothing) ^= true;
    }

    updateSelections();
}

void QualityDialog::resized() {
    Rectangle<int> bounds = getBounds().withPosition(0, 0);
    bounds.reduce(50, 30);

    int colSpace = 15;
    int firstCol = bounds.getWidth() / 3 + 60 - colSpace;
    int colWidth = (bounds.getWidth() - firstCol - 2 * colSpace) / 2;

    Rectangle<int> row 	= bounds.removeFromTop(25);
    row			.removeFromLeft(firstCol);
    row			.removeFromLeft(colSpace);

    // titles
    rltmTitle	.setBounds(row.removeFromLeft(colWidth));
    row			.removeFromLeft(colSpace);
    rendTitle	.setBounds(row);

    bounds		.removeFromTop(10);

    row 		= bounds.removeFromTop(36);
    rltmLabel	.setBounds(row.removeFromLeft(firstCol));
    row			.removeFromLeft(colSpace);
    row			.reduce(0, 3);
    rltmOvsp	.setBounds(row.removeFromLeft(colWidth));
    row			.removeFromLeft(colSpace);
    rendOvsp	.setBounds(row);

    bounds		.removeFromTop(20);

    row 		= bounds.removeFromTop(36);
    rltmHqLabel	.setBounds(row.removeFromLeft(firstCol));
    row			.removeFromLeft(colSpace);
    row			.reduce(0, 3);
    rltmAlgoCmbo.setBounds(row.removeFromLeft(colWidth));
    row			.removeFromLeft(colSpace);
    rendAlgoCmbo.setBounds(row);

    bounds		.removeFromTop(20);

//	row = bounds.removeFromTop(30);
//	useCache	.setBounds(row.removeFromLeft(20));
//	row	.removeFromLeft(1);
//	useCacheLbl	.setBounds(row);

    bounds		.removeFromTop(10);
    row			= bounds.removeFromTop(36);
    useSmooth	.setBounds(row.removeFromLeft(20));
    row.removeFromLeft(1);
    paramSmoothLbl.setBounds(row);

    ctrlFreqLbl	.setBounds(bounds.removeFromTop(20));
    ctrlFreqCmbo.setBounds(bounds.removeFromTop(30).removeFromLeft(150));
}

void QualityDialog::triggerOversample(int which) {
    rltmOvsp.setSelectedId(which, sendNotificationAsync);
}
