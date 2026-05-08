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

#define BACKGROUND_COLOR 250 / 255.f, 248 / 255.f, 239 / 255.f, 1 // #FAF8EF

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

    float aspect = float(width_) / height_;
    float proj[16];
    Utility::buildOrthographicMatrix(
            proj,
            kProjectionHalfHeight,
            aspect,
            kProjectionNearPlane,
            kProjectionFarPlane);

    shader_->activate();
    shader_->setProjectionMatrix(proj);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (models_.empty()) return;

    // --- 1. 定数とレイアウト計算 ---
    float screenWidth = 2.0f * kProjectionHalfHeight * aspect;
    float boardWidth = std::min(screenWidth * 0.92f, 2.8f);
    float spacingRatio = 0.12f; // タイルサイズに対する隙間の割合
    float tileSize = boardWidth / (4.0f + 5.0f * spacingRatio);
    float spacing = tileSize * spacingRatio;
    float gridWidth = 4.0f * tileSize + 3.0f * spacing;
    float totalBoardWidth = 4.0f * tileSize + 5.0f * spacing;
    float boardCenterY = -0.4f;

    // --- 2. 盤面の背景を描画 ---
    {
        float bgModel[16];
        Utility::buildIdentityMatrix(bgModel);
        bgModel[0] = totalBoardWidth / 2.0f;
        bgModel[5] = totalBoardWidth / 2.0f;
        bgModel[12] = 0.0f;
        bgModel[13] = boardCenterY;
        shader_->setModelMatrix(bgModel);
        float bgColor[4] = {0.733f, 0.678f, 0.627f, 1.0f}; // #bbada0
        shader_->setColor(bgColor);
        glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        shader_->drawModel(models_[0]);
    }

    // --- 3. タイルの描画 ---
    const auto &grid = gameLogic_.getGrid();
    float offset = (gridWidth - tileSize) / 2.0f;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int value = grid[r][c];
            float x = (c * (tileSize + spacing)) - offset;
            float y = ( (3 - r) * (tileSize + spacing)) - offset + boardCenterY;

            float modelMatrix[16];
            Utility::buildIdentityMatrix(modelMatrix);
            modelMatrix[0] = tileSize / 2.0f;
            modelMatrix[5] = tileSize / 2.0f;
            modelMatrix[12] = x;
            modelMatrix[13] = y;

            shader_->setModelMatrix(modelMatrix);
            shader_->setVec2("uUVOffset", 0, 0);
            shader_->setVec2("uUVScale", 1, 1);

            float tileColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            if (value == 0) {
                tileColor[0] = 0.804f; tileColor[1] = 0.757f; tileColor[2] = 0.706f; // #cdc1b4
            } else {
                switch (value) {
                    case 2:    tileColor[0] = 0.933f; tileColor[1] = 0.894f; tileColor[2] = 0.855f; break; // #eee4da
                    case 4:    tileColor[0] = 0.929f; tileColor[1] = 0.878f; tileColor[2] = 0.784f; break; // #ede0c8
                    case 8:    tileColor[0] = 0.949f; tileColor[1] = 0.694f; tileColor[2] = 0.475f; break; // #f2b179
                    case 16:   tileColor[0] = 0.961f; tileColor[1] = 0.584f; tileColor[2] = 0.388f; break; // #f59563
                    case 32:   tileColor[0] = 0.965f; tileColor[1] = 0.486f; tileColor[2] = 0.373f; break; // #f67c5f
                    case 64:   tileColor[0] = 0.965f; tileColor[1] = 0.369f; tileColor[2] = 0.231f; break; // #f65e3b
                    case 128:  tileColor[0] = 0.929f; tileColor[1] = 0.812f; tileColor[2] = 0.447f; break; // #edcf72
                    case 256:  tileColor[0] = 0.929f; tileColor[1] = 0.800f; tileColor[2] = 0.380f; break; // #edcc61
                    case 512:  tileColor[0] = 0.929f; tileColor[1] = 0.784f; tileColor[2] = 0.314f; break; // #edc850
                    case 1024: tileColor[0] = 0.929f; tileColor[1] = 0.773f; tileColor[2] = 0.247f; break; // #edc53f
                    case 2048: tileColor[0] = 0.929f; tileColor[1] = 0.761f; tileColor[2] = 0.180f; break; // #edc22e
                    default:   tileColor[0] = 0.235f; tileColor[1] = 0.227f; tileColor[2] = 0.196f; break; // #3c3a32
                }
            }
            shader_->setColor(tileColor);
            glBindTexture(GL_TEXTURE_2D, whiteTexture_);
            shader_->drawModel(models_[0]);

            if (value > 0 && fontRenderer_) {
                float textColor[4] = {0.467f, 0.431f, 0.396f, 1.0f}; // #776e65
                if (value >= 8) {
                    textColor[0] = 0.976f; textColor[1] = 0.965f; textColor[2] = 0.949f; // #f9f6f2
                }
                shader_->setVec4("uColor", textColor[0], textColor[1], textColor[2], textColor[3]);
                fontRenderer_->renderText(std::to_string(value), x, y, tileSize * 0.5f, proj, *shader_);
            }
        }
    }

    // --- 4. ゲームオーバー表示 (オーバーレイ) ---
    if (gameLogic_.isGameOver() && fontRenderer_) {
        float overlayColor[4] = {1.0f, 1.0f, 1.0f, 0.6f};
        float modelMatrix[16];
        Utility::buildIdentityMatrix(modelMatrix);
        modelMatrix[0] = 2.0f; modelMatrix[5] = 2.0f;
        shader_->setModelMatrix(modelMatrix);
        shader_->setColor(overlayColor);
        glBindTexture(GL_TEXTURE_2D, whiteTexture_);
        shader_->drawModel(models_[0]);

        shader_->setVec4("uColor", 0.3f, 0.3f, 0.3f, 1.0f);
        fontRenderer_->renderText("Game Over!", 0.0f, 0.3f, 0.5f, proj, *shader_);
        fontRenderer_->renderText("Tap to Restart", 0.0f, -0.3f, 0.2f, proj, *shader_);
    }

    // --- 5. UIエリア ---
    if (fontRenderer_) {
        glDisable(GL_DEPTH_TEST);
        float uiTopY = 1.5f;
        // タイトル "2048"
        shader_->setVec4("uColor", 0.467f, 0.431f, 0.396f, 1.0f);
        fontRenderer_->renderText("2048", -screenWidth * 0.22f, uiTopY, 0.55f, proj, *shader_);

        // スコア表示ボックス
        float scoreBoxW = 0.32f, scoreBoxH = 0.18f;
        float scoreX = screenWidth * 0.25f, scoreY = uiTopY;
        {
            float m[16]; Utility::buildIdentityMatrix(m);
            m[0] = scoreBoxW; m[5] = scoreBoxH; m[12] = scoreX; m[13] = scoreY;
            shader_->setModelMatrix(m);
            float c[4] = {0.733f, 0.678f, 0.627f, 1.0f};
            shader_->setColor(c);
            glBindTexture(GL_TEXTURE_2D, whiteTexture_);
            shader_->drawModel(models_[0]);
        }
        shader_->setVec4("uColor", 0.933f, 0.894f, 0.855f, 1.0f);
        fontRenderer_->renderText("SCORE", scoreX, scoreY + 0.06f, 0.09f, proj, *shader_);
        fontRenderer_->renderText(std::to_string(gameLogic_.getScore()), scoreX, scoreY - 0.03f, 0.16f, proj, *shader_);

        // NEW GAME ボタン
        float btnW = 0.28f, btnH = 0.08f;
        float btnX = screenWidth * 0.25f, btnY = 1.20f;
        {
            float m[16]; Utility::buildIdentityMatrix(m);
            m[0] = btnW; m[5] = btnH; m[12] = btnX; m[13] = btnY;
            shader_->setModelMatrix(m);
            float c[4] = {0.561f, 0.478f, 0.396f, 1.0f}; // #8f7a66
            shader_->setColor(c);
            glBindTexture(GL_TEXTURE_2D, whiteTexture_);
            shader_->drawModel(models_[0]);
        }
        shader_->setVec4("uColor", 1.0f, 1.0f, 1.0f, 1.0f);
        fontRenderer_->renderText("NEW GAME", btnX, btnY, 0.075f, proj, *shader_);
        glEnable(GL_DEPTH_TEST);
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

    glClearColor(BACKGROUND_COLOR);

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

    float aspect = float(width_) / height_;
    float screenWidthWorld = 2.0f * kProjectionHalfHeight * aspect;
    float screenHeightWorld = 2.0f * kProjectionHalfHeight;

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
                        if (!gameLogic_.isGameOver()) {
                            if (std::abs(dx) > std::abs(dy)) {
                                if (dx > 0) gameLogic_.move(MoveDirection::RIGHT);
                                else gameLogic_.move(MoveDirection::LEFT);
                            } else {
                                if (dy > 0) gameLogic_.move(MoveDirection::DOWN);
                                else gameLogic_.move(MoveDirection::UP);
                            }
                        }
                    } else {
                        if (gameLogic_.isGameOver()) {
                            gameLogic_.reset();
                        } else {
                            // スクリーン座標をワールド座標に変換
                            float wx = (x / width_ - 0.5f) * screenWidthWorld;
                            float wy = (0.5f - y / height_) * screenHeightWorld;

                            // NEW GAME ボタンの判定 (render() 内の定義と正確に合わせる)
                            float btnW = 0.28f, btnH = 0.08f;
                            float btnX = screenWidthWorld * 0.25f, btnY = 1.20f;
                            if (wx >= btnX - btnW && wx <= btnX + btnW &&
                                wy >= btnY - btnH && wy <= btnY + btnH) {
                                gameLogic_.reset();
                            }
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
