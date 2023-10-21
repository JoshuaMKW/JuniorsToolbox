#include "gui/camera.hpp"
#include "gui/input.hpp"

#include <algorithm>
#include <GLFW/glfw3.h>

namespace Toolbox::UI {

Camera::Camera() : mNearPlane(100.0f), mFarPlane(1000000.f), mFovy(glm::radians(60.f)),
    mCenter(ZERO), mEye(ZERO), mPitch(0.f), mYaw(glm::half_pi<float>()), mUp(UNIT_Y), mRight(UNIT_X), mForward(UNIT_Z),
    mAspectRatio(16.f / 9.f), mMoveSpeed(1000.f), mMouseSensitivity(0.25f)
{
	mCenter = mEye - mForward;
}

void Camera::Update(float deltaTime) {
	glm::vec3 moveDir = glm::zero<glm::vec3>();

	if (Input::GetKey(GLFW_KEY_W))
		moveDir -= mForward;
	if (Input::GetKey(GLFW_KEY_S))
		moveDir += mForward;
	if (Input::GetKey(GLFW_KEY_D))
		moveDir -= mRight;
	if (Input::GetKey(GLFW_KEY_A))
		moveDir += mRight;

	if (Input::GetKey(GLFW_KEY_Q))
		moveDir -= UNIT_Y;
	if (Input::GetKey(GLFW_KEY_E) || Input::GetKey(GLFW_KEY_SPACE))
		moveDir += UNIT_Y;

	mMoveSpeed += Input::GetMouseScrollDelta() * 100 * deltaTime;
	mMoveSpeed = std::clamp(mMoveSpeed, 100.f, 50000.f);
	float actualMoveSpeed = Input::GetKey(GLFW_KEY_LEFT_SHIFT) ? mMoveSpeed * 10.f : mMoveSpeed;

	if (Input::GetMouseButton(GLFW_MOUSE_BUTTON_RIGHT))
		Rotate(deltaTime, Input::GetMouseDelta());

	if (glm::length(moveDir) != 0.f)
		moveDir = glm::normalize(moveDir);

	mEye += moveDir * (actualMoveSpeed * deltaTime);
	mCenter = mEye - mForward;
}

void Camera::Rotate(float deltaTime, glm::vec2 mouseDelta) {
	if (mouseDelta.x == 0.f && mouseDelta.y == 0.f)
		return;

	mPitch += mouseDelta.y * deltaTime * mMouseSensitivity;
	mYaw += mouseDelta.x * deltaTime * mMouseSensitivity;

	mPitch = std::clamp(mPitch, LOOK_UP_MIN, LOOK_UP_MAX);

	mForward.x = cos(mYaw) * cos(mPitch);
	mForward.y = sin(mPitch);
	mForward.z = sin(mYaw) * cos(mPitch);

	mForward = glm::normalize(mForward);

	mRight = glm::normalize(glm::cross(mForward, UNIT_Y));
	mUp = glm::normalize(glm::cross(mRight, mForward));
}

};