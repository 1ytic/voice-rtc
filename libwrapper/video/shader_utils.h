/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */
#ifndef _SHADER_UTILS_H
#define _SHADER_UTILS_H
#include <GL/osmesa.h>
GLuint create_program(const char* vertexfile, const char *fragmentfile);
GLint get_attrib(GLuint program, const char *name);
GLint get_uniform(GLuint program, const char *name);
#endif
