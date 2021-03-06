#pragma once
#include "AimAssistance.h"
class GlobalMoveAssistance :
	public AimAssistance
{
public:
	GlobalMoveAssistance(BallPtr ball, RacketPtr playerRacket, chai3d::cCamera* camera);
	virtual ~GlobalMoveAssistance(void);

	void useGuiding(bool guide) { m_useGuiding = guide; }
	void setGuideForce(double force) { m_guideForce = force; }

	virtual void render(const double& timeStep);
	virtual void updateLogic(const double& timeStep);
	virtual void updateHaptics(const double& timeStep, chai3d::cVector3d& force);

	virtual void applyImpulseFromRacket(btManifoldPoint& point);

private:

	bool m_useGuiding;
	double m_guideForce;
};

