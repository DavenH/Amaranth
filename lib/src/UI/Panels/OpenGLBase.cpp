#include "OpenGLBase.h"
#include "Panel.h"
#include "../../App/SingletonRepo.h"


OpenGLBase::OpenGLBase(OpenGLRenderer* renderer, Component* component) :
		smoothLines		(true)
	,	renderer		(renderer)
	,	component		(component) {
}

OpenGLBase::~OpenGLBase() {
}


void OpenGLBase::printErrors(SingletonRepo* repo) {
#ifdef _DEBUG
    int errorCode;
    if((errorCode = glGetError()) != GL_NO_ERROR)
    {
        const char* error = (char*)(gluErrorString(errorCode));
        dout << "OpenGL error " << errorCode << ": " << error << "\n";
    }
#endif
}


void OpenGLBase::clearAndOrtho(int width, int height) {
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    gluOrtho2D(0.0, width, height, 0.0);
    glColor3f(1, 1, 1);
}


void OpenGLBase::detach() {
    context.detach();
}


void OpenGLBase::attach() {
    if (!context.isAttached()) {
        context.setRenderer(renderer);
        context.setComponentPaintingEnabled(false);
        context.attachTo(*component);
    }
}
