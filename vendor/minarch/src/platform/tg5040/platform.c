// tg5040
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <GLES2/gl2.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

int is_brick = 0;

///////////////////////////////

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;
	SDL_Texture* screen_mask;
	SDL_Texture* lcd_mask;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
	int screen_mask_type;
	int screen_mask_active;
	int use_game_gl;
	int crt_shader_mode;
	int gl_effect_logged;
	SDL_GLContext gl_context;
	GLuint gl_program;
	GLuint gl_game_texture;
	GLuint gl_overlay_texture;
	GLint gl_position;
	GLint gl_texcoord;
	GLint gl_sampler;
	GLint gl_effect_mode;
	GLint gl_game_viewport;
	GLint gl_output_height;
	GLint gl_texture_size;
	int gl_texture_width;
	int gl_texture_height;
	void* gl_upload_buffer;
	size_t gl_upload_capacity;
} vid;

enum {
	SCREEN_MASK_NONE = 0,
	SCREEN_MASK_GB,
	SCREEN_MASK_GBC,
	SCREEN_MASK_GBA,
	SCREEN_MASK_SFC,
	SCREEN_MASK_MD,
};

enum {
	SCREEN_EFFECT_NONE = 0,
	SCREEN_EFFECT_GBA_APERTURE,
	SCREEN_EFFECT_SFC_SHARP,
	SCREEN_EFFECT_SFC_CRT,
	SCREEN_EFFECT_SFC_COMPOSITE,
};

static int device_width;
static int device_height;
static int device_pitch;

static const char* game_vertex_shader =
	"attribute vec2 a_position;\n"
	"attribute vec2 a_texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  gl_Position = vec4(a_position, 0.0, 1.0);\n"
	"  v_texcoord = a_texcoord;\n"
	"}\n";

static const char* game_fragment_shader =
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"precision highp float;\n"
	"#else\n"
	"precision mediump float;\n"
	"#endif\n"
	"uniform sampler2D u_texture;\n"
	"uniform float u_effect_mode;\n"
	"uniform vec4 u_game_viewport;\n"
	"uniform float u_output_height;\n"
	"uniform vec2 u_texture_size;\n"
	"varying vec2 v_texcoord;\n"
	"void main() {\n"
	"  vec4 color = texture2D(u_texture, v_texcoord);\n"
	"  if (u_effect_mode > 0.5 && u_effect_mode < 1.5) {\n"
	"    vec2 game_pos = vec2(\n"
	"      gl_FragCoord.x - u_game_viewport.x,\n"
	"      (u_output_height - gl_FragCoord.y) - u_game_viewport.y\n"
	"    );\n"
	"    vec2 local_pixel = mod(floor(game_pos), 4.0);\n"
	"    float edge_x = (1.0 - step(0.5, local_pixel.x)) + step(2.5, local_pixel.x);\n"
	"    float edge_y = (1.0 - step(0.5, local_pixel.y)) + step(2.5, local_pixel.y);\n"
	"    float aperture = 1.0 - 0.10 * (edge_x + edge_y);\n"
	"    color.rgb *= aperture;\n"
	"  } else if (u_effect_mode > 1.5) {\n"
	"    vec2 texel = 1.0 / u_texture_size;\n"
	"    vec3 left = texture2D(u_texture, v_texcoord - vec2(texel.x, 0.0)).rgb;\n"
	"    vec3 right = texture2D(u_texture, v_texcoord + vec2(texel.x, 0.0)).rgb;\n"
	"    vec3 up = texture2D(u_texture, v_texcoord - vec2(0.0, texel.y)).rgb;\n"
	"    vec3 down = texture2D(u_texture, v_texcoord + vec2(0.0, texel.y)).rgb;\n"
	"    float consumer_enabled = step(2.5, u_effect_mode);\n"
	"    float composite_enabled = step(3.5, u_effect_mode);\n"
	"    float side_weight = mix(0.0, 0.04, consumer_enabled);\n"
	"    side_weight = mix(side_weight, 0.14, composite_enabled);\n"
	"    vec3 rgb = color.rgb * (1.0 - 2.0 * side_weight)\n"
	"             + (left + right) * side_weight;\n"
	"    if (u_effect_mode > 3.5) {\n"
	"      vec3 composite = vec3(left.r, rgb.g, right.b);\n"
	"      rgb = mix(rgb, composite, 0.22);\n"
	"    }\n"
	"    vec2 game_pos = vec2(\n"
	"      gl_FragCoord.x - u_game_viewport.x,\n"
	"      (u_output_height - gl_FragCoord.y) - u_game_viewport.y\n"
	"    );\n"
	"    vec2 local_pixel = mod(floor(game_pos), 3.0);\n"
	"    float mid_loss = mix(0.10, 0.16, consumer_enabled);\n"
	"    float bottom_loss = mix(0.22, 0.24, consumer_enabled);\n"
	"    float scanline = 1.0 - mid_loss * step(0.5, local_pixel.y)\n"
	"                         - bottom_loss * step(1.5, local_pixel.y);\n"
	"    float mask_column = mod(floor(game_pos.x * 0.5), 3.0);\n"
	"    float red_column = 1.0 - step(0.5, mask_column);\n"
	"    float blue_column = step(1.5, mask_column);\n"
	"    float green_column = 1.0 - red_column - blue_column;\n"
	"    float phosphor_base = mix(0.98, 0.975, consumer_enabled);\n"
	"    vec3 phosphor = vec3(phosphor_base)\n"
	"                  + 3.0 * (1.0 - phosphor_base)\n"
	"                  * vec3(red_column, green_column, blue_column);\n"
	"    vec3 bloom_source = (left + right + up + down) * 0.25;\n"
	"    float bloom_strength = mix(0.02, 0.055, consumer_enabled);\n"
	"    bloom_strength = mix(bloom_strength, 0.08, composite_enabled);\n"
	"    vec3 bloom = max(bloom_source - vec3(0.62), vec3(0.0)) * bloom_strength;\n"
	"    vec2 screen_pos = game_pos / u_game_viewport.zw * 2.0 - 1.0;\n"
	"    float vignette_strength = mix(0.0, 0.055, consumer_enabled);\n"
	"    float vignette = 1.0 - vignette_strength * dot(screen_pos, screen_pos);\n"
	"    rgb *= scanline * phosphor;\n"
	"    float gamma_power = mix(0.92, 0.90, consumer_enabled);\n"
	"    float brightness = mix(1.10, 1.14, consumer_enabled);\n"
	"    rgb = pow(max(rgb, vec3(0.0)), vec3(gamma_power));\n"
	"    rgb = (((rgb - vec3(0.5)) * 1.04 + vec3(0.5)) * brightness + bloom) * vignette;\n"
	"    color.rgb = clamp(rgb, 0.0, 1.0);\n"
	"  }\n"
	"  gl_FragColor = color;\n"
	"}\n";

