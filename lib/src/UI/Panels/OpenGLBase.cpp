#include "OpenGLBase.h"

#include <Definitions.h>

#include "Panel.h"
#include "OpenGL.h"
#include "../../App/SingletonRepo.h"

using namespace gl;

OpenGLBase::OpenGLBase(OpenGLRenderer* renderer, Component* component) :
        smoothLines     (true)
    ,   renderer        (renderer)
    ,   component       (component) {
}

void OpenGLBase::printErrors(SingletonRepo* repo) {
  #ifdef _DEBUG
    int errorCode;
    if((errorCode = glGetError()) != GL_NO_ERROR) {
        const char* error = "Unknown error";
        switch(errorCode) {
            case GL_INVALID_ENUM: error      = "GL_INVALID_ENUM";       break;
            case GL_INVALID_VALUE: error     = "GL_INVALID_VALUE";      break;
            case GL_INVALID_OPERATION: error = "GL_INVALID_OPERATION";  break;
            case GL_STACK_OVERFLOW: error    = "GL_STACK_OVERFLOW";     break;
            case GL_STACK_UNDERFLOW: error   = "GL_STACK_UNDERFLOW";    break;
            case GL_OUT_OF_MEMORY: error     = "GL_OUT_OF_MEMORY";      break;
            default:
                error = "Unknown error";
        }
        info("OpenGL error " << errorCode << ": " << error << "\n");
    }
  #endif
}

void OpenGLBase::clearAndOrtho(int width, int height) {
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
    // gluOrtho2D(0.0, width, height, 0.0);
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
