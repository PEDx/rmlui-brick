#ifndef TG5040_GAME_VERTEX_SHADER_H
#define TG5040_GAME_VERTEX_SHADER_H

static const char* game_vertex_shader_source =
	"attribute vec2 a_position;\n"
	"attribute vec2 a_texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
	"  v_texcoord = a_texcoord;\n"
	"}\n";

#endif
