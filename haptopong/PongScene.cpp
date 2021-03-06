#include "pch.h"
#include "PongScene.h"
#include "Application.h"
#include "CustomMesh.h"
#include "CustomSphere.h"
#include "CustomCamera.h"
#include "GlobalMoveAssistance.h"

#include <ode/ode.h>

using namespace chai3d;

BallEventManagerPtr g_ballEventMgr = nullptr;

bool OnContactProcessed(btManifoldPoint& point,void* body0,void* body1)
{
	//std::cerr<<"CONTACT\n";
	GameObject* go0 = (GameObject*)((btRigidBody*)body0)->getUserPointer();
	GameObject* go1 = (GameObject*)((btRigidBody*)body1)->getUserPointer();

	if(go1->getType() == BALL)
		std::swap(go0, go1);

	if(go0->getType() == BALL)
	{
		switch(go1->getType())
		{
		case RACKET:
			g_ballEventMgr->OnRacketHit(point, *(Racket*)go1, *(Ball*)go0);
			break;
		case NET:
			g_ballEventMgr->OnNetHit(point, *(Net*)go1, *(Ball*)go0);
			break;
		case TABLE:
			g_ballEventMgr->OnTableHit(point, *(Table*)go1, *(Ball*)go0);
			break;
		case OUTSIDE:
			g_ballEventMgr->OnOutside(point, *(Ball*)go0);
			break;
		}
	}

	//Return value is ignored by bullet in newer versions.
	return true;
}

PongScene::PongScene(Application& app, GameRulesManagerPtr gameRules) :
	Scene(app),
	m_gameRules(gameRules),
	m_networkTimer(1.0 / 20.0),
	m_serve(PLAYER_LOCAL),
	m_roundOver(false),
	m_roundOverTimer(0.0)
{
	m_hapticResponseMgr = HapticResponseManagerPtr(new HapticResponseManager());

	// create a new world.
	m_world = std::make_shared<cWorld>();

	m_bouncePool = BounceEffectPoolPtr(new BounceEffectPool(m_world.get(), 10));

	m_ballEventMgr = BallEventManagerPtr(new BallEventManager(m_hapticResponseMgr, m_gameRules, m_bouncePool));
	g_ballEventMgr = m_ballEventMgr;

	// set the background color of the environment
	//m_world->m_backgroundColor.setGrayLevel(0.6f);
	m_world->m_backgroundColor.setGrayLevel(0.7f);

	createCamera();
	createLight();

	//Create dynamics world, default settings
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new	btCollisionDispatcher(collisionConfiguration);
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	m_dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,overlappingPairCache,solver,collisionConfiguration);
	m_dynamicsWorld->setGravity(btVector3(0, 0, -7));
	
	createTable();
	createNet();
	createBall();
	createRackets();
	createOutside();

	gContactProcessedCallback = &OnContactProcessed;
	
	//std::cerr<<3;
	m_aimAssistance = AimAssistancePtr(new AimAssistance(m_ball, m_playerRacket, m_camera));
	m_ballEventMgr->setAimAssistance(m_aimAssistance);

	if(m_gameRules != nullptr)
		m_gameRules->initialize();

	m_ball->onRoundStart();

    // front layer
	m_scoreFont = NEW_CFONTCALIBRI40(); //cFontPtr(NEW_CFONTCALIBRI40());
	m_infoFont = NEW_CFONTCALIBRI40(); //cFontPtr(NEW_CFONTCALIBRI40());

	m_scoreLabel = new cLabel(m_scoreFont/*.get()*/);
	m_scoreLabel->setString("0 - 0");

	m_infoLabel = new cLabel(m_infoFont/*.get()*/);
	m_infoLabel->setString(m_gameRules->getPlayerId() == PLAYER_LOCAL ? "Your serve!" : "Opponent's serve!");

	m_camera->m_frontLayer->addChild(m_infoLabel);
	m_camera->m_frontLayer->addChild(m_scoreLabel);
	
	m_hapticButtonDown = false;
	m_hapticButtonPressed = false;
    
    // background layer
    cBackground* background = new cBackground();
    m_camera->m_backLayer->addChild(background);
    
    // load a texture file
    bool fileload = background->loadFromFile("../gfx/background.png");
    if (!fileload)
    {
        std::cout << "Error - Image failed to load correctly." << std::endl;
		std::exit(EXIT_FAILURE);
    }
    
    // init camera 2
    onKeyDown('2', 0, 0);
}

