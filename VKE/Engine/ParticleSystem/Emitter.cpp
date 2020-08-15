#include "Emitter.h"
#include "Utilities.h"
namespace VKE
{
	#define MinRadius 0.00001f
	cEmitter::cEmitter()
	{
		EmitterData.Radius = glm::max(EmitterData.Radius, MinRadius);
	}

	cEmitter::~cEmitter()
	{

	}

	void cEmitter::NextParticle(BufferFormats::FParticle& oParticle)
	{
		// Position
		float R = glm::clamp(EmitterData.Radius * sqrt(Rand01()), MinRadius, EmitterData.Radius);
		float theta = Rand01() * 2 * PI;
		
		glm::vec3 oPos = glm::vec3(0.0f);
		float sinTheta = glm::sin(theta);
		float cosTheta = glm::cos(theta);
		oPos.x += R * cosTheta;
		oPos.z += R * sinTheta;
		oParticle.Pos = oPos;

		// Velocity
		float PercentageToCenter = R / EmitterData.Radius;
		float Alpha = PercentageToCenter * EmitterData.Angle;
		float sinAlpha = glm::sin(Alpha);
		float StartSpeed = RandRange(EmitterData.StartSpeedMin, EmitterData.StartSpeedMax);
		oParticle.Vel = glm::vec3(sinAlpha * cosTheta, glm::cos(Alpha), sinAlpha * sinTheta) * StartSpeed;	// normalized * Start Speed

		// Time
		oParticle.LifeTime = RandRange(EmitterData.LifeTimeRangeMin, EmitterData.LifeTimeRangeMax);
		oParticle.ElpasedTime = -RandRange(EmitterData.StartDelayRangeMin, EmitterData.StartDelayRangeMax);
	}

}