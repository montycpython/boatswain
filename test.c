#include <android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"Bosun",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"Bosun",__VA_ARGS__)

// ---------- GL TRACE ----------
#define GLCHK(x) do{ x; GLenum e=glGetError(); if(e) LOGE("GL err %x @ %s:%d",e,__FILE__,__LINE__);}while(0)

// ---------- JNI ----------
static pthread_key_t jni_env_key;
static JavaVM* g_vm;

JNIEnv* get_jni_env(){
    if(!g_vm) return NULL;
    JNIEnv* env=pthread_getspecific(jni_env_key);
    if(!env){
        if((*g_vm)->AttachCurrentThread(g_vm,&env,NULL)!=JNI_OK) return NULL;
        pthread_setspecific(jni_env_key,env);
    }
    return env;
}

void detach_thread_env(void* v){
    if(v && g_vm) (*g_vm)->DetachCurrentThread(g_vm);
}

// ---------- JNI CALLS TO KOTLIN ------
void toggle_keyboard(struct android_app* app, int show){
    JNIEnv* env = get_jni_env();
    if(!env) return;

    jobject activity = app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    if(!cls){
        LOGE("No activity class");
        return;
    }

    jmethodID mid = (*env)->GetMethodID(env, cls, "toggleKeyboard", "(Z)V");
    if(!mid){
        LOGE("toggleKeyboard(Z) not found");
        return;
    }

    (*env)->CallVoidMethod(env, activity, mid, show ? JNI_TRUE : JNI_FALSE);
}

//----------- dialog ------
void show_dialog(struct android_app* app){
    JNIEnv* env = get_jni_env();
    if(!env) {
        LOGE("JNI env null");
        return;
    }

    jobject activity = app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    if(!cls){
        LOGE("Activity class not found");
        return;
    }

    jmethodID mid = (*env)->GetMethodID(env, cls, "showBoatswainDialog", "()V");
    if(!mid){
        LOGE("showBoatswainDialog not found");
        return;
    }

    (*env)->CallVoidMethod(env, activity, mid);
}
//----------- dialog ------
// ---------- GL ----------
static EGLDisplay display;
static EGLSurface surface;
static EGLContext context;

static GLuint prog,tex,vbo;
static int initialized=0;
static float time_sec=0;

// overlay rect (NDC)
static float ov_x=0.0f;
static float ov_y=0.0f;
static float ov_w=0.6f;
static float ov_h=0.6f;

// ---------- SHADERS ----------
static const char* vs_src=
"#version 300 es\n"
"layout(location=0)in vec2 pos;"
"layout(location=1)in vec2 uv;"
"out vec2 vUV;"
"void main(){gl_Position=vec4(pos,0,1);vUV=uv;}";

static const char* fs_src=
"#version 300 es\n"
"precision mediump float;"
"uniform sampler2D uTex;"
"in vec2 vUV;"
"out vec4 frag;"
"void main(){vec4 t=texture(uTex,vUV);if(t.a<0.1)discard;frag=t;}";

GLuint make_shader(GLenum t,const char* s){
    GLuint sh=glCreateShader(t);
    glShaderSource(sh,1,&s,NULL);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){LOGE("shader fail");glDeleteShader(sh);return 0;}
    return sh;
}

GLuint make_prog(){
    GLuint vs=make_shader(GL_VERTEX_SHADER,vs_src);
    GLuint fs=make_shader(GL_FRAGMENT_SHADER,fs_src);
    GLuint p=glCreateProgram();
    glAttachShader(p,vs);
    glAttachShader(p,fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){LOGE("link fail");return 0;}
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

// ---------- TEXTURE ----------
void load_overlay(struct android_app* app){
    AAsset* a=AAssetManager_open(app->activity->assetManager,"overlay.png",AASSET_MODE_STREAMING);
    if(!a){LOGE("no overlay");return;}

    int len=AAsset_getLength(a);
    unsigned char* buf=malloc(len);
    AAsset_read(a,buf,len);

    int w,h,n;
    unsigned char* img=stbi_load_from_memory(buf,len,&w,&h,&n,4);
    free(buf);
    AAsset_close(a);

    if(!img){LOGE("stb fail");return;}

    GLCHK(glGenTextures(1,&tex));
    GLCHK(glBindTexture(GL_TEXTURE_2D,tex));
    GLCHK(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR));
    GLCHK(glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR));
    GLCHK(glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,img));
    stbi_image_free(img);
}