PongScene::~PongScene(void)
{
	g_ballEventMgr = nullptr;

    #ifdef MACOSX
	// osx texture deletion crash fix
	m_world->removeChild(m_ball->getShape());
    m_ball = nullptr;
    #endif
}

void PongScene::enter(ScenePtr from)
{

}

void PongScene::exit(ScenePtr to)
{

}

void PongScene::render(const double& timeStep)
{

	//btTransform transform;
	//btMotionState* pState = m_sphereBody->getMotionState();
	//pState->getWorldTransform(transform);
	//m_sphere->setLocalPos(Util::Vec(transform.getOrigin()));


	m_aimAssistance->render(timeStep);
	m_table->render((float)timeStep);
	m_net->render((float)timeStep);
	m_ball->render((float)timeStep);
	m_playerRacket->render((float)timeStep);
	m_opponentRacket->render((float)timeStep);

	m_bouncePool->update(timeStep);

	{
		double w = (double)m_app.getWindowWidth();
		double h = (double)m_app.getWindowHeight();
		double lw, lh, y = h;

		//Render title
		lw = m_scoreLabel->getStringWidth();
		lh = m_scoreLabel->getStringHeight();
		y -= lh * 1.5;
		m_scoreLabel->setLocalPos(20, y, 0);
		//y -= lh * 0.5;

		lw = m_infoLabel->getStringWidth();
		lh = m_infoLabel->getStringHeight();
		m_infoLabel->setLocalPos(0.5 * (w - lw), y, 0);
		y -= lh * 0.5;

	}

	m_camera->renderView(m_app.getWindowWidth(), m_app.getWindowHeight());

#ifdef TESTING_NETWORK
	::Sleep(5);
#endif
}

void PongScene::updateLogic(const double& timeStep)
{
	m_ballEventMgr->update(timeStep);

	if(m_roundOver)
	{
		m_roundOverTimer -= timeStep;
		if(m_roundOverTimer <= 0.0)
		{
			m_roundOver = false;
			m_ball->onRoundStart();
			prepareServe(m_nextServe);
		}
	}

	if(m_gameRules != nullptr)
	{
		m_gameRules->update(timeStep);

		if(m_networkTimer.update(timeStep) > 0)
		{
			btVector3 pos = m_playerRacket->getPosition();
			m_gameRules->updatePlayerPos(pos);
		}
	}

	if(m_hapticButtonPressed)
	{
		m_hapticButtonPressed = false;
		startServe();
	}

	if(m_serve != NO_PLAYER)
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(0, 0, 0.7f));
		m_ball->setVelocity(btVector3(0, 0, 0));
		m_ball->setAngularVelocity(btVector3(0, 0, 0));
		m_ball->setActive(false);
	}

	//std::cout<<1;
	m_aimAssistance->updateLogic(timeStep);
	m_table->updateLogic((float)timeStep);
	m_net->updateLogic((float)timeStep);
	m_ball->updateLogic((float)timeStep);
	m_playerRacket->updateLogic((float)timeStep);
	m_opponentRacket->updateLogic((float)timeStep);
	
	//std::cout<<2;
	//m_dynamicsWorld->stepSimulation((btScalar)timeStep, 10);
	//m_dynamicsWorld->stepSimulation((btScalar)timeStep, 5, btScalar(1.)/btScalar(120.));
	//std::cout<<"   "<<m_dynamicsWorld<<"  ";
	m_dynamicsWorld->stepSimulation((btScalar)timeStep * 0.8f, 10, btScalar(1.)/btScalar(500.));
	
	//std::cout<<3;


	/*	m_camera->set(cVector3d (2.47, (double)m_ball->getBody()->getCenterOfMassPosition().y(), (double)m_ball->getBody()->getCenterOfMassPosition().z()),   // camera position (eye)
	cVector3d (0.0, 0.0, 0.01),    // look at position (target)
	cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
	*/
}