static GLuint compileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		char log[512] = {0};
		glGetShaderInfoLog(shader, sizeof(log) - 1, NULL, log);
		LOG_info("Game shader compile failed: %s\n", log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static int initGameGl(void) {
	vid.gl_context = SDL_GL_CreateContext(vid.window);
	if (!vid.gl_context) {
		LOG_info("Game GLES context is unavailable: %s\n", SDL_GetError());
		return 0;
	}
	SDL_GL_SetSwapInterval(1);

	GLuint vertex = compileShader(GL_VERTEX_SHADER, game_vertex_shader);
	GLuint fragment = compileShader(GL_FRAGMENT_SHADER, game_fragment_shader);
	if (!vertex || !fragment) return 0;

	vid.gl_program = glCreateProgram();
	glAttachShader(vid.gl_program, vertex);
	glAttachShader(vid.gl_program, fragment);
	glLinkProgram(vid.gl_program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	GLint linked = GL_FALSE;
	glGetProgramiv(vid.gl_program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[512] = {0};
		glGetProgramInfoLog(vid.gl_program, sizeof(log) - 1, NULL, log);
		LOG_info("Game shader link failed: %s\n", log);
		return 0;
	}

	vid.gl_position = glGetAttribLocation(vid.gl_program, "a_position");
	vid.gl_texcoord = glGetAttribLocation(vid.gl_program, "a_texcoord");
	vid.gl_sampler = glGetUniformLocation(vid.gl_program, "u_texture");
	vid.gl_effect_mode = glGetUniformLocation(vid.gl_program, "u_effect_mode");
	vid.gl_game_viewport = glGetUniformLocation(vid.gl_program, "u_game_viewport");
	vid.gl_output_height = glGetUniformLocation(vid.gl_program, "u_output_height");
	vid.gl_texture_size = glGetUniformLocation(vid.gl_program, "u_texture_size");

	glGenTextures(1, &vid.gl_game_texture);
	glBindTexture(GL_TEXTURE_2D, vid.gl_game_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glViewport(0, 0, FIXED_WIDTH, FIXED_HEIGHT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	LOG_info("Game GLES2 screen effects initialized: %s / %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER));
	return 1;
}

static int uploadGameOverlay(SDL_Surface* source) {
	SDL_Surface* rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
	if (!rgba) return 0;
	glGenTextures(1, &vid.gl_overlay_texture);
	glBindTexture(GL_TEXTURE_2D, vid.gl_overlay_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
	SDL_FreeSurface(rgba);
	return glGetError() == GL_NO_ERROR;
}

static void uploadGameFrame(const void* pixels, int width, int height, int pitch) {
	const void* upload = pixels;
	size_t row_size = (size_t)width * sizeof(uint16_t);
	size_t required = row_size * height;
	if ((size_t)pitch != row_size) {
		if (vid.gl_upload_capacity < required) {
			void* resized = realloc(vid.gl_upload_buffer, required);
			if (!resized) return;
			vid.gl_upload_buffer = resized;
			vid.gl_upload_capacity = required;
		}
		for (int y = 0; y < height; y++) {
			memcpy((uint8_t*)vid.gl_upload_buffer + y * row_size, (const uint8_t*)pixels + y * pitch, row_size);
		}
		upload = vid.gl_upload_buffer;
	}

	glBindTexture(GL_TEXTURE_2D, vid.gl_game_texture);
	if (vid.gl_texture_width != width || vid.gl_texture_height != height) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, upload);
		vid.gl_texture_width = width;
		vid.gl_texture_height = height;
	}
	else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, upload);
	}
}

static void drawGameTexture(GLuint texture, const SDL_Rect* src, int texture_width, int texture_height,
	const SDL_Rect* dst, int effect_mode, int blend) {
	const GLfloat left = -1.0f + 2.0f * dst->x / device_width;
	const GLfloat right = -1.0f + 2.0f * (dst->x + dst->w) / device_width;
	const GLfloat top = 1.0f - 2.0f * dst->y / device_height;
	const GLfloat bottom = 1.0f - 2.0f * (dst->y + dst->h) / device_height;
	const GLfloat u0 = (GLfloat)src->x / texture_width;
	const GLfloat v0 = (GLfloat)src->y / texture_height;
	const GLfloat u1 = (GLfloat)(src->x + src->w) / texture_width;
	const GLfloat v1 = (GLfloat)(src->y + src->h) / texture_height;
	const GLfloat positions[] = { left, top, left, bottom, right, top, right, bottom };
	const GLfloat texcoords[] = { u0, v0, u0, v1, u1, v0, u1, v1 };

	glUseProgram(vid.gl_program);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(vid.gl_sampler, 0);
	glUniform1f(vid.gl_effect_mode, (GLfloat)effect_mode);
	glUniform4f(vid.gl_game_viewport, dst->x, dst->y, dst->w, dst->h);
	glUniform1f(vid.gl_output_height, (GLfloat)device_height);
	glUniform2f(vid.gl_texture_size, (GLfloat)texture_width, (GLfloat)texture_height);
	if (blend) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else glDisable(GL_BLEND);
	glVertexAttribPointer(vid.gl_position, 2, GL_FLOAT, GL_FALSE, 0, positions);
	glVertexAttribPointer(vid.gl_texcoord, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
	glEnableVertexAttribArray(vid.gl_position);
	glEnableVertexAttribArray(vid.gl_texcoord);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void destroyGameGl(void) {
	if (vid.gl_game_texture) glDeleteTextures(1, &vid.gl_game_texture);
	if (vid.gl_overlay_texture) glDeleteTextures(1, &vid.gl_overlay_texture);
	if (vid.gl_program) glDeleteProgram(vid.gl_program);
	free(vid.gl_upload_buffer);
	vid.gl_upload_buffer = NULL;
	vid.gl_upload_capacity = 0;
	if (vid.gl_context) SDL_GL_DeleteContext(vid.gl_context);
	vid.gl_context = NULL;
}

SDL_Surface* PLAT_initVideo(void) {
	char* device = getenv("DEVICE");
	is_brick = device && strcmp(device, "brick") == 0;
	const char* system = getenv("MINARCH_SYSTEM");
	int request_game_gl = is_brick && system &&
		(strcmp(system, "GBA") == 0 || strcmp(system, "SFC") == 0 || strcmp(system, "MD") == 0);
	vid.crt_shader_mode = SCREEN_EFFECT_SFC_SHARP;
	const char* crt_shader = system && strcmp(system, "MD") == 0
		? getenv("MINARCH_MD_SHADER")
		: getenv("MINARCH_SFC_SHADER");
	if (crt_shader && strcmp(crt_shader, "composite") == 0) {
		vid.crt_shader_mode = SCREEN_EFFECT_SFC_COMPOSITE;
	}
	else if (crt_shader &&
		(strcmp(crt_shader, "crt") == 0 || strcmp(crt_shader, "consumer") == 0)) {
		vid.crt_shader_mode = SCREEN_EFFECT_SFC_CRT;
	}
	else if (crt_shader && strcmp(crt_shader, "off") == 0) {
		vid.crt_shader_mode = SCREEN_EFFECT_NONE;
	}
	// LOG_info("DEVICE: %s is_brick: %i\n", device, is_brick);
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	//
	// LOG_info("Available video drivers:\n");
	// for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetVideoDriver(i));
	// }
	// LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	//
	// LOG_info("Available render drivers:\n");
	// for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
	// 	SDL_RendererInfo info;
	// 	SDL_GetRenderDriverInfo(i,&info);
	// 	LOG_info("- %s\n", info.name);
	// }
	//
	// LOG_info("Available display modes:\n");
	// SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (request_game_gl) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}
	vid.window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
		SDL_WINDOW_SHOWN | (request_game_gl ? SDL_WINDOW_OPENGL : 0));
	vid.use_game_gl = request_game_gl && initGameGl();
	if (request_game_gl && !vid.use_game_gl) {
		destroyGameGl();
		SDL_DestroyWindow(vid.window);
		vid.window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
		LOG_info("Falling back to SDL renderer for %s\n", system ? system : "game");
	}
	vid.renderer = vid.use_game_gl ? NULL : SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	vid.texture = vid.use_game_gl ? NULL : SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	vid.screen_mask = NULL;
	vid.lcd_mask = NULL;
	vid.screen_mask_type = SCREEN_MASK_NONE;
	vid.screen_mask_active = 0;

	if (is_brick) {
		const char* mask_name = NULL;
		const char* lcd_name = NULL;
		if (system && strcmp(system, "GB") == 0) {
			vid.screen_mask_type = SCREEN_MASK_GB;
			mask_name = "gb-brick-mask.png";
			lcd_name = "gb-gbc-lcd-5x.png";
		}
		else if (system && strcmp(system, "GBC") == 0) {
			vid.screen_mask_type = SCREEN_MASK_GBC;
			mask_name = "gbc-brick-mask.png";
			lcd_name = "gb-gbc-lcd-5x.png";
		}
		else if (system && strcmp(system, "GBA") == 0) {
			vid.screen_mask_type = SCREEN_MASK_GBA;
			mask_name = "gba-brick-mask.png";
		}
		else if (system && strcmp(system, "SFC") == 0) {
			vid.screen_mask_type = SCREEN_MASK_SFC;
			mask_name = "sfc-brick-mask.png";
			if (!vid.use_game_gl) lcd_name = "sfc-crt-3x.png";
			const char* preset = vid.crt_shader_mode==SCREEN_EFFECT_SFC_COMPOSITE ? "composite" :
				(vid.crt_shader_mode==SCREEN_EFFECT_SFC_CRT ? "crt" :
				(vid.crt_shader_mode==SCREEN_EFFECT_NONE ? "off" : "sharp"));
			LOG_info("SFC screen shader preset: %s\n", preset);
		}
		else if (system && strcmp(system, "MD") == 0) {
			vid.screen_mask_type = SCREEN_MASK_MD;
			mask_name = "md-brick-mask.png";
			if (!vid.use_game_gl) lcd_name = "md-crt-3x.png";
			const char* preset = vid.crt_shader_mode==SCREEN_EFFECT_SFC_COMPOSITE ? "composite" :
				(vid.crt_shader_mode==SCREEN_EFFECT_SFC_CRT ? "crt" :
				(vid.crt_shader_mode==SCREEN_EFFECT_NONE ? "off" : "sharp"));
			LOG_info("MD screen shader preset: %s\n", preset);
		}

		if (mask_name) {
			char mask_path[128];
			snprintf(mask_path, sizeof(mask_path), "%s/%s", RES_PATH, mask_name);
			SDL_Surface* mask_surface = IMG_Load(mask_path);
			if (mask_surface) {
				if (vid.use_game_gl) {
					if (!uploadGameOverlay(mask_surface)) LOG_info("Game GLES overlay upload failed\n");
				}
				else {
					vid.screen_mask = SDL_CreateTextureFromSurface(vid.renderer, mask_surface);
					if (vid.screen_mask) SDL_SetTextureBlendMode(vid.screen_mask, SDL_BLENDMODE_BLEND);
				}
				SDL_FreeSurface(mask_surface);
			}
			else {
				LOG_info("Screen mask is unavailable: %s\n", IMG_GetError());
			}
		}

		if (lcd_name) {
			char lcd_path[128];
			snprintf(lcd_path, sizeof(lcd_path), "%s/%s", RES_PATH, lcd_name);
			SDL_Surface* lcd_surface = IMG_Load(lcd_path);
			if (lcd_surface) {
				vid.lcd_mask = SDL_CreateTextureFromSurface(vid.renderer, lcd_surface);
				if (vid.lcd_mask) {
					SDL_SetTextureBlendMode(vid.lcd_mask, SDL_BLENDMODE_BLEND);
					SDL_SetTextureScaleMode(vid.lcd_mask, SDL_ScaleModeNearest);
				}
				SDL_FreeSurface(lcd_surface);
			}
			else {
				LOG_info("LCD mask is unavailable: %s\n", IMG_GetError());
			}
			if (vid.texture) SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
		}
	}
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}

static void clearVideo(void) {
	if (vid.use_game_gl) {
		for (int i=0; i<3; i++) {
			glClear(GL_COLOR_BUFFER_BIT);
			SDL_FillRect(vid.screen, NULL, 0);
			SDL_GL_SwapWindow(vid.window);
		}
		return;
	}
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, 0);
		
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, 0);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.use_game_gl) destroyGameGl();
	else {
		if (vid.target) SDL_DestroyTexture(vid.target);
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		if (vid.screen_mask) SDL_DestroyTexture(vid.screen_mask);
		if (vid.lcd_mask) SDL_DestroyTexture(vid.lcd_mask);
		if (vid.texture) SDL_DestroyTexture(vid.texture);
		if (vid.renderer) SDL_DestroyRenderer(vid.renderer);
	}
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	if (vid.use_game_gl) glClear(GL_COLOR_BUFFER_BIT);
	else SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	vid.screen_mask_active = is_brick &&
		((vid.screen_mask_type==SCREEN_MASK_GBA && w==240 && h==160) ||
		 (vid.screen_mask_type==SCREEN_MASK_SFC && w==256 && (h==224 || h==239)) ||
		 (vid.screen_mask_type==SCREEN_MASK_MD && w==320 && (h==224 || h==240)) ||
		 (vid.screen_mask_type!=SCREEN_MASK_NONE && w==160 && h==144));
	if (vid.screen_mask_type==SCREEN_MASK_SFC && w==256 && h==239) {
		LOG_info("SFC 239-line overscan: crop 7 top + 8 bottom for 256x224 mask viewport\n");
	}
	if (vid.screen_mask_type==SCREEN_MASK_MD && w==320 && h==240) {
		LOG_info("MD 240-line overscan: crop 8 top + 8 bottom for 320x224 mask viewport\n");
	}
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native, is this true?
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	// else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient for 640x480)
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_FreeSurface(vid.buffer);
	if (vid.use_game_gl) {
		vid.buffer = SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
		vid.width = w;
		vid.height = h;
		vid.pitch = p;
		return;
	}
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	if (vid.lcd_mask) SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}

