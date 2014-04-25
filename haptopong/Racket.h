#pragma once

#include "RacketProperties.h"

class Racket
{
public:
	Racket(void);
	~Racket(void);
	
	void setProperties(const RacketProperties& properties) { m_properties = properties; }

	//Setters for testing (should be updated by the physics engine later)
	void setVelocity(const chai3d::cVector3d& velocity) { m_velocity = velocity; }
	void setNormal(const chai3d::cVector3d& normal) { m_normal = normal; }

	const RacketProperties& getProperties() const { return m_properties; }
	const chai3d::cVector3d& getNormal() const { return m_normal; }
	const chai3d::cVector3d& getVelocity() const { return m_velocity; }

private:
	chai3d::cVector3d m_normal;
	chai3d::cVector3d m_velocity;
	RacketProperties m_properties;
};
