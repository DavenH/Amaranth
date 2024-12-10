#ifndef QUALITYDIALOG_H_
#define QUALITYDIALOG_H_

#include "JuceHeader.h"
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

class QualityDialog :
		public Component
	,	public Button::Listener
	,	public ComboBox::Listener
	, 	public SingletonAccessor
{
public:
	QualityDialog(SingletonRepo* repo);
	void comboBoxChanged(ComboBox* box);
	void buttonClicked(Button* button);
	void updateSelections();
	void resized();
	void triggerOversample(int which);

	enum
	{
		OversampRltm1x = 1,
		OversampRltm2x,
		OversampRltm4x,
		OversampRltm16x,

		OversampRend1x,
		OversampRend2x,
		OversampRend4x,
		OversampRend16x,

		ControlFreq16,
		ControlFreq64,
		ControlFreq256,
		ControlFreq1024,
		UseCache,

		ResampAlgoRltmLinear,
		ResampAlgoRltmHermite,
		ResampAlgoRltmBspline,
		ResampAlgoRltmSinc,

		ResampAlgoRendLinear,
		ResampAlgoRendHermite,
		ResampAlgoRendBspline,
		ResampAlgoRendSinc,

		HighQualResampling
	};
private:

	ComboBox 		rltmOvsp, rendOvsp, ctrlFreqCmbo, rltmAlgoCmbo, rendAlgoCmbo;
	Label 			rltmTitle, rendTitle, rltmLabel, paramSmoothLbl, ctrlFreqLbl, rltmHqLabel;
	ToggleButton 	useSmooth;
};


#endif