static struct FX_Context {
	int scale;
	int type;
	int color;
	int next_scale;
	int next_type;
	int next_color;
	int live_type;
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
	.color = 0,
	.next_color = 0,
};
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Extract the red component (5 bits)
    uint8_t red = (rgb565 >> 11) & 0x1F;
    // Extract the green component (6 bits)
    uint8_t green = (rgb565 >> 5) & 0x3F;
    // Extract the blue component (5 bits)
    uint8_t blue = rgb565 & 0x1F;

    // Scale the values to 8-bit range
    *r = (red << 3) | (red >> 2);
    *g = (green << 2) | (green >> 4);
    *b = (blue << 3) | (blue >> 2);
}
static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type && effect.next_color==effect.color) return; // unchanged
	
	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;
	
	if (effect.type==EFFECT_NONE) return; // disabled
	if (effect.type==effect.live_type && effect.scale==live_scale && effect.color==live_color) return; // already loaded
	
	char* effect_path;
	int opacity = 128; // 1 - 1/2 = 50%
	if (effect.type==EFFECT_LINE) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/line-2.png";
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/line-3.png";
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/line-4.png";
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/line-5.png";
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/line-6.png";
		}
		else {
			effect_path = RES_PATH "/line-8.png";
		}
	}
	else if (effect.type==EFFECT_GRID) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/grid-2.png";
			opacity = 64; // 1 - 3/4 = 25%
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/grid-3.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/grid-4.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/grid-5.png";
			opacity = 160; // 1 - 9/25 = ~64%
			// opacity = 96; // TODO: tmp, for white grid
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/grid-6.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<11) {
			effect_path = RES_PATH "/grid-8.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else {
			effect_path = RES_PATH "/grid-11.png";
			opacity = 136; // 1 - 57/121 = ~52%
		}
	}
	
	// LOG_info("effect: %s opacity: %i\n", effect_path, opacity);
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		if (effect.type==EFFECT_GRID) {
			if (effect.color) {
				// LOG_info("dmg color grid...\n");
			
				uint8_t r,g,b;
				rgb565_to_rgb888(effect.color,&r,&g,&b);
				// LOG_info("rgb %i,%i,%i\n",r,g,b); 
			
				uint32_t* pixels = (uint32_t*)tmp->pixels;
				int width = tmp->w;
				int height = tmp->h;
				for (int y = 0; y < height; ++y) {
				    for (int x = 0; x < width; ++x) {
				        uint32_t pixel = pixels[y * width + x];
				        uint8_t _,a;
				        SDL_GetRGBA(pixel, tmp->format, &_, &_, &_, &a);
				        if (a) pixels[y * width + x] = SDL_MapRGBA(tmp->format, r,g,b, a);
				    }
				}
				
				// if (r==247 && g==243 & b==247) opacity = 64;
			}
		}

		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("getScaler for scale: %i\n", renderer->scale);
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	if (vid.use_game_gl) glClear(GL_COLOR_BUFFER_BIT);
	else SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

static void flipGameGl(void) {
	glViewport(0, 0, device_width, device_height);
	glClear(GL_COLOR_BUFFER_BIT);
	glActiveTexture(GL_TEXTURE0);

	if (!vid.blit) {
		uploadGameFrame(vid.screen->pixels, device_width, device_height, vid.screen->pitch);
		SDL_Rect full = {0, 0, device_width, device_height};
		drawGameTexture(vid.gl_game_texture, &full, device_width, device_height, &full, SCREEN_EFFECT_NONE, 0);
		SDL_GL_SwapWindow(vid.window);
		return;
	}

	uploadGameFrame(vid.blit->src, vid.blit->true_w, vid.blit->true_h, vid.blit->src_p);
	SDL_Rect src_rect = {vid.blit->src_x, vid.blit->src_y, vid.blit->src_w, vid.blit->src_h};
	const int crop_sfc_overscan =
		vid.screen_mask_type==SCREEN_MASK_SFC &&
		vid.blit->aspect==0 &&
		vid.blit->true_w==256 && vid.blit->true_h==239 &&
		vid.blit->src_x==0 && vid.blit->src_y==0 &&
		vid.blit->src_w==256 && vid.blit->src_h==239;
	const int crop_md_overscan =
		vid.screen_mask_type==SCREEN_MASK_MD &&
		vid.blit->aspect==0 &&
		vid.blit->true_w==320 && vid.blit->true_h==240 &&
		vid.blit->src_x==0 && vid.blit->src_y==0 &&
		vid.blit->src_w==320 && vid.blit->src_h==240;
	if (crop_sfc_overscan) {
		src_rect.y += 7;
		src_rect.h = 224;
	}
	else if (crop_md_overscan) {
		src_rect.y += 8;
		src_rect.h = 224;
	}
	SDL_Rect dst_rect = {0, 0, device_width, device_height};
	if (vid.blit->aspect==0) {
		dst_rect.w = vid.blit->src_w * vid.blit->scale;
		dst_rect.h = (crop_sfc_overscan || crop_md_overscan ? 224 : vid.blit->src_h) * vid.blit->scale;
		dst_rect.x = (device_width - dst_rect.w) / 2;
		dst_rect.y = (device_height - dst_rect.h) / 2;
	}
	else if (vid.blit->aspect>0) {
		dst_rect.h = device_height;
		dst_rect.w = dst_rect.h * vid.blit->aspect;
		if (dst_rect.w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			dst_rect.w = device_width;
			dst_rect.h = dst_rect.w * ratio;
		}
		dst_rect.x = (device_width - dst_rect.w) / 2;
		dst_rect.y = (device_height - dst_rect.h) / 2;
	}

	int mask_matches_game = 0;
	int effect_mode = SCREEN_EFFECT_NONE;
	if (vid.screen_mask_active && vid.screen_mask_type==SCREEN_MASK_GBA &&
		dst_rect.x==32 && dst_rect.y==64 && dst_rect.w==960 && dst_rect.h==640) {
		mask_matches_game = 1;
		effect_mode = SCREEN_EFFECT_GBA_APERTURE;
	}
	else if (vid.screen_mask_active && vid.screen_mask_type==SCREEN_MASK_SFC &&
		dst_rect.x==128 && dst_rect.y==48 && dst_rect.w==768 && dst_rect.h==672 &&
		src_rect.w==256 && src_rect.h==224) {
		mask_matches_game = 1;
		effect_mode = vid.crt_shader_mode;
	}
	else if (vid.screen_mask_active && vid.screen_mask_type==SCREEN_MASK_MD &&
		dst_rect.x==32 && dst_rect.y==48 && dst_rect.w==960 && dst_rect.h==672 &&
		src_rect.w==320 && src_rect.h==224) {
		mask_matches_game = 1;
		effect_mode = vid.crt_shader_mode;
	}
	if (effect_mode!=SCREEN_EFFECT_NONE && !vid.gl_effect_logged) {
		const char* effect_name = "gba-aperture";
		if (effect_mode!=SCREEN_EFFECT_GBA_APERTURE) {
			const int md = vid.screen_mask_type==SCREEN_MASK_MD;
			effect_name = effect_mode==SCREEN_EFFECT_SFC_COMPOSITE
				? (md ? "md-composite" : "sfc-composite")
				: (effect_mode==SCREEN_EFFECT_SFC_CRT
					? (md ? "md-crt" : "sfc-crt")
					: (md ? "md-sharp" : "sfc-sharp"));
		}
		LOG_info("Game shader active: %s viewport %d,%d %dx%d source %dx%d\n",
			effect_name, dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h, src_rect.w, src_rect.h);
		vid.gl_effect_logged = 1;
	}
	drawGameTexture(vid.gl_game_texture, &src_rect, vid.blit->true_w, vid.blit->true_h,
		&dst_rect, effect_mode, 0);

	if (mask_matches_game && vid.gl_overlay_texture) {
		SDL_Rect full = {0, 0, device_width, device_height};
		drawGameTexture(vid.gl_overlay_texture, &full, device_width, device_height, &full, SCREEN_EFFECT_NONE, 1);
	}

	SDL_GL_SwapWindow(vid.window);
	vid.blit = NULL;
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (vid.use_game_gl) {
		flipGameGl();
		return;
	}
	
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
	// uint32_t then = SDL_GetTicks();
	SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);
	// LOG_info("blit blocked for %ims (%i,%i)\n", SDL_GetTicks()-then,vid.buffer->w,vid.buffer->h);
	
	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	const int crop_sfc_overscan =
		vid.screen_mask_type==SCREEN_MASK_SFC &&
		vid.blit->aspect==0 &&
		vid.blit->true_w==256 && vid.blit->true_h==239 &&
		vid.blit->src_x==0 && vid.blit->src_y==0 &&
		vid.blit->src_w==256 && vid.blit->src_h==239;
	const int crop_md_overscan =
		vid.screen_mask_type==SCREEN_MASK_MD &&
		vid.blit->aspect==0 &&
		vid.blit->true_w==320 && vid.blit->true_h==240 &&
		vid.blit->src_x==0 && vid.blit->src_y==0 &&
		vid.blit->src_w==320 && vid.blit->src_h==240;
	if (crop_sfc_overscan) {
		// Snes9x can switch from the common 224-line frame to the 239-line
		// overscan mode after startup. Keep the Brick's 3x 256x224 viewport
		// stable by removing 7 lines above and 8 below instead of stretching
		// the CRT aperture or disabling the console mask.
		y += 7;
		h = 224;
	}
	else if (crop_md_overscan) {
		y += 8;
		h = 224;
	}
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};
	if (vid.blit->aspect==0) { // native or cropped
		// LOG_info("src_rect %i,%i %ix%i\n",src_rect->x,src_rect->y,src_rect->w,src_rect->h);
		
		int w = vid.blit->src_w * vid.blit->scale;
		int h = (crop_sfc_overscan || crop_md_overscan ? 224 : vid.blit->src_h) * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
						
		// LOG_info("dst_rect %i,%i %ix%i\n",dst_rect->x,dst_rect->y,dst_rect->w,dst_rect->h);
	}
	else if (vid.blit->aspect>0) { // aspect
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	
	SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);

	int mask_matches_game = 0;
	if (vid.screen_mask_active) {
		if (vid.screen_mask_type==SCREEN_MASK_GBA) {
			mask_matches_game = dst_rect->x==32 && dst_rect->y==64 && dst_rect->w==960 && dst_rect->h==640;
		}
		else if (vid.screen_mask_type==SCREEN_MASK_GB) {
			mask_matches_game = dst_rect->x==112 && dst_rect->y==24 && dst_rect->w==800 && dst_rect->h==720;
		}
		else if (vid.screen_mask_type==SCREEN_MASK_GBC) {
			mask_matches_game = dst_rect->x==112 && dst_rect->y==24 && dst_rect->w==800 && dst_rect->h==720;
		}
		else if (vid.screen_mask_type==SCREEN_MASK_SFC) {
			mask_matches_game = dst_rect->x==128 && dst_rect->y==48 && dst_rect->w==768 && dst_rect->h==672;
		}
		else if (vid.screen_mask_type==SCREEN_MASK_MD) {
			mask_matches_game = dst_rect->x==32 && dst_rect->y==48 && dst_rect->w==960 && dst_rect->h==672;
		}
	}
	if (mask_matches_game && vid.screen_mask) {
		SDL_RenderCopy(vid.renderer, vid.screen_mask, NULL, NULL);
	}
	
	updateEffect();
	int lcd_active = mask_matches_game && vid.lcd_mask;
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect && !lcd_active) {
		SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,dst_rect->w,dst_rect->h}, dst_rect);
	}
	if (lcd_active) {
		SDL_RenderCopy(vid.renderer, vid.lcd_mask, NULL, dst_rect);
	}
	// uint32_t then = SDL_GetTicks();
	SDL_RenderPresent(vid.renderer);
	// LOG_info("SDL_RenderPresent blocked for %ims\n", SDL_GetTicks()-then);
	vid.blit = NULL;
}