void PongScene::updateHaptics(const double& timeStep)
{
	//std::cerr<<'h';
	m_playerRacket->updateHaptics(m_app.getHapticDevice(), timeStep);

	cVector3d force(0,0,0);

	m_hapticResponseMgr->updateHaptics(timeStep, force);
	m_aimAssistance->updateHaptics(timeStep, force);

	m_app.getHapticDevice()->setForce(force);
	
	bool button;
	m_app.getHapticDevice()->getUserSwitch(0, button);

	if(button)
	{
		if(!m_hapticButtonDown)
		{
			m_hapticButtonDown = true;
			m_hapticButtonPressed = true;
		}
	}
	else
	{
		m_hapticButtonDown = false;
	}

#ifdef TESTING_NETWORK
	::Sleep(10);
#endif
}

void PongScene::onNewRound(int localScore, int remoteScore, PlayerId nextServe, PlayerId prevWinner)
{
	std::cout<<"NEW ROUND!  "<< localScore<< " - "<<remoteScore<<"\n";
	//prepareServe(nextServe);
	m_roundOver = true;
	m_nextServe = nextServe;
	m_roundOverTimer = 2.0;
	m_ball->setActive(false);
	m_ball->onRoundEnd();

	if(m_gameRules->getPlayerId() == PLAYER_LOCAL)
	{
		std::stringstream s;
		s << localScore << " - " << remoteScore;
		m_scoreLabel->setString(s.str());
	}
	else
	{
		std::stringstream s;
		s << localScore << " - " << remoteScore;
		m_scoreLabel->setString(s.str());
	}

	if(m_gameRules->getPlayerId() == prevWinner)
		m_infoLabel->setString("Well done!");
	else
		m_infoLabel->setString("Too bad!");

}

void PongScene::onGameOver(int localScore, int remoteScore, PlayerId winner)
{
	m_roundOver = false;
	m_ball->setActive(false);
	m_ball->onRoundEnd();

	if(m_gameRules->getPlayerId() == PLAYER_LOCAL)
	{
		std::stringstream s;
		s << localScore << " - " << remoteScore;
		m_scoreLabel->setString(s.str());
	}
	else
	{
		std::stringstream s;
		s << localScore << " - " << remoteScore;
		m_scoreLabel->setString(s.str());
	}

	if(m_gameRules->getPlayerId() == winner)
		m_infoLabel->setString("Game over! You won!");
	else
		m_infoLabel->setString("Game over! You lost!");
}

btVector3 PongScene::invert(const btVector3& vec)
{
	btVector3 res;
	res[0] = -vec[0];
	res[1] = -vec[1];
	res[2] = vec[2];
	return res;
}

void PongScene::updateOpponentPos(const btVector3& position)
{
	btVector3 pos = invert(position);
	m_opponentRacket->setPosition(pos, m_networkTimer.getTimeStep());
}

void PongScene::updateBallState(const btVector3& position, const btVector3& velocity, const btVector3& angularVelocity, int serve, float hitMagnitude)
{
	m_ball->stop();
	m_ball->setPosition(invert(position));
	m_ball->setVelocity(invert(velocity));
	m_ball->setAngularVelocity(invert(angularVelocity));
	m_ball->setActive(serve == 0);
	if(serve != 0)
	{
		m_serve = NO_PLAYER;
		m_infoLabel->setString("");
	}
	else
	{
		m_opponentRacket->flash();
		m_ballEventMgr->playSound(hitMagnitude);
	}
}

void PongScene::prepareServe(PlayerId serve)
{
	m_serve = serve;

	if(m_gameRules->getPlayerId() == serve)
		m_infoLabel->setString("Your serve!");
	else
		m_infoLabel->setString("Opponent's serve!");
}

void PongScene::startServe()
{
	if(m_serve == m_gameRules->getPlayerId())
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(0, 0, 0.7f));
		m_ball->setVelocity(btVector3(2.0f, Util::RandRange(-0.5f, 0.5f), 0.4f));
		m_ball->setAngularVelocity(btVector3(0, 0, -30 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
		m_serve = NO_PLAYER;

		m_gameRules->onServeStart(*m_ball);
		m_infoLabel->setString("");
	}
}

void PongScene::onKeyDown(unsigned char key, int x, int y)
{
	if(key == 's')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(-2, 0, 0.3f));
		m_ball->setVelocity(btVector3(4, Util::RandRange(-1.5f, 1.5f), 3.3f));
		m_ball->setAngularVelocity(btVector3(0, 0, -100 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
	}
	if(key == 'd')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(2, 0, 0.3f));
		m_ball->setVelocity(btVector3(-4, Util::RandRange(-1, 1), 3.3f));
		m_ball->setAngularVelocity(btVector3(0, -300, 0));
		m_ball->setActive(true);
	}
	if(key == 'q')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(-2, 0, 0.3f));
		m_ball->setVelocity(btVector3(3, Util::RandRange(-1.5f, 1.5f), 3.3f));
		m_ball->setAngularVelocity(btVector3(0, 0, -100 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
	}
	if(key == 'w')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(-2, 0, 0.3f));
		m_ball->setVelocity(btVector3(3, Util::RandRange(-2.5f, 2.5f), 3.3f));
		m_ball->setAngularVelocity(btVector3(0, 0, -150 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
	}
	if(key == 'e')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(-2, 0, 0.3f));
		m_ball->setVelocity(btVector3(3, Util::RandRange(-0.5f, 0.5f), 3.3f));
		m_ball->setAngularVelocity(btVector3(0, 0, -10 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
	}
	if(key == 'a')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.5, 0, 0.1f));
		m_ball->setVelocity(btVector3(-1, Util::RandRange(-0.5f, 0.5f), 2.0f));
		m_ball->setAngularVelocity(btVector3(0, 0, 00 * m_ball->getVelocity().y()));
		m_ball->setActive(true);
	}
	if(key == '<')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(-.1f, 0, 0.1f));
		m_ball->setVelocity(btVector3(0, -0.6f, 0));
		m_ball->setAngularVelocity(btVector3(0, 0, 0));
		m_ball->setActive(true);
	}
	if(key == 'z')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.27f, 0.65f, 0.1f));
		m_ball->setVelocity(btVector3(0.04f, 0.04f, 0.0));
		m_ball->setAngularVelocity(btVector3(0, 0, 0));
		m_ball->setActive(true);
	}
	if(key == 'r')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.2f, 0.0f, 0.58f));
		m_ball->setVelocity(btVector3(5.f, 0.f, 0.f));
		m_ball->setAngularVelocity(btVector3(0, 0, 0));
		m_ball->setActive(true);
	}
	if(key == 't')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.2f, 0.0f, 0.58f));
		m_ball->setVelocity(btVector3(5.f, 0.f, 0.f));
		m_ball->setAngularVelocity(btVector3(10, 0, 0));
		m_ball->setActive(true);
	}
	if(key == 'y')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.2f, 0.0f, 0.58f));
		m_ball->setVelocity(btVector3(5.f, 0.f, 0.f));
		m_ball->setAngularVelocity(btVector3(0, 10, 0));
		m_ball->setActive(true);
	}
	if(key == 'u')
	{
		m_ball->stop();
		m_ball->setPosition(btVector3(1.2f, 0.0f, 0.58f));
		m_ball->setVelocity(btVector3(5.f, 0.f, 0.f));
		m_ball->setAngularVelocity(btVector3(0, 0, 10));
		m_ball->setActive(true);
	}

	if(key == '1')
	{
		// position and orient the camera
		m_camera->set(cVector3d (2.47, 0.0, 0.95),   // camera position (eye)
			cVector3d (0.0, 0.0, 0.01),    // look at position (target)
			cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		m_aimAssistance = AimAssistancePtr(new AimAssistance(m_ball, m_playerRacket, m_camera));
		m_ballEventMgr->setAimAssistance(m_aimAssistance);

		m_playerRacket->setSize(1.0);
		m_playerRacket->setMoveAreaScale(10.0f);
		m_opponentRacket->setSize(1.0);
		m_opponentRacket->setMoveAreaScale(10.0f);
	}
	if(key == '2')
	{
		m_aimAssistance = AimAssistancePtr(new GlobalMoveAssistance(m_ball, m_playerRacket, m_camera));
		m_ballEventMgr->setAimAssistance(m_aimAssistance);
		m_playerRacket->setSize(2.0);
		m_playerRacket->setMoveAreaScale(18.0f);
		m_opponentRacket->setSize(2.0);
		m_opponentRacket->setMoveAreaScale(18.0f);
	}
	if(key == '3')
	{
		auto* gma = new GlobalMoveAssistance(m_ball, m_playerRacket, m_camera);
		gma->useGuiding(true);
		m_aimAssistance = AimAssistancePtr(gma);
		m_ballEventMgr->setAimAssistance(m_aimAssistance);
		m_playerRacket->setSize(2.0);
		m_playerRacket->setMoveAreaScale(18.0f);
		m_opponentRacket->setSize(2.0);
		m_opponentRacket->setMoveAreaScale(18.0f);
	}
    if (key == '0')
    {
        // enable/disable wire mode
        bool useWireMode = m_table->getShape()->getWireMode();
        
        m_table->getShape()->setWireMode(!useWireMode, true);
        m_net->getShape()->setWireMode(!useWireMode, true);
        m_ball->getShape()->setWireMode(!useWireMode, true);
        m_playerRacket->getShape()->setWireMode(!useWireMode, true);
        m_opponentRacket->getShape()->setWireMode(!useWireMode, true);
    }
	if(key == ' ' )
	{
		startServe();
	}

	if(key == 'h')
	{
		prepareServe(PLAYER_LOCAL);
	}
}

