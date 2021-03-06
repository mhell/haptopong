#pragma once
#include "Scene.h"

#include "PlayerId.h"
#include "Table.h"
#include "Net.h"
#include "Ball.h"
#include "Racket.h"
#include "BallEventManager.h"
#include "HapticResponseManager.h"
#include "GameRulesManager.h"
#include "StepTimer.h"
#include "Outside.h"
#include "AimAssistance.h"
#include "BounceEffectPool.h"

class PongScene : public Scene
{
public:
	PongScene(Application& app, GameRulesManagerPtr gameRules);
	~PongScene(void);

	void enter(ScenePtr from);
	void exit(ScenePtr to);

	void render(const double& timeStep);
	void updateLogic(const double& timeStep);
	void updateHaptics(const double& timeStep);

	void onKeyDown(unsigned char key, int x, int y);
    void onSpecialDown(int key, int x, int y);
	
	void onNewRound(int localScore, int remoteScore, PlayerId nextServe, PlayerId prevWinner);
	void onGameOver(int localScore, int remoteScore, PlayerId winner);

	void updateOpponentPos(const btVector3& position);
	void updateBallState(const btVector3& position, const btVector3& velocity, const btVector3& angularVelocity, int serve, float hitMagnitude);
	
	static ScenePtr create(Application& app) { return ScenePtr(new PongScene(app, nullptr)); }
	static ScenePtr create(Application& app, GameRulesManagerPtr gameRules) 
	{ 
		PongScene* scene = new PongScene(app, gameRules);
		gameRules->setPongScene(scene);
		return ScenePtr(scene); 
	}

private:

	btVector3 invert(const btVector3& vec);
	
	void prepareServe(PlayerId serve);
	void startServe();
	 
	chai3d::cWorldPtr m_world;
    chai3d::cCamera* m_camera;
	
	btDiscreteDynamicsWorld* m_dynamicsWorld;
	
	btCollisionShapePtr m_tableCollisionShape;
    btCollisionShapePtr m_netCollisionShape;
    btCollisionShapePtr m_ballCollisionShape;
    btCollisionShapePtr m_racketsCollisionShape;
    btCollisionShapePtr m_outsideCollisionShape;
	
	TablePtr m_table;
	NetPtr m_net;
	BallPtr m_ball;
    RacketPtr m_playerRacket;
    RacketPtr m_opponentRacket;
	OutsidePtr m_outside;
    OutsidePtr m_outsideWall;
	BallEventManagerPtr m_ballEventMgr;
	HapticResponseManagerPtr m_hapticResponseMgr;
	StepTimer m_networkTimer;
    
	void createCamera();
    void createLight();
	void createTable();
	void createNet();
	void createBall();
	void createRackets();
	void createOutside();
	
	GameRulesManagerPtr m_gameRules;
	AimAssistancePtr m_aimAssistance;
	PlayerId m_serve;
	BounceEffectPoolPtr m_bouncePool;
	bool m_roundOver;
	double m_roundOverTimer;
	PlayerId m_nextServe;
	
	chai3d::cLabel* m_scoreLabel;
	chai3d::cFont* m_scoreFont;
	chai3d::cLabel* m_infoLabel;
	chai3d::cFont* m_infoFont;
	
	bool m_hapticButtonDown;
	bool m_hapticButtonPressed;
};

