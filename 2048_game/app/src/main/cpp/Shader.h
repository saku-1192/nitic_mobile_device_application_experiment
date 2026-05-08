#ifndef ANDROIDGLINVESTIGATIONS_SHADER_H
#define ANDROIDGLINVESTIGATIONS_SHADER_H

#include <string>
#include <GLES3/gl3.h>

class Model;

class Shader {
public:
    static Shader *loadShader(
            const std::string &vertexSource,
            const std::string &fragmentSource,
            const std::string &positionAttributeName,
            const std::string &uvAttributeName,
            const std::string &projectionMatrixUniformName,
            const std::string &modelMatrixUniformName,
            const std::string &colorUniformName);

    inline ~Shader() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    void activate() const;
    void deactivate() const;
    void drawModel(const Model &model) const;

    void setProjectionMatrix(const float *projectionMatrix) const;
    void setModelMatrix(const float *modelMatrix) const;
    void setColor(const float *color) const;
    void setVec2(const std::string &name, float x, float y) const;
    void setVec4(const std::string &name, float r, float g, float b, float a) const;
    void setMatrix(const std::string &name, const float* matrix) const;

    GLuint getProgram() const { return program_; }
    GLint getPositionAttrib() const { return position_; }
    GLint getUvAttrib() const { return uv_; }

private:
    static GLuint loadShader(GLenum shaderType, const std::string &shaderSource);

    constexpr Shader(
            GLuint program,
            GLint position,
            GLint uv,
            GLint projectionMatrix,
            GLint modelMatrix,
            GLint color)
            : program_(program),
              position_(position),
              uv_(uv),
              projectionMatrix_(projectionMatrix),
              modelMatrix_(modelMatrix),
              color_(color) {}

    GLuint program_;
    GLint position_;
    GLint uv_;
    GLint projectionMatrix_;
    GLint modelMatrix_;
    GLint color_;
};

#endif //ANDROIDGLINVESTIGATIONS_SHADER_H