void PongScene::onSpecialDown(int key, int x, int y)
{
	switch (key) {
	case GLUT_KEY_DOWN:
		m_camera->set(cVector3d (0.02, -1.4, 0.055),   // camera position (eye)
                      cVector3d (0.0, 0.0, 0.055),    // look at position (target)
                      cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		m_camera->set(cVector3d (-1.5, 0.0, 0.6),   // camera position (eye)
                      cVector3d (-2.0, 0.0, 0.6),    // look at position (target)
                      cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		break;
	case GLUT_KEY_LEFT:
		m_camera->set(cVector3d (0, -5.0, 0.6),   // camera position (eye)
                      cVector3d (.0, 0.0, 0.6),    // look at position (target)
                      cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		break;
	case GLUT_KEY_RIGHT:
		m_camera->set(cVector3d (1.4, 1.2, 0.17),    // camera position (eye)
                      cVector3d (1.4, 0.0, 0.01),    // look at position (target)
                      cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		m_camera->set(cVector3d (1.2, 1.6, 0.4),    // camera position (eye)
                      cVector3d (1.2, 0.0, 0.4),    // look at position (target)
                      cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		break;
	case GLUT_KEY_UP:
		m_camera->set(cVector3d (2.47, 0.0, 0.95),   // camera position (eye)
        cVector3d (0.0, 0.0, 0.01),    // look at position (target)
        cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector
		break;
	}
}

void PongScene::createCamera()
{
	// create a camera and insert it into the virtual world
	m_camera = new CustomCamera(m_world.get());
	m_world->addChild(m_camera);

	// position and orient the camera
	m_camera->set(cVector3d (2.47, 0.0, 0.95),   // camera position (eye)
		cVector3d (0.0, 0.0, 0.01),    // look at position (target)
		cVector3d (0.0, 0.0, 1.0));    // direction of the (up) vector

	// set the near and far clipping planes of the camera
	m_camera->setClippingPlanes(0.01, 100.0);

	// set stereo mode
	m_camera->setStereoMode(C_STEREO_DISABLED);

	// set stereo eye separation and focal length (applies only if stereo is enabled)
	m_camera->setStereoEyeSeparation(0.01);
	m_camera->setStereoFocalLength(0.5);

	// set vertical mirrored display mode
	m_camera->setMirrorVertical(false);

	// enable multi-pass rendering to handle transparent objects
	m_camera->setUseMultipassTransparency(true);

	// enable shadow casting
	m_camera->setUseShadowCasting(true);
}

void PongScene::createLight()
{
	/////////////////////////////////////////////////////////////////////////
	// create a directional light source
	/////////////////////////////////////////////////////////////////////////

	chai3d::cDirectionalLight *dirLight = new cDirectionalLight(m_world.get());

	// insert light source inside world
	m_world->addChild(dirLight);

	// enable light source
	dirLight->setEnabled(true);

	// define direction of light beam
	dirLight->setDir(-7 , 5, -3);
	//dirLight->setDir(-7 , 2, -3);

	// set lighting conditions
	dirLight->m_ambient.set(0.4f, 0.4f, 0.4f);
	dirLight->m_diffuse.set(0.45f, 0.45f, 0.45f);
	dirLight->m_specular.set(0.2f, 0.2f, 0.2f);

	//dirLight->setUseTwoSideLightModel(false);

	/////////////////////////////////////////////////////////////////////////
	// create a spot light source - shadow casting only
	/////////////////////////////////////////////////////////////////////////

	cSpotLight* shadowSpotLight = new cSpotLight(m_world.get());

	//m_world->addChild(spotLight);

	// enable light source
	shadowSpotLight->setEnabled(true);

	// position the light source
	shadowSpotLight->setLocalPos(0.5, 0.0, 5.0);

	// define the direction of the light beam
	shadowSpotLight->setDir(-0.1, 0.0, -1.0);

	// enable this light source to generate shadows
	shadowSpotLight->setShadowMapEnabled(true);

	// set the resolution of the shadow map
	shadowSpotLight->m_shadowMap->setResolutionMedium();

	//spotLight->setShadowMapProperties(0.1, 10.);

	// set light cone half angle
	//spotLight->setCutOffAngleDeg(15);
	shadowSpotLight->setCutOffAngleDeg(16);

	// set lighting conditions
	shadowSpotLight->m_ambient.set(0.f, 0.f, 0.f);
	shadowSpotLight->m_diffuse.set(0.f, 0.f, 0.f);
	shadowSpotLight->m_specular.set(0.f, 0.f, 0.f);

	//spotLight->setUseTwoSideLightModel(false);
	//spotLight->setSpotExponent(0);

	/////////////////////////////////////////////////////////////////////////
	// create a spot light source - light casting only
	/////////////////////////////////////////////////////////////////////////

	chai3d::cSpotLight* spotLight2 = new cSpotLight(m_world.get());

	// attach light to camera
	//m_world->addChild(spotLight);

	// enable light source
	spotLight2->setEnabled(true);

	// position the light source
	spotLight2->setLocalPos(2.5, 0.0, 2.0);

	// define the direction of the light beam
	spotLight2->setDir(-1.0, 0.0, -1.0);

	// set light cone half angle
	spotLight2->setCutOffAngleDeg(49);

	// set lighting conditions
	spotLight2->m_ambient.set(0.0f, 0.0f, 0.0f);
	spotLight2->m_diffuse.set(0.9f, 0.9f, 0.9f);
	spotLight2->m_specular.set(0.7f, 0.7f, 0.7f);

	//spotLight->setUseTwoSideLightModel(false);
	//spotLight->setSpotExponent(0);
}

void PongScene::createTable()
{
	/////////////////////////////////////////////////////////////////////////
	// create visual shape
	/////////////////////////////////////////////////////////////////////////

	cMultiMesh* table = new CustomMesh(m_world.get(), false, true);
	//cMultiMesh* table = new ShadowlessMesh();

	bool fileload = table->loadFromFile("../gfx/table.obj");

	if (!fileload)
	{
		std::cout << "Error - 3D Model failed to load correctly" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	m_world->addChild(table);

	// enable culling to disable rendering of the inside
	table->setUseCulling(true);

	// enable display list for faster graphic rendering (recompute if translated)
	table->setUseDisplayList(true, true);

	// create texture
	cTexture2dPtr table_texture = cTexture2d::create();
	table_texture->setWrapMode(GL_REPEAT);
	fileload = table_texture->loadFromFile("../gfx/table_diffuse.png");
	if (!fileload)
	{
		std::cout << "Error - Texture image failed to load correctly." << std::endl;
		std::exit(EXIT_FAILURE);
	}

	table->setTexture(table_texture);
	table->setUseTexture(true, true);

	// shadow mesh

	cMultiMesh* tableShadow = new cMultiMesh();

	fileload = tableShadow->loadFromFile("../gfx/table_shadow.obj");

	if (!fileload)
	{
		std::cout << "Error - 3D Model failed to load correctly" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	m_world->addChild(tableShadow);

	tableShadow->setUseTransparency(true);
	tableShadow->setTransparencyLevel(0.f);

	/////////////////////////////////////////////////////////////////////////
	// create physics body
	/////////////////////////////////////////////////////////////////////////

	m_tableCollisionShape = std::shared_ptr<btCollisionShape>(Util::LoadCollisionShape("../gfx/table_body.obj"));
	//m_tableShape = new btBoxShape(btVector3(btScalar(2.74*0.5),btScalar(1.52*0.5),btScalar(0.10*0.5)));

	m_table = std::make_shared<Table>(table, m_tableCollisionShape.get());

	m_dynamicsWorld->addRigidBody(m_table->getBody());
}

void PongScene::createNet()
{
	/////////////////////////////////////////////////////////////////////////
	// create visual shape
	/////////////////////////////////////////////////////////////////////////

	cMultiMesh* net = new CustomMesh(m_world.get(), false, true);

	bool fileload = net->loadFromFile("../gfx/net.obj");

	if (!fileload)
	{
		std::cout << "Error - 3D Model failed to load correctly" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	m_world->addChild(net);

	// enable culling to disable rendering of the inside
	net->setUseCulling(true);

	// enable display list for faster graphic rendering (recompute if translated)
	net->setUseDisplayList(true, true);

	// create texture
	cTexture2dPtr net_texture = cTexture2d::create();
	net_texture->setWrapMode(GL_REPEAT);
	fileload = net_texture->loadFromFile("../gfx/net_diffuse.png");
	if (!fileload)
	{
		std::cout << "Error - Texture image failed to load correctly." << std::endl;
		std::exit(EXIT_FAILURE);
	}

	net->getMesh(0)->setTexture(net_texture);
	net->getMesh(0)->setUseTexture(true, true);

	// enable transparency for this object
	net->getMesh(0)->m_texture->m_image->setTransparentColor(30, 30, 30, 0);
	net->getMesh(0)->setUseTransparency(true);

	/////////////////////////////////////////////////////////////////////////
	// create physics body
	/////////////////////////////////////////////////////////////////////////

	m_netCollisionShape = std::shared_ptr<btCollisionShape>(Util::LoadCollisionShape("../gfx/net_body.obj"));

	m_net = std::make_shared<Net>(net, m_netCollisionShape.get());

	m_dynamicsWorld->addRigidBody(m_net->getBody());
}

void PongScene::createBall()
{
	BallProperties properties;

	CustomSphere* ballShape = new CustomSphere(m_world.get(), (double)properties.getRadius(), true, false);

	m_world->addChild(ballShape);
    
	m_ballCollisionShape = std::make_shared<btSphereShape>(btScalar(properties.getRadius()));

	cTexture2dPtr activeTex = cTexture2d::create();
	activeTex->setWrapMode(GL_REPEAT);
	bool fileload = activeTex->loadFromFile("../gfx/ball_active_diffuse.png");
	if (!fileload)
	{
		std::cout << "Error - Texture image failed to load correctly." << std::endl;
		std::exit(EXIT_FAILURE);
	}
    
    cTexture2dPtr inactiveTex = cTexture2d::create();
	inactiveTex->setWrapMode(GL_REPEAT);
	fileload = inactiveTex->loadFromFile("../gfx/ball_inactive_diffuse.png");
	if (!fileload)
	{
		std::cout << "Error - Texture image failed to load correctly." << std::endl;
		std::exit(EXIT_FAILURE);
	}

	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(0,0,1));

	m_ball = std::make_shared<Ball>(ballShape, m_ballCollisionShape.get(), activeTex, inactiveTex, properties, startTransform);

	m_dynamicsWorld->addRigidBody(m_ball->getBody());
}


void PongScene::createOutside()
{
	BallProperties properties;
    
    // floor

	m_outsideCollisionShape = btCollisionShapePtr(new btBoxShape(btVector3(5.7f, 5.7f, 0.2f)));
	m_outside = std::make_shared<Outside>(m_outsideCollisionShape.get());

	m_dynamicsWorld->addRigidBody(m_outside->getBody().get());

	cMaterial mat;
	mat.setWhite();
	mat.m_ambient = cColorf(1.0f, 1.0f, 1.0f, 1.0f);
	//mat.m_ambient = cColorf(0.92f, 0.92f, 0.92f, 1.0f);

	cShapeBox* box = new cShapeBox(4.0, 15, 0.4);
	box->setMaterial(mat, true);
	box->setLocalPos(0.5, 0, -0.5);
	m_world->addChild(box);
    
    // anti-shadow floors (2)
    
    cShapeBox* box3 = new cShapeBox(3, 5, 0.4);
    mat.m_ambient.set(0.f, 0.f, 0.f);
	mat.m_diffuse.set(0.f, 0.f, 0.f);
	mat.m_specular.set(0.f, 0.f, 0.f);
    mat.m_emission.set(0.75f, 0.75f , 0.75f);
    //mat.m_emission.set(0.7f, 0.7f , 0.7f);
	box3->setMaterial(mat, true);
	box3->setLocalPos(0, -4.0, -0.495);
	m_world->addChild(box3);
    
    cShapeBox* box4 = new cShapeBox(3, 5, 0.4);
    mat.m_ambient.set(0.f, 0.f, 0.f);
	mat.m_diffuse.set(0.f, 0.f, 0.f);
	mat.m_specular.set(0.f, 0.f, 0.f);
    mat.m_emission.set(0.75f, 0.75f , 0.75f);
    //mat.m_emission.set(0.7f, 0.7f , 0.7f);
	box4->setMaterial(mat, true);
	box4->setLocalPos(0, 4.0, -0.495);
	m_world->addChild(box4);
     
    // far wall
    
	m_outsideWall = std::make_shared<Outside>(m_outsideCollisionShape.get());
    btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(-5.7f, 0, 0));
	transform.setRotation(btQuaternion(90*0.017f, 0, 0));
    m_outsideWall->getBody()->setWorldTransform(transform);
    
    m_dynamicsWorld->addRigidBody(m_outsideWall->getBody().get());
    m_outsideWall->getBody()->setRestitution(0.025f);
    
}

void PongScene::createRackets()
{
	/////////////////////////////////////////////////////////////////////////
	// create visual shapes
	/////////////////////////////////////////////////////////////////////////

	// player racket

	cMultiMesh* playerRacket = new cMultiMesh();
	//ShadowlessMesh* playerRacket = new ShadowlessMesh();

	bool fileload = playerRacket->loadFromFile("../gfx/racket.obj");
	if (!fileload)
	{
		std::cout << "Error - 3D Model failed to load correctly" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	cMaterial mat;
	mat.m_ambient.set( 0.2f, 0.2f, 0.2f);
	mat.m_diffuse.set( 1.0f, 1.0f, 1.0f);
	mat.m_specular.set(1.0f, 1.0f, 1.0f);
	playerRacket->setMaterial(mat, true);
	//playerRacket->computeAllNormals();

	playerRacket->setUseTransparency(true);
	playerRacket->setTransparencyLevel(0.6f);

	m_world->addChild(playerRacket);

	// opponent racket

	cMultiMesh* opponentRacket = new CustomMesh(m_world.get(), false, false);

	fileload = opponentRacket->loadFromFile("../gfx/racket.obj");
	if (!fileload)
	{
		std::cout << "Error - 3D Model failed to load correctly" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	mat.m_ambient.set( 0.5f, 0.5f, 0.5f);
	mat.m_diffuse.set( 0.5f, 0.5f, 0.5f);
	mat.m_specular.set(1.0f, 1.0f, 1.0f);
	opponentRacket->setMaterial(mat, true);
    
	opponentRacket->setUseCulling(true);

	m_world->addChild(opponentRacket);


	/////////////////////////////////////////////////////////////////////////
	// create physics bodys
	/////////////////////////////////////////////////////////////////////////

	// player racket

	RacketProperties properties;

	m_racketsCollisionShape = std::shared_ptr<btCollisionShape>(Util::LoadCollisionShape("../gfx/racket_body.obj"));
	btTransform startTransform;
	startTransform.setIdentity();
	//startTransform.setOrigin(btVector3(2.1f, 0, 0.88f));
	startTransform.setOrigin(btVector3(1.8f, 0, 0.6f));
	startTransform.setRotation(btQuaternion(0, 40*0.0174532925f, 0));


	m_playerRacket = std::make_shared<Racket>(playerRacket, m_racketsCollisionShape.get(), properties, startTransform);
	if(m_gameRules != nullptr)
		m_playerRacket->setPlayerId(m_gameRules->getPlayerId());

	m_dynamicsWorld->addRigidBody(m_playerRacket->getBody());

	// opponent racket

	startTransform.setIdentity();
	startTransform.setOrigin(btVector3(-1.8f, 0, 0.6f));
	startTransform.setRotation(btQuaternion(0, -40*0.0174532925f, 0));

	m_opponentRacket = std::make_shared<Racket>(opponentRacket, m_racketsCollisionShape.get(), properties, startTransform);
	if(m_gameRules != nullptr)
		m_opponentRacket->setPlayerId(m_gameRules->getOpponentId());
	m_opponentRacket->setIsOpponent(true);
	m_dynamicsWorld->addRigidBody(m_opponentRacket->getBody());


}


