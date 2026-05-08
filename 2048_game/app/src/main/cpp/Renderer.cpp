#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}

#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) {\
    aout << extension << "\n";\
}\
aout << std::endl;\
}

#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;
out vec2 fragUV;
uniform mat4 uProjection;
uniform mat4 uModel;
uniform vec2 uUVOffset;
uniform vec2 uUVScale;
void main() {
    fragUV = inUV * uUVScale + uUVOffset;
    gl_Position = uProjection * uModel * vec4(inPosition, 1.0);
}
)vertex";

static const char *fragment = R"fragment(#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
uniform vec4 uColor;
out vec4 outColor;
void main() {
    outColor = texture(uTexture, fragUV) * uColor;
}
)fragment";

static constexpr float kProjectionHalfHeight = 2.f;
static constexpr float kProjectionNearPlane = -1.f;
static constexpr float kProjectionFarPlane = 1.f;

Renderer::~Renderer() {
    if (whiteTexture_) glDeleteTextures(1, &whiteTexture_);
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::render() {
    updateRenderArea();

    if (shaderNeedsNewProjectionMatrix_) {
        float projectionMatrix[16] = {0};
        Utility::buildOrthographicMatrix(
                projectionMatrix,
                kProjectionHalfHeight,
                float(width_) / height_,
                kProjectionNearPlane,
                kProjectionFarPlane);
        shader_->setProjectionMatrix(projectionMatrix);
        shaderNeedsNewProjectionMatrix_ = false;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (models_.empty()) return;

    const auto &grid = gameLogic_.getGrid();
    float tileSize = 0.8f;
    float spacing = 0.1f;
    float offset = (4.0f * tileSize + 3.0f * spacing) / 2.0f - tileSize / 2.0f;

    float projectionMatrix[16] = {0};
    Utility::buildOrthographicMatrix(
            projectionMatrix,
            kProjectionHalfHeight,
            float(width_) / height_,
            kProjectionNearPlane,
            kProjectionFarPlane);

    // スコアの描画
    if (fontRenderer_) {
        std::string scoreText = "Score: " + std::to_string(gameLogic_.getScore());
        // 画面上部に配置
        shader_->setVec4("uColor", 0.3f, 0.3f, 0.3f, 1.0f);
        fontRenderer_->renderText(scoreText, 0.0f, 1.5f, 0.3f, projectionMatrix, *shader_);
    }

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int value = grid[r][c];
            float x = (c * (tileSize + spacing)) - offset;
            float y = ( (3 - r) * (tileSize + spacing)) - offset;

            float modelMatrix[16];
            Utility::buildIdentityMatrix(modelMatrix);
            modelMatrix[0] = tileSize / 2.0f;
            modelMatrix[5] = tileSize / 2.0f;
            modelMatrix[12] = x;
            modelMatrix[13] = y;

            shader_->activate();
            shader_->setModelMatrix(modelMatrix);
            shader_->setVec2("uUVOffset", 0.0f, 0.0f);
            shader_->setVec2("uUVScale", 1.0f, 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, whiteTexture_);

            float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            if (value == 0) {
                color[0] = 0.8f; color[1] = 0.8f; color[2] = 0.8f;
            } else {
                switch (value) {
                    case 2:    color[0] = 0.93f; color[1] = 0.89f; color[2] = 0.85f; break;
                    case 4:    color[0] = 0.93f; color[1] = 0.88f; color[2] = 0.78f; break;
                    case 8:    color[0] = 0.95f; color[1] = 0.69f; color[2] = 0.47f; break;
                    case 16:   color[0] = 0.96f; color[1] = 0.58f; color[2] = 0.39f; break;
                    case 32:   color[0] = 0.96f; color[1] = 0.48f; color[2] = 0.37f; break;
                    case 64:   color[0] = 0.96f; color[1] = 0.37f; color[2] = 0.23f; break;
                    case 128:  color[0] = 0.93f; color[1] = 0.80f; color[2] = 0.45f; break;
                    case 256:  color[0] = 0.93f; color[1] = 0.80f; color[2] = 0.45f; break;
                    case 512:  color[0] = 0.93f; color[1] = 0.80f; color[2] = 0.45f; break;
                    case 1024: color[0] = 0.93f; color[1] = 0.80f; color[2] = 0.45f; break;
                    case 2048: color[0] = 0.93f; color[1] = 0.80f; color[2] = 0.45f; break;
                    default:   color[0] = 0.24f; color[1] = 0.23f; color[2] = 0.19f; break;
                }
            }
            shader_->setColor(color);
            shader_->drawModel(models_[0]);

            if (value > 0 && fontRenderer_) {
                float textColor[4] = {0.3f, 0.3f, 0.3f, 1.0f};
                if (value >= 8) {
                    textColor[0] = 0.97f; textColor[1] = 0.96f; textColor[2] = 0.94f;
                }
                shader_->setVec4("uColor", textColor[0], textColor[1], textColor[2], textColor[3]);
                fontRenderer_->renderText(std::to_string(value), x, y, tileSize * 0.4f, projectionMatrix, *shader_);
            }
        }
    }

    if (gameLogic_.isGameOver() && fontRenderer_) {
        // 背景を少し暗くする
        float overlayColor[4] = {1.0f, 1.0f, 1.0f, 0.5f};
        shader_->activate();
        float modelMatrix[16];
        Utility::buildIdentityMatrix(modelMatrix);
        modelMatrix[0] = 2.0f; // 画面全体
        modelMatrix[5] = 2.0f;
        shader_->setModelMatrix(modelMatrix);
        shader_->setColor(overlayColor);
        shader_->setVec2("uUVOffset", 0.0f, 0.0f);
        shader_->setVec2("uUVScale", 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        shader_->drawModel(models_[0]);

        shader_->setVec4("uColor", 0.3f, 0.3f, 0.3f, 1.0f);
        fontRenderer_->renderText("Game Over!", 0.0f, 0.0f, 0.5f, projectionMatrix, *shader_);
    }

    auto swapResult = eglSwapBuffers(display_, surface_);
    assert(swapResult == EGL_TRUE);
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    auto config = *std::find_if(supportedConfigs.get(), supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                return eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)
                    && red == 8 && green == 8 && blue == 8 && depth == 24;
            });

    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    eglMakeCurrent(display, surface, surface, context);

    display_ = display; surface_ = surface; context_ = context;
    width_ = -1; height_ = -1;

    shader_ = std::unique_ptr<Shader>(Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection", "uModel", "uColor"));
    shader_->activate();

    glClearColor(CORNFLOWER_BLUE);

    uint8_t white[] = {255, 255, 255, 255};
    glGenTextures(1, &whiteTexture_);
    glBindTexture(GL_TEXTURE_2D, whiteTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    fontRenderer_ = FontRenderer::create(app_->activity->vm, app_->activity->javaGameActivity);

    createModels();
}

void Renderer::updateRenderArea() {
    EGLint width, height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width; height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::createModels() {
    std::vector<Vertex> vertices = {
            Vertex(Vector3{-1.0f, -1.0f, 0.0f}, Vector2{0.0f, 1.0f}), // 左下
            Vertex(Vector3{ 1.0f, -1.0f, 0.0f}, Vector2{1.0f, 1.0f}), // 右下
            Vertex(Vector3{ 1.0f,  1.0f, 0.0f}, Vector2{1.0f, 0.0f}), // 右上
            Vertex(Vector3{-1.0f,  1.0f, 0.0f}, Vector2{0.0f, 0.0f})  // 左上
    };
    std::vector<Index> indices = { 0, 1, 2, 0, 2, 3 };
    models_.emplace_back(vertices, indices, nullptr);
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;

    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                lastTouchX_ = x; lastTouchY_ = y; isTouching_ = true; break;
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                if (isTouching_) {
                    float dx = x - lastTouchX_, dy = y - lastTouchY_;
                    if (std::max(std::abs(dx), std::abs(dy)) > 100.0f) {
                        if (std::abs(dx) > std::abs(dy)) {
                            if (dx > 0) gameLogic_.move(MoveDirection::RIGHT);
                            else gameLogic_.move(MoveDirection::LEFT);
                        } else {
                            if (dy > 0) gameLogic_.move(MoveDirection::DOWN);
                            else gameLogic_.move(MoveDirection::UP);
                        }
                    }
                    isTouching_ = false;
                }
                break;
        }
    }
    android_app_clear_motion_events(inputBuffer);
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        if (keyEvent.action == AKEY_EVENT_ACTION_DOWN) {
            switch (keyEvent.keyCode) {
                case AKEYCODE_DPAD_UP: gameLogic_.move(MoveDirection::UP); break;
                case AKEYCODE_DPAD_DOWN: gameLogic_.move(MoveDirection::DOWN); break;
                case AKEYCODE_DPAD_LEFT: gameLogic_.move(MoveDirection::LEFT); break;
                case AKEYCODE_DPAD_RIGHT: gameLogic_.move(MoveDirection::RIGHT); break;
            }
        }
    }
    android_app_clear_key_events(inputBuffer);
}
