#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

#include <GLES3/gl3.h>
#include <vector>
#include <memory>
#include <map>
#include <string>
#include <jni.h>
#include "Shader.h"

struct TextTexture {
    GLuint textureId;
    int width;
    int height;
};

class FontRenderer {
public:
    static std::unique_ptr<FontRenderer> create(JavaVM* vm, jobject activity);
    ~FontRenderer();
    void renderText(const std::string& text, float centerX, float centerY, float size, const float* projection, Shader& shader);

private:
    FontRenderer(JavaVM* vm, jobject activity);
    TextTexture getOrCreateTextTexture(const std::string& text);
    JNIEnv* getEnv();

    JavaVM* vm_;
    jobject activityRef_;
    jclass classRef_;
    jmethodID renderMethod_;

    std::map<std::string, TextTexture> textureCache_;
};

#endif
