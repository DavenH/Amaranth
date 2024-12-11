#pragma once
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class WaveDragTarget :
		public FileDragAndDropTarget
	,	public virtual SingletonAccessor
{
public:
	class Listener
	{
	public:
		virtual ~Listener() = default;

		virtual void loadWave(const File& file) = 0;
	};

	explicit WaveDragTarget(SingletonRepo* repo);

	void setListener(Listener* listener) { this->listener = listener; }

	bool isInterestedInFileDrag (const StringArray& files) override;

	/** Callback to indicate that some files are being dragged over this component.

			This gets called when the user moves the mouse into this component while dragging.

			Use this callback as a trigger to make your component repaint itself to give the
			user feedback about whether the files can be dropped here or not.

			@param files        the set of (absolute) pathnames of the files that the user is dragging
			@param x            the mouse x position, relative to this component
			@param y            the mouse y position, relative to this component
	 */
	void fileDragEnter (const StringArray& files, int x, int y) override;


	/** Callback to indicate that the mouse has moved away from this component.

			This gets called when the user moves the mouse out of this component while dragging
			the files.

			If you've used fileDragEnter() to repaint your component and give feedback, use this
			as a signal to repaint it in its normal state.

			@param files        the set of (absolute) pathnames of the files that the user is dragging
	 */
	void fileDragExit (const StringArray& files) override;

	/** Callback to indicate that the user has dropped the files onto this component.

			When the user drops the files, this get called, and you can use the files in whatever
			way is appropriate.

			Note that after this is called, the fileDragExit method may not be called, so you should
			clean up in here if there's anything you need to do when the drag finishes.

			@param files        the set of (absolute) pathnames of the files that the user is dragging
			@param x            the mouse x position, relative to this component
			@param y            the mouse y position, relative to this component
	 */
	void filesDropped (const StringArray& files, int x, int y) override;

private:
	Ref<Listener> listener;
	bool isFileGood(const File& file);
};
