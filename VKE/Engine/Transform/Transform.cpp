#include "Transform.h"

FTransform& FTransform::operator=(const glm::mat4& i_m)
{
	m = i_m;
	mInv = glm::inverse(m);
	return *this;
}

FTransform::FTransform(const glm::vec3& i_initialTranslation, const glm::quat& i_intialRotation, const glm::vec3& i_initialScale)
{
	SetTransform(i_initialTranslation, i_intialRotation, i_initialScale);
}

glm::quat FTransform::ToQuaternian(const double yaw, const double pitch, const double roll)
{
	// Abbreviations for the various angular functions
	double cy = cos(yaw * 0.5);
	double sy = sin(yaw * 0.5);
	double cp = cos(pitch * 0.5);
	double sp = sin(pitch * 0.5);
	double cr = cos(roll * 0.5);
	double sr = sin(roll * 0.5);

	glm::quat q;
	q.w = static_cast<float>(cy * cp * cr + sy * sp * sr);
	q.x = static_cast<float>(cy * cp * sr - sy * sp * cr);
	q.y = static_cast<float>(sy * cp * sr + cy * sp * cr);
	q.z = static_cast<float>(sy * cp * cr - cy * sp * sr);

	return q;
}

void FTransform::Translate(const glm::vec3& i_location)
{
	m_position += i_location;
}

void FTransform::Rotate(const glm::vec3& i_axis, const float& i_angle)
{
	m_rotation *= glm::angleAxis(i_angle, i_axis);
}

void FTransform::Rotate(const glm::quat& i_quat)
{
	m_rotation *= i_quat;
}

void FTransform::Scale(const glm::vec3& i_scale)
{
	m_scale *= i_scale;
}

void FTransform::gRotate(const glm::vec3& i_axis, const float& i_angle)
{
	glm::vec3 _worldAxis = glm::inverse(m_rotation) * i_axis;
	m_rotation *= glm::angleAxis(i_angle, _worldAxis);
}

void FTransform::gScale(const glm::vec3& i_scale)
{
	glm::vec3 _worldScale = i_scale;
	m_scale *= _worldScale;
}

void FTransform::gScale(const float x, const float y, const float z)
{
	gScale(glm::vec3(x, y, z));
}

void FTransform::MirrorAlongPlane(const FTransform& Other)
{
	float deltaY = (m_position - Other.Position()).y;
	Translate(glm::vec3(0, -2.f*deltaY, 0));
	gScale(1, 1, -1);
	Update();
}

void FTransform::SetTransform(const glm::vec3 & i_initialTranslation, const glm::quat & i_intialRotation, const glm::vec3 & i_initialScale)
{
	m_position = i_initialTranslation;
	m_rotation = i_intialRotation;
	m_scale = i_initialScale;

	Update();
}


glm::vec3 FTransform::GetEulerAngle() const
{
	glm::vec3 _euler = glm::eulerAngles(m_rotation);
	return glm::degrees(_euler);
}

glm::mat4 FTransform::GetTranslationMatrix() const
{
	glm::mat4 _m = glm::mat4(1.0);
	_m[3] = glm::vec4(m_position, 1);
	return _m;
}

glm::mat4 FTransform::GetRotationMatrix() const
{
	return glm::toMat4(m_rotation);
}

glm::mat4 FTransform::GetScaleMatrix() const
{
	glm::mat4 _m = glm::mat4(1.0);
	_m[0][0] = m_scale.x; _m[1][1] = m_scale.y; _m[2][2] = m_scale.z;
	return _m;

}

glm::vec3 FTransform::Forward() const
{
	return m_rotation * FTransform::WorldForward;
}

glm::vec3 FTransform::Right() const
{
	return m_rotation * FTransform::WorldRight;
}

glm::vec3 FTransform::Up() const
{
	return m_rotation * FTransform::WorldUp;
}

bool FTransform::HasScale() const
{
#define NOT_ONE(x) ((x) < .999f || (x) > 1.001f)
	return (NOT_ONE((m*glm::vec4(1, 0, 0, 0)).length()) || NOT_ONE((m*glm::vec4(1, 0, 0, 0)).length()) || NOT_ONE((m*glm::vec4(1, 0, 0, 0)).length()));
#undef NOT_ONE
}

void FTransform::Update()
{
	m = GetTranslationMatrix() * GetRotationMatrix() * GetScaleMatrix();
	mInv = glm::inverse(m);
}
glm::vec3 FTransform::WorldUp = glm::vec3(0.0, 1.0, 0.0);

glm::vec3 FTransform::WorldRight = glm::vec3(1.0, 0.0, 0.0);

glm::vec3 FTransform::WorldForward = glm::vec3(0.0, 0.0, 1.0);
