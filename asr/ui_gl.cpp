#include "ui.h"

#if OPENGL_ENABLED

OpenGLUI::OpenGLUI(ASIOProcessor *io, int argc, char **argv) : _io(io)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(1024,1024);
}

void OpenGLUI::create()
{
	glutCreateWindow("ASR");
}

#endif
