
#pragma once
#ifndef MATRIXBUFFERS_H
#define MATRIXBUFFERS_H

#include "vec\vec.h"
#include "vec\mat.h"

using namespace linalg;

struct MatrixBuffer_t
{
	mat4f ModelToWorldMatrix;
	mat4f WorldToViewMatrix;
	mat4f ProjectionMatrix;
};

struct PointLightBuffer_t {
	float4 my_color;
	float4 my_pos;
	float4 camera_pos;
};

struct PhongBuffer_t {
	float4 ambient_color;
	float4 diffuse_color;
	float4 specular_color;
	//float shine;
};

#endif