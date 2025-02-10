#pragma once

#include <fastgltf/tools.hpp>
#include "cglm/types-struct.h"


namespace fastgltf
{

template<>
struct ElementTraits<vec2s> : ElementTraitsBase<vec2s, AccessorType::Vec2, float> {};

template<>
struct ElementTraits<vec3s> : ElementTraitsBase<vec3s, AccessorType::Vec3, float> {};

template<>
struct ElementTraits<vec4s> : ElementTraitsBase<vec4s, AccessorType::Vec4, float> {};

template<>
struct ElementTraits<ivec2s> : ElementTraitsBase<ivec2s, AccessorType::Vec2, int> {};

template<>
struct ElementTraits<ivec3s> : ElementTraitsBase<ivec3s, AccessorType::Vec3, int> {};

template<>
struct ElementTraits<ivec4s> : ElementTraitsBase<ivec4s, AccessorType::Vec4, int> {};

template<>
struct ElementTraits<mat2s> : ElementTraitsBase<mat2s, AccessorType::Mat2, float> {};

template<>
struct ElementTraits<mat3s> : ElementTraitsBase<mat3s, AccessorType::Mat3, float> {};

template<>
struct ElementTraits<mat4s> : ElementTraitsBase<mat4s, AccessorType::Mat4, float> {};

} // namespace fastgltf
