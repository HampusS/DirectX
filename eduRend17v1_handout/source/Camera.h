//
//  Camera.h
//
//	Basic camera class
//

#pragma once
#ifndef CAMERA_H
#define CAMERA_H

#include "vec\vec.h"
#include "vec\mat.h"
#include <iostream>

using namespace linalg;

class camera_t
{
	float pitch, yaw;
	float camera_speed = 50.0f;
	float sensitivity = 0.5f;
public:

	float vfov, aspect;	// Aperture attributes
	float zNear, zFar;	// Clip planes
						// zNear should be >0
						// zFar should depend on the size of the scene
	vec3f position;
	mat4f rotationMatrix;
	camera_t(
		float vfov,
		float aspect,
		float zNear,
		float zFar) :
		vfov(vfov), aspect(aspect), zNear(zNear), zFar(zFar)
	{

	}

	// Move to an absolute position
	//
	void moveTo(const vec3f& p)
	{
		position = p;
	}

	// Move relatively
	//
	void move(const vec3f& v)
	{
		vec4f direction = v.xyz0();
		vec4f velocity = get_ViewToWorldMatrix() * direction * camera_speed;
		position += velocity.xyz();
	}

	// Return World-to-View matrix for this camera
	//
	mat4f get_WorldToViewMatrix()
	{
		// Assuming a camera's position and rotation is defined by matrices T(p) and R,
		// the View-to-World transform is T(p)*R (for a first-person style camera)
		// World-to-View then is the inverse of T*R;
		//	inverse(T(p)*R) = inverse(R)*inverse(T(p)) = transpose(R)*T(-p)
		// Since now there is no rotation, this matrix is simply T(-p)

		return transpose(rotationMatrix) * mat4f::translation(-position);
	}

	mat4f get_ViewToWorldMatrix() {
		return mat4f::translation(position)* rotationMatrix;
	}

	void RotateCamera(float mouseY, float mouseX) {
		pitch -= mouseY * sensitivity;
		yaw -= mouseX * sensitivity;
		rotationMatrix = mat4f::rotation(0, yaw, pitch);
	}

	// Matrix transforming from View space to Clip space
	// In a performance sensitive situation this matrix can be precomputed, as long as
	// all parameters are constant (which typically is the case)
	//
	mat4f get_ProjectionMatrix()
	{
		return mat4f::projection(vfov, aspect, zNear, zFar);
	}
};

#endif