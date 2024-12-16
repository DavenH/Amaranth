#pragma once


  #ifdef _WIN32
	#include "../../Util/WinHeader.h"
	#include <gl/gl.h>
	#include <gl/glu.h>
  #elif defined (__linux__)
    #include <GL/gl.h>
    #include <GL/glu.h>
    #include <GL/glext.h>
  #elif defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glu.h>
	#include <OpenGL/gl3ext.h>
  #endif

