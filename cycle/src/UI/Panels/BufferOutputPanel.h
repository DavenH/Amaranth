//#ifndef _bufferoutputpanel_h
//#define _bufferoutputpanel_h
//
//#include <UI/Panels/Panel.h>
//
//class BufferOutputPanel :
//	public Panel,
//	public Timer
//{
//private:
//	ScopedAlloc<Float32> channelData[2];
//	Buffer<float> currData[2];
//
//	BufferOutputPanel(SingletonRepo* repo) :
//		Panel("Buffer Output Panel", repo)
//	{
//		startTimer(30);
//
//		channelData[0].resize(8192);
//		channelData[1].resize(8192);
//	}
//
//public:
//	virtual ~BufferOutputPanel()
//	{
//	}
//
//	void renderOpenGL()
//	{
//		Panel::renderOpenGL();
//
//		int size = currData[0].size();
//		if (size <= 1)
//			return;
//
//		ScopedAlloc<Float32> xs(size);
//		ippsVectorRamp_32f(xs, size, 0.f, 1.f / float(size - 1));
//
//		ippsMulC_32f_I(0.25, currData[0], size);
//		ippsAddC_32f_I(0.75f, currData[0], size);
//
//		ippsMulC_32f_I(0.25, currData[1], size);
//		ippsAddC_32f_I(0.25f, currData[1], size);
//
//		for (int c = 0; c < 2; ++c)
//		{
//			float yOffset = 0.75f - c * 0.5;
//			glColor3f(1, 1, 1);
//			glBegin(GL_LINE_STRIP);
//
//			drawAndScaleLineStrip(Buffer<float>(xs), currData[c]);
//			glEnd();
//		}
//
//		glColor3f(1.f, 0.5f, 0.2f);
//		glBegin(GL_LINES);
//		glVertex2f(sx(0), sy(0.5f));
//		glEnd();
//	}
//
//	void timerCallback()
//	{
//		repaint();
//	}
//
//	void setBufferAndRepaint(AudioSampleBuffer& buffer)
//	{
//
//		for (int c = 0; c < buffer.getNumChannels(); ++c)
//		{
//			channelData[c].ensureSize(buffer.getNumSamples());
//			ippsCopy_32f(buffer.getWritePointer(c), channelData[c], buffer.getNumSamples());
//			currData[c] = Buffer<float>(channelData[c], buffer.getNumSamples());
//		}
//
////		if (isVisible())
////		{
////			* current = buffer;
////			repaint();
////		}
////		repaint();
//	}
//};
//
//#endif