///////////////////////////////

// TODO: 
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// // wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define LED_PATH1 "/sys/class/led_anim/max_scale"
#define LED_PATH2 "/sys/class/led_anim/max_scale_lr"
#define LED_PATH3 "/sys/class/led_anim/max_scale_f1f2" // front facing
static void PLAT_enableLED(int enable) {
	if (enable) {
		putInt(LED_PATH1,60);
		if (is_brick) putInt(LED_PATH2,60);
		if (is_brick) putInt(LED_PATH3,60);
	}
	else {
		putInt(LED_PATH1,0);
		if (is_brick) putInt(LED_PATH2,0);
		if (is_brick) putInt(LED_PATH3,0);
	}
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		// putInt(BLANK_PATH,0);
		if (is_brick) SetRawBrightness(8);
		SetBrightness(GetBrightness());
	}
	else {
		// putInt(BLANK_PATH,4);
		SetRawBrightness(0);
	}
	PLAT_enableLED(!enable);
}

void PLAT_powerOff(void) {
	// break the MinUI.pak/launch.sh while loop
	unlink("/tmp/minui_exec");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	PLAT_enableLED(1);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	
	exit(0); // poweroff handled by PLATFORM/bin/shutdown
}

///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1200000; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; break;
		case CPU_SPEED_PERFORMANCE: freq = 2000000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"
void PLAT_setRumble(int strength) {
	putInt(RUMBLE_PATH, (strength && !GetMute())?1:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	char* model = getenv("TRIMUI_MODEL");
	if (model) return model;
	return "Trimui Smart Pro";
}

int PLAT_isOnline(void) {
	return online;
}
