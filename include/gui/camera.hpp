#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct GLFWwindow;

namespace Toolbox::UI {

constexpr glm::vec3 ZERO = glm::vec3(0.0f, 0.0f, 0.0f);
constexpr glm::vec3 UNIT_X = glm::vec3(1.0f, 0.0f, 0.0f);
constexpr glm::vec3 UNIT_Y = glm::vec3(0.0f, 1.0f, 0.0f);
constexpr glm::vec3 UNIT_Z = glm::vec3(0.0f, 0.0f, 1.0f);

constexpr float LOOK_UP_MIN = -glm::half_pi<float>() + glm::epsilon<float>();
constexpr float LOOK_UP_MAX = glm::half_pi<float>() - glm::epsilon<float>();

class Camera {
	glm::vec3 mEye;
	glm::vec3 mCenter;

	float mPitch;
	float mYaw;
	glm::vec3 mForward;
	glm::vec3 mRight;
	glm::vec3 mUp;

	float mNearPlane;
	float mFarPlane;
	float mFovy;
	float mAspectRatio;

	float mMoveSpeed;
	float mMouseSensitivity;

	void Rotate(float deltaTime, glm::vec2 mouseDelta);

public:
	Camera();
	~Camera() {}

	void Update(float deltaTime);

	glm::vec3 GetPosition() { return mEye; }
	glm::vec3 GetForward() { return mForward; }
	glm::mat4 GetViewMatrix() { return glm::lookAt(mEye, mCenter, mUp); }
	glm::mat4 GetProjectionMatrix() { return glm::perspective(mFovy, mAspectRatio, mNearPlane, mFarPlane); }
};

};