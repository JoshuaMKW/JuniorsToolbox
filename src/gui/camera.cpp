#include "gui/scene/camera.hpp"
#include <assert.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <math.h>

namespace Toolbox {

    Camera::Camera() : aspectRatio(0), farDist(0), fovy(0), nearDist(0) {
        projMatrix = glm::identity<glm::mat4x4>();
        viewMatrix = glm::identity<glm::mat4x4>();

        vUp     = {0, 1, 0};
        vDir    = {0, 0, 1};
        vRight  = {1, 0, 0};
        vPos    = {0, 0, 0};
        vLookAt = {0, 0, 1};
    }

    void Camera::setPerspective(const float Fovy, const float Aspect, const float NearDist,
                                const float FarDist) {
        this->aspectRatio = Aspect;
        this->fovy        = Fovy;
        this->nearDist    = NearDist;
        this->farDist     = FarDist;
    };

    void Camera::setOrientAndPosition(const glm::vec3 &inUp, const glm::vec3 &inLookAt,
                                      const glm::vec3 &inPos) {
        // Remember the up, dir and right are unit length, and are perpendicular.
        // Treat lookAt as king, find Right vect, then correct Up to insure perpendiculare.
        // Make sure that all vectors are unit vectors.

        this->vLookAt = inLookAt;
        this->vDir    = (inLookAt - inPos);  // Right-Hand camera: vDir is flipped
        this->vDir    = glm::normalize(this->vDir);

        // Clean up the vectors (Right hand rule)
        this->vRight = glm::cross(inUp, this->vDir);
        this->vRight = glm::normalize(this->vRight);

        this->vUp = glm::cross(this->vDir, this->vRight);
        this->vUp = glm::normalize(this->vUp);

        this->vPos = inPos;
    };

    // The projection matrix
    void Camera::privUpdateProjectionMatrix(void) {
        if (aspectRatio <= 0)
            return;
        this->projMatrix = glm::perspective(fovy, aspectRatio, nearDist, farDist);
    };

    void Camera::privUpdateViewMatrix(void) {
        this->viewMatrix = glm::lookAt(vPos, vPos + vDir, vUp);
    };

    // Update everything (make sure it's consistent)
    void Camera::updateCamera(void) {
        // update the projection matrix
        this->privUpdateProjectionMatrix();

        // update the view matrix
        this->privUpdateViewMatrix();
    }

    glm::mat4x4 &Camera::getViewMatrix(void) { return this->viewMatrix; }

    glm::mat4x4 &Camera::getProjMatrix(void) { return this->projMatrix; }

    void Camera::getPos(glm::vec3 &outPos) const { outPos = this->vPos; }

    void Camera::getDir(glm::vec3 &outDir) const { outDir = this->vDir; }

    void Camera::getUp(glm::vec3 &outUp) const { outUp = this->vUp; }

    void Camera::getLookAt(glm::vec3 &outLookAt) const { outLookAt = this->vLookAt; }

    void Camera::getRight(glm::vec3 &outRight) const { outRight = this->vRight; }

    float Camera::getNearDist() const { return this->nearDist; }

    float Camera::getFarDist() const { return this->farDist; }

    float Camera::getFOV() const { return this->fovy; }

    float Camera::getAspectRatio() const { return this->aspectRatio; }

    void Camera::translateLeftRight(float delta) { vPos += vRight * delta; }

    void Camera::translateFwdBack(float delta) { vPos += vDir * delta; }

    void Camera::tiltUpDown(float ang) {
        glm::mat4x4 Rot = glm::rotate(glm::mat4(1.0f), ang, vRight);
        vDir            = glm::vec3(Rot * glm::vec4(vDir, 1.0f));
        vUp             = glm::vec3(Rot * glm::vec4(vUp, 1.0f));
        setOrientAndPosition(vUp, vPos - vDir, vPos);
    }

    void Camera::turnLeftRight(float ang) {
        glm::mat4x4 Rot = glm::rotate(glm::mat4(1.0f), ang, glm::vec3(0.0f, 1.0f, 0.0f));
        vDir            = glm::vec3(Rot * glm::vec4(vDir, 1.0f));
        vUp             = glm::vec3(Rot * glm::vec4(vUp, 1.0f));
        setOrientAndPosition(vUp, vPos - vDir, vPos);
    }

    void Camera::setNearDist(float nearDist) { this->nearDist = nearDist; }

    void Camera::setFarDist(float farDist) { this->farDist = farDist; }

    void Camera::setAspectRatio(float aspect) { this->aspectRatio = aspect; }

    void Camera::setFOV(float fov) { this->fovy = fov; }

}  // namespace Toolbox