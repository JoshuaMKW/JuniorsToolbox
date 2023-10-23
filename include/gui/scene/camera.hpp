#pragma once

#include <glm/glm.hpp>

class Camera {

public:
    // Default constructor
    Camera();
    ~Camera() = default;

    // Setup on single camera
    void setPerspective(const float FieldOfView_Degs, const float AspectRatio, const float NearDist,
                        const float FarDist);
    void setOrientAndPosition(const glm::vec3 &Up_vect, const glm::vec3 &inLookAt_pt, const glm::vec3 &pos_pt);

    // update camera system
    void updateCamera(void);

    // Get the matrices for rendering
    glm::mat4x4 &getViewMatrix();
    glm::mat4x4 &getProjMatrix();

    // accessors
    void getPos(glm::vec3 &outPos) const;
    void getDir(glm::vec3 &outDir) const;
    void getUp(glm::vec3 &outUp) const;
    void getLookAt(glm::vec3 &outLookAt) const;
    void getRight(glm::vec3 &outRight) const;

    void TranslateLeftRight(float delta);
    void TranslateFwdBack(float delta);
    void TiltUpDown(float ang);
    void TurnLeftRight(float ang);

    void setAspect(float aspect);

    // Why no SETS for Pos, Dir, Up, LookAt and Right?
    //   They have to be adjusted _together_ in setOrientAndPosition()

private:  // methods should never be public
    void privUpdateProjectionMatrix(void);
    void privUpdateViewMatrix(void);

private:  // data  (Keep it private)
    // Projection Matrix
    glm::mat4x4 projMatrix;
    glm::mat4x4 viewMatrix;

    // camera unit vectors (up, dir, right)
    glm::vec3 vUp;
    glm::vec3 vDir;
    glm::vec3 vRight;  // derived by up and dir
    glm::vec3 vPos;
    glm::vec3 vLookAt;

    // Define the frustum inputs
    float nearDist;
    float farDist;
    float fovy;  // aka view angle along y axis
    float aspectRatio;
};