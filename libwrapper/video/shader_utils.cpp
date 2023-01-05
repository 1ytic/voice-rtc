/**
 * From the OpenGL Programming wikibook: http://en.wikibooks.org/wiki/OpenGL_Programming
 * This file is in the public domain.
 * Contributors: Sylvain Beucler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <GL/osmesa.h>

/**
 * Store all the file's contents in memory, useful to pass shaders
 * source code to OpenGL
 */
char* file_read(const char* filename) {
  FILE* in = fopen(filename, "rb");
  if (in == NULL) return NULL;

  int res_size = BUFSIZ;
  char* res = (char*)malloc(res_size);
  int nb_read_total = 0;

  while (!feof(in) && !ferror(in)) {
    if (nb_read_total + BUFSIZ > res_size) {
      if (res_size > 10*1024*1024) break;
      res_size = res_size * 2;
      res = (char*)realloc(res, res_size);
    }
    char* p_res = res + nb_read_total;
    nb_read_total += fread(p_res, 1, BUFSIZ, in);
  }
  
  fclose(in);
  res = (char*)realloc(res, nb_read_total + 1);
  res[nb_read_total] = '\0';
  return res;
}

/**
 * Display compilation errors from the OpenGL shader compiler
 */
void print_log(GLuint object) {
  GLint log_length = 0;
  if (glIsShader(object))
    glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
  else if (glIsProgram(object))
    glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
  else {
    fprintf(stderr, "printlog: Not a shader or a program\n");
    return;
  }
  char* log = (char*)malloc(log_length);
  if (glIsShader(object))
    glGetShaderInfoLog(object, log_length, NULL, log);
  else if (glIsProgram(object))
    glGetProgramInfoLog(object, log_length, NULL, log);
  fprintf(stderr, "%s", log);
  free(log);
}

/**
 * Compile the shader from file 'filename', with error handling
 */
GLuint create_shader(const char* filename, GLenum type) {
  const GLchar* source = file_read(filename);
  if (source == NULL) {
    fprintf(stderr, "Error opening %s\n", filename);
    return 0;
  }
  GLuint res = glCreateShader(type);
  const GLchar* sources[] = {source};
	GLint lengths[] = {(GLint)strlen(source)};
  glShaderSource(res, 1, sources, lengths);
  free((void*)source);
  glCompileShader(res);
  GLint compile_ok = GL_FALSE;
  glGetShaderiv(res, GL_COMPILE_STATUS, &compile_ok);
  if (compile_ok == GL_FALSE) {
    fprintf(stderr, "%s:", filename);
    print_log(res);
    glDeleteShader(res);
    return 0;
  }
  return res;
}

GLuint create_program(const char *vertexfile, const char *fragmentfile) {

	GLuint program = glCreateProgram();

	if (vertexfile) {
		GLuint shader = create_shader(vertexfile, GL_VERTEX_SHADER);
		if (!shader)
			return 0;
		glAttachShader(program, shader);
	}

	if (fragmentfile) {
		GLuint shader = create_shader(fragmentfile, GL_FRAGMENT_SHADER);
		if (!shader)
			return 0;
		glAttachShader(program, shader);
	}

	glLinkProgram(program);

	GLint link_ok = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (!link_ok) {
		print_log(program);
		glDeleteProgram(program);
		return 0;
	}

	return program;
}

GLint get_attrib(GLuint program, const char *name) {
	GLint attribute = glGetAttribLocation(program, name);
	if(attribute == -1)
		fprintf(stderr, "Could not bind attribute %s\n", name);
	return attribute;
}

GLint get_uniform(GLuint program, const char *name) {
	GLint uniform = glGetUniformLocation(program, name);
	if(uniform == -1)
		fprintf(stderr, "Could not bind uniform %s\n", name);
	return uniform;
}




//////////////////////////////////////////////////
// Grab the OpenGL screen and save it as a .tga //
// Copyright (C) Marius Andra 2001              //
// http://cone3d.gz.ee  EMAIL: cone3d@hot.ee    //
//////////////////////////////////////////////////
// (modified by me a little)
static int screenShot() {
    typedef unsigned char uchar;
    // we will store the image data here
    uchar *pixels;
    // the thingy we use to write files
    FILE * shot;
    // we get the width/height of the screen into this array
    int screenStats[4];

    // get the width/height of the window
    glGetIntegerv(GL_VIEWPORT, screenStats);

    // generate an array large enough to hold the pixel data 
    // (width*height*bytesPerPixel)
    pixels = new unsigned char[screenStats[2]*screenStats[3]*3];
    // read in the pixel data, TGA's pixels are BGR aligned
    glReadPixels(0, 0, screenStats[2], screenStats[3], 0x80E0, 
    GL_UNSIGNED_BYTE, pixels);

    // open the file for writing. If unsucessful, return 1

    shot=fopen("screen.tga", "wb");

    if (shot == NULL)
        return 1;

    // this is the tga header it must be in the beginning of 
    // every (uncompressed) .tga
    uchar TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};
    // the header that is used to get the dimensions of the .tga
    // header[1]*256+header[0] - width
    // header[3]*256+header[2] - height
    // header[4] - bits per pixel
    // header[5] - ?
    uchar header[6]={((uchar)(screenStats[2]%256)),
    ((uchar)(screenStats[2]/256)),
    ((uchar)(screenStats[3]%256)),
    ((uchar)(screenStats[3]/256)),24,0};

    // write out the TGA header
    fwrite(TGAheader, sizeof(uchar), 12, shot);
    // write out the header
    fwrite(header, sizeof(uchar), 6, shot);
    // write the pixels
    fwrite(pixels, sizeof(uchar), 
    screenStats[2]*screenStats[3]*3, shot);

    // close the file
    fclose(shot);
    // free the memory
    delete [] pixels;

    // return success
    return 0;
}
