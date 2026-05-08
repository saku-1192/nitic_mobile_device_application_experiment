#include "FontRenderer.h"
#include <string>
#include <vector>
#include <android/bitmap.h>
#include "Utility.h"
#include "AndroidOut.h"

std::unique_ptr<FontRenderer> FontRenderer::create(JavaVM* vm, jobject activity) {
    return std::unique_ptr<FontRenderer>(new FontRenderer(vm, activity));
}

FontRenderer::FontRenderer(JavaVM* vm, jobject activity) : vm_(vm) {
    aout << "FontRenderer constructor start" << std::endl;
    JNIEnv* env = getEnv();
    activityRef_ = env->NewGlobalRef(activity);
    jclass localClass = env->GetObjectClass(activityRef_);
    classRef_ = (jclass)env->NewGlobalRef(localClass);
    renderMethod_ = env->GetStaticMethodID(classRef_, "renderTextToBitmap", "(Ljava/lang/String;F)Landroid/graphics/Bitmap;");
    if (!renderMethod_) {
        aout << "Error: Could not find renderTextToBitmap method!" << std::endl;
    }
    aout << "FontRenderer constructor end" << std::endl;
}

FontRenderer::~FontRenderer() {
    JNIEnv* env = getEnv();
    for (auto& pair : textureCache_) {
        glDeleteTextures(1, &pair.second.textureId);
    }
    if (classRef_) {
        env->DeleteGlobalRef(classRef_);
    }
    if (activityRef_) {
        env->DeleteGlobalRef(activityRef_);
    }
}

JNIEnv* FontRenderer::getEnv() {
    JNIEnv* env = nullptr;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        vm_->AttachCurrentThread(&env, nullptr);
    }
    return env;
}

TextTexture FontRenderer::getOrCreateTextTexture(const std::string& text) {
    auto it = textureCache_.find(text);
    if (it != textureCache_.end()) {
        return it->second;
    }

    aout << "Creating texture for: " << text << std::endl;
    JNIEnv* env = getEnv();
    jstring jstr = env->NewStringUTF(text.c_str());
    jobject bitmap = env->CallStaticObjectMethod(classRef_, renderMethod_, jstr, 80.0f);
    if (!bitmap) {
        aout << "Error: Bitmap generation failed for " << text << std::endl;
        env->DeleteLocalRef(jstr);
        return {0, 0, 0};
    }
    aout << "Bitmap created for: " << text << std::endl;

    AndroidBitmapInfo info;
    AndroidBitmap_getInfo(env, bitmap, &info);

    void* pixels;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        aout << "Error: AndroidBitmap_lockPixels failed" << std::endl;
        env->DeleteLocalRef(jstr);
        env->DeleteLocalRef(bitmap);
        return {0, 0, 0};
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.width, info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    AndroidBitmap_unlockPixels(env, bitmap);
    env->DeleteLocalRef(jstr);
    env->DeleteLocalRef(bitmap);

    TextTexture tt = {tex, (int)info.width, (int)info.height};
    textureCache_[text] = tt;
    aout << "Texture generated for: " << text << " ID: " << tex << std::endl;
    return tt;
}

void FontRenderer::renderText(const std::string& text, float centerX, float centerY, float size, const float* projection, Shader& shader) {
    TextTexture tt = getOrCreateTextTexture(text);
    if (tt.textureId == 0) return;

    float aspect = (float)tt.width / (float)tt.height;
    float h = size;
    float w = h * aspect;

    struct Vertex {
        float pos[3];
        float uv[2];
    };

    Vertex vertices[] = {
        {{-w/2, -h/2, 0.0f}, {0.0f, 1.0f}}, // 左下
        {{ w/2, -h/2, 0.0f}, {1.0f, 1.0f}}, // 右下
        {{ w/2,  h/2, 0.0f}, {1.0f, 0.0f}}, // 右上
        {{-w/2,  h/2, 0.0f}, {0.0f, 0.0f}}  // 左上
    };
    uint16_t indices[] = {0, 1, 2, 0, 2, 3};

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tt.textureId);

    float model[16];
    Utility::buildIdentityMatrix(model);
    model[12] = centerX;
    model[13] = centerY;
    shader.setProjectionMatrix(projection);
    shader.setModelMatrix(model);
    shader.setVec2("uUVOffset", 0.0f, 0.0f);
    shader.setVec2("uUVScale", 1.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);

    GLint posAttrib = shader.getPositionAttrib();
    GLint uvAttrib = shader.getUvAttrib();
    glEnableVertexAttribArray(posAttrib);
    glEnableVertexAttribArray(uvAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].pos);
    glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), &vertices[0].uv);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);

    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(uvAttrib);

    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}
