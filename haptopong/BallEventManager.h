#pragma once

#include "Ball.h"
#include "Table.h"
#include "Racket.h"
#include "Net.h"
#include "HapticResponseManager.h"
#include "SoundPlayer.h"
#include "GameRulesManager.h"
#include "GameObjectType.h"
#include "AimAssistance.h"
#include "BounceEffectPool.h"

class BallEventManager
{
public:
	BallEventManager(HapticResponseManagerPtr hapticResponseMgr, GameRulesManagerPtr gameRulesMgr, BounceEffectPoolPtr bouncePool);
	~BallEventManager(void);
	
	void OnTableHit(btManifoldPoint& point, Table& table, Ball& ball);
	void OnNetHit(btManifoldPoint& point, Net& net, Ball& ball);
	void OnRacketHit(btManifoldPoint& point, Racket& racket, Ball& ball);
	void OnOutside(btManifoldPoint& point, Ball& ball);

	void setAimAssistance(AimAssistancePtr aimAssistance) { m_aimAssistance = aimAssistance; }

	void playSound(float hitMagnitude);
	void update(const double& timeStep) { m_tableHitCooldown += timeStep; }

private:
	HapticResponseManagerPtr m_hapticResponseMgr;
	GameRulesManagerPtr m_gameRulesMgr;
    SoundPlayer* m_racketHit;
    SoundPlayer* m_tableHit;
	
	AimAssistancePtr m_aimAssistance;
	GameObjectType m_lastHit;
	int m_count;
	BounceEffectPoolPtr m_bouncePool;
	double m_tableHitCooldown;
};

typedef std::shared_ptr<BallEventManager> BallEventManagerPtr;
