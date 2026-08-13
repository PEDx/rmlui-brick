#ifndef TG5040_GAME_GEAR_FRAGMENT_SHADER_H
#define TG5040_GAME_GEAR_FRAGMENT_SHADER_H

static const char* game_gear_fragment_shader_source =
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"uniform sampler2D u_texture;\n"
	"uniform float u_effect_mode;\n"
	"uniform vec4 u_game_viewport;\n"
	"uniform float u_output_height;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  vec4 color = texture2D(u_texture, v_texcoord);\n"
	"  if (u_effect_mode > 0.5) {\n"
	"    vec2 game_pos = vec2(\n"
	"      gl_FragCoord.x - u_game_viewport.x,\n"
	"      (u_output_height - gl_FragCoord.y) - u_game_viewport.y\n"
	"    );\n"
	"    vec2 local_pixel = mod(floor(game_pos), vec2(6.0, 5.0));\n"
	"    float edge_x = (1.0 - step(0.5, local_pixel.x)) + step(4.5, local_pixel.x);\n"
	"    float edge_y = (1.0 - step(0.5, local_pixel.y)) + step(3.5, local_pixel.y);\n"
	"    float aperture = 1.0 - 0.08 * edge_x - 0.10 * edge_y;\n"
	"    color.rgb *= aperture;\n"
	"  }\n"
	"  gl_FragColor = color;\n"
	"}\n";

#endif
