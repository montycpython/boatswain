#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/asset_manager.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <jni.h>
#include <math.h>

EGLDisplay display;
EGLSurface surface;
EGLContext context;
int g_initialized = 0;
float g_time = 0.0f;

GLuint g_program;
GLuint g_texture;
GLuint g_vbo;

const char* vShader = 
    "#version 300 es\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec2 tex;\n"
    "out vec2 vTex;\n"
    "void main() {\n"
    "   gl_Position = vec4(pos, 0.0, 1.0);\n"
    "   vTex = tex;\n"
    "}\n";

const char* fShader = 
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uTex;\n"
    "in vec2 vTex;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "   vec4 texColor = texture(uTex, vTex);\n"
    "   if(texColor.a < 0.1) discard;\n" // Handle transparency
    "   color = texColor;\n"
    "}\n";

void init_gl(struct android_app* app) {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);
    const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    surface = eglCreateWindowSurface(display, config, app->window, NULL);
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context = eglCreateContext(display, config, NULL, contextAttribs);
    eglMakeCurrent(display, surface, surface, context);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vShader, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fShader, NULL);
    glCompileShader(fs);

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);

    float vertices[] = {
        -0.5f,  0.5f, 0, 0,
        -0.5f, -0.5f, 0, 1,
         0.5f,  0.5f, 1, 0,
         0.5f, -0.5f, 1, 1 
    };

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    AAsset* asset = AAssetManager_open(app->activity->assetManager, "overlay.png", AASSET_MODE_BUFFER);
    if(asset) {
        off_t length = AAsset_getLength(asset);
        unsigned char* buffer = (unsigned char*)AAsset_getBuffer(asset);
        int w, h, n;
        unsigned char* data = stbi_load_from_memory(buffer, length, &w, &h, &n, 4);
        glGenTextures(1, &g_texture);
        glBindTexture(GL_TEXTURE_2D, g_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        AAsset_close(asset);
    }
    g_initialized = 1;
}

void draw_frame() {
    if (!g_initialized) return;
    g_time += 0.02f;
    glClearColor((sinf(g_time)+1.0f)/2.0f, 0.0f, (cosf(g_time)+1.0f)/2.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(display, surface);
}

void handle_cmd(struct android_app* app, int32_t cmd) {
    if (cmd == APP_CMD_INIT_WINDOW) init_gl(app);
    if (cmd == APP_CMD_TERM_WINDOW) g_initialized = 0;
}

void android_main(struct android_app* state) {
    state->onAppCmd = handle_cmd;
    while (1) {
        int ident, events;
        struct android_poll_source* source;
        while ((ident = ALooper_pollOnce(g_initialized ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        if (g_initialized) draw_frame();
    }
}

