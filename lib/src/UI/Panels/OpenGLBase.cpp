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

namespace {
    String openGLErrorName(int errorCode) {
        switch(errorCode) {
            case GL_NO_ERROR:           return "GL_NO_ERROR";
            case GL_INVALID_ENUM:       return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:      return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:  return "GL_INVALID_OPERATION";
            case GL_STACK_OVERFLOW:     return "GL_STACK_OVERFLOW";
            case GL_STACK_UNDERFLOW:    return "GL_STACK_UNDERFLOW";
            case GL_OUT_OF_MEMORY:      return "GL_OUT_OF_MEMORY";
            default:                    break;
        }

        return "Unknown error";
    }
}

void OpenGLBase::printErrors(SingletonRepo* repo, const String& phase) {
    int errorCode = pollErrorNow(phase);

    if (errorCode != GL_NO_ERROR) {
        info("OpenGL error " << errorCode << ": " << openGLErrorName(errorCode)
             << " phase=" << phase << "\n");
    }
}

OpenGLBase::Diagnostics OpenGLBase::getDiagnostics(bool pollNow, const String& phase) {
    Diagnostics diagnostics;
    diagnostics.attached = context.isAttached();
    diagnostics.width = component != nullptr ? component->getWidth() : 0;
    diagnostics.height = component != nullptr ? component->getHeight() : 0;
    diagnostics.renderingScale = context.getRenderingScale();
    diagnostics.renderCount = renderCount;
    diagnostics.contextCreateCount = contextCreateCount;
    diagnostics.contextCloseCount = contextCloseCount;
    diagnostics.errorCount = errorCount;
    diagnostics.lastErrorCode = lastErrorCode;
    diagnostics.lastErrorName = lastErrorName.isNotEmpty() ? lastErrorName : openGLErrorName(lastErrorCode);
    diagnostics.lastErrorPhase = lastErrorPhase;

    if (pollNow && context.isAttached()) {
        int polledError = GL_NO_ERROR;
        context.executeOnGLThread([this, &polledError, phase](OpenGLContext&) {
            polledError = pollErrorNow(phase);
        }, true);

        diagnostics.pollSucceeded = true;
        diagnostics.polledErrorCode = polledError;
        diagnostics.polledErrorName = openGLErrorName(polledError);
        diagnostics.errorCount = errorCount;
        diagnostics.lastErrorCode = lastErrorCode;
        diagnostics.lastErrorName = lastErrorName;
        diagnostics.lastErrorPhase = lastErrorPhase;
    } else {
        diagnostics.polledErrorCode = GL_NO_ERROR;
        diagnostics.polledErrorName = openGLErrorName(GL_NO_ERROR);
    }

    return diagnostics;
}

void OpenGLBase::noteContextCreated() {
    ++contextCreateCount;
}

void OpenGLBase::noteContextClosing() {
    ++contextCloseCount;
}

void OpenGLBase::noteRender() {
    ++renderCount;
}

int OpenGLBase::pollErrorNow(const String& phase) {
    int firstError = GL_NO_ERROR;
    int errorCode = GL_NO_ERROR;

    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        if (firstError == GL_NO_ERROR) {
            firstError = errorCode;
        }

        recordError(errorCode, phase);
    }

    return firstError;
}

void OpenGLBase::recordError(int errorCode, const String& phase) {
    ++errorCount;
    lastErrorCode = errorCode;
    lastErrorName = openGLErrorName(errorCode);
    lastErrorPhase = phase;
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