// ---------- GL INIT ----------
void init_gl(struct android_app* app){
    if(!app->window) return;

    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display,0,0);

    EGLint attr[]={
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,
        EGL_NONE};
    EGLConfig cfg; EGLint n;
    eglChooseConfig(display,attr,&cfg,1,&n);

    surface=eglCreateWindowSurface(display,cfg,app->window,NULL);

    EGLint ctxattr[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context=eglCreateContext(display,cfg,NULL,ctxattr);
    eglMakeCurrent(display,surface,surface,context);

    int w=ANativeWindow_getWidth(app->window);
    int h=ANativeWindow_getHeight(app->window);
    GLCHK(glViewport(0,0,w,h));

    prog=make_prog();
    GLCHK(glUseProgram(prog));
    GLCHK(glUniform1i(glGetUniformLocation(prog,"uTex"),0));

    float verts[16]={
        -ov_w/2, ov_h/2, 0,0,
        -ov_w/2,-ov_h/2, 0,1,
         ov_w/2, ov_h/2, 1,0,
         ov_w/2,-ov_h/2, 1,1
    };

    GLCHK(glGenBuffers(1,&vbo));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER,vbo));
    GLCHK(glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW));

    load_overlay(app);

    initialized=1;
    LOGI("GL ready");
}

// ---------- DRAW ----------
void draw(){
    if(!initialized) return;

    time_sec+=0.02f;
    GLCHK(glClearColor((sinf(time_sec)+1)/2,0,(cosf(time_sec)+1)/2,1));
    GLCHK(glClear(GL_COLOR_BUFFER_BIT));

    GLCHK(glUseProgram(prog));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER,vbo));

    GLCHK(glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0));
    GLCHK(glEnableVertexAttribArray(0));
    GLCHK(glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))));
    GLCHK(glEnableVertexAttribArray(1));

    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D,tex));

    GLCHK(glDrawArrays(GL_TRIANGLE_STRIP,0,4));
    eglSwapBuffers(display,surface);
}
//------------$$$$$$$$$$$$
void show_native_edit(struct android_app* app, float x_px, float y_px, const char* text) {
    JNIEnv* env = get_jni_env();
    if (!env) return;

    jobject activity = app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "showEditableText", "(FFLjava/lang/String;)V");

    if (mid) {
        jstring jtext = (*env)->NewStringUTF(env, text);
        (*env)->CallVoidMethod(env, activity, mid, (jfloat)x_px, (jfloat)y_px, jtext);
        (*env)->DeleteLocalRef(env, jtext);
    }
}
//------------>>>>>>>>>>>>
void get_native_text(struct android_app* app, char* out_buf, int max_len) {
    JNIEnv* env = get_jni_env();
    if (!env) return;

    jobject activity = app->activity->clazz;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "getTypedText", "()Ljava/lang/String;");

    if (mid) {
        jstring jstr = (*env)->CallObjectMethod(env, activity, mid);
        const char* str = (*env)->GetStringUTFChars(env, jstr, NULL);
        if (str) {
            snprintf(out_buf, max_len, "%s", str);
            (*env)->ReleaseStringUTFChars(env, jstr, str);
        }
        (*env)->DeleteLocalRef(env, jstr);
    }
}

// ---------- TOUCH ----------
int handle_input(struct android_app* app,AInputEvent* e){
    if(AInputEvent_getType(e)!=AINPUT_EVENT_TYPE_MOTION) return 0;

    float x=AMotionEvent_getX(e,0);
    float y=AMotionEvent_getY(e,0);

    int w=ANativeWindow_getWidth(app->window);
    int h=ANativeWindow_getHeight(app->window);

    float nx=(x/(float)w)*2-1;
    float ny=1-(y/(float)h)*2;

    float left=ov_x-ov_w/2;
    float right=ov_x+ov_w/2;
    float top=ov_y+ov_h/2;
    float bottom=ov_y-ov_h/2;

    if(nx>left && nx<right && ny>bottom && ny<top){
        LOGI("overlay touched");
        show_dialog(app);
        static int kb=0;
        kb=!kb;
        //toggle_keyboard(app,kb);   // JNI → Kotlin
        // ------
        float pixel_x = ((ov_x - ov_w / 2.0f) + 1.0f) / 2.0f * w;
        float pixel_y = (1.0f - (ov_y + ov_h / 2.0f)) / 2.0f * h;

        show_native_edit(app, pixel_x, pixel_y, "Enter text...");
        // -----------------------

    }

    return 1;
}

// ---------- CMD ----------
void handle_cmd(struct android_app* app,int32_t cmd){
    if(cmd==APP_CMD_INIT_WINDOW && app->window && !initialized){
        init_gl(app);
    }
}

// ---------- MAIN ----------
void android_main(struct android_app* state){
    g_vm=state->activity->vm;
    pthread_key_create(&jni_env_key,detach_thread_env);

    state->onAppCmd=handle_cmd;
    state->onInputEvent=handle_input;

    while(1){
        int id,events;
        struct android_poll_source* src;
        while((id=ALooper_pollOnce(initialized?0:-1,NULL,&events,(void**)&src))>=0){
            if(src) src->process(state,src);
            if(state->destroyRequested) return;
        }
        draw();
    }
}
