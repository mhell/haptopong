#include "pch.h"
#include "PongScene.h"

#include "Application.h"

#include <ode/ode.h>

using namespace chai3d;

PongScene::PongScene(Application& app) :
	Scene(app)
{
    // create a new world.
    m_world = std::make_shared<chai3d::cWorld>();
    
    // set the background color of the environment
    m_world->m_backgroundColor.setGrayLevel(0.6f);
    
    createCamera();
    
    createLight();
	
    //Create dynamics world, default settings
	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btCollisionDispatcher* dispatcher = new	btCollisionDispatcher(collisionConfiguration);
	btBroadphaseInterface* overlappingPairCache = new btDbvtBroadphase();
	btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

	m_dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,overlappingPairCache,solver,collisionConfiguration);
	m_dynamicsWorld->setGravity(btVector3(0, 0, -10));

    
    
	createTable();
    
	/*

	m_groundShape = new btBoxShape(btVector3(btScalar(0.5),btScalar(2),btScalar(0.5)));

	btTransform groundTransform;
	groundTransform.setIdentity();
	{
		btScalar mass(0.);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0,0,0);
		if (isDynamic)
			m_groundShape->calculateLocalInertia(mass,localInertia);

		//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,m_groundShape,localInertia);
		m_groundBody = new btRigidBody(rbInfo);

		//add the body to the dynamics world
		m_dynamicsWorld->addRigidBody(m_groundBody);
	}
	m_ground = new cShapeBox(1.0, 4, 1.0);
	m_world->addChild(m_ground);
	
	{
		//create a dynamic rigidbody

		//btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
		btCollisionShape* m_sphereShape = new btSphereShape(btScalar(1.));

		/// Create Dynamic Objects
		btTransform startTransform;
		startTransform.setIdentity();

		btScalar	mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0,0,0);
		if (isDynamic)
			m_sphereShape->calculateLocalInertia(mass,localInertia);

			startTransform.setOrigin(btVector3(0,0,5));
		
			//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
			btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,m_sphereShape,localInertia);
			m_sphereBody = new btRigidBody(rbInfo);
			

			m_dynamicsWorld->addRigidBody(m_sphereBody);
	}
	
	m_sphere = new cShapeSphere(1.0);
	m_world->addChild(m_sphere);*/
}

PongScene::~PongScene(void)
{

}

void PongScene::enter(ScenePtr from)
{

}

void PongScene::exit(ScenePtr to)
{

}

void PongScene::render(const double& timeStep)
{
    /*
	btTransform transform;
	btMotionState* pState = m_sphereBody->getMotionState();
	pState->getWorldTransform(transform);	
	m_sphere->setLocalPos(Util::Vec(transform.getOrigin()));
	
	pState = m_groundBody->getMotionState();
	pState->getWorldTransform(transform);	
	m_ground->setLocalPos(Util::Vec(transform.getOrigin()));
 */
	m_camera->renderView(m_app.getWindowWidth(), m_app.getWindowHeight());
    
}

void PongScene::updateLogic(const double& timeStep)
{
	m_dynamicsWorld->stepSimulation((btScalar)timeStep, 10);
}

void PongScene::updateHaptics(const double& timeStep)
{
	cVector3d pos;
	m_app.getHapticDevice()->getPosition(pos);

	pos *= 100;
	double mag = cClamp((pos.x() + 3.5) * 1.5, 2.0, 20.0); 

	/*m_camera->set( cVector3d (mag * cCosRad(pos.y()* 0.5), mag * cSinRad(pos.y() * 0.5), pos.z() * 1.7),    // camera position (eye)
                 cVector3d (0.0, 0.0, 0.0),    // look at position (target)
                 cVector3d (0.0, 0.0, 1.0));   // direction of the (up) vector
*/
	cVector3d zero(0,0,0);
	m_app.getHapticDevice()->setForce(zero);
}

void PongScene::onKeyDown(unsigned char key, int x, int y)
{
	if(key == 'w')
	{
		m_sphereBody->activate();
		m_sphereBody->applyImpulse(btVector3(0,0,5), m_sphereBody->getCenterOfMassPosition());
	}
}

void PongScene::createCamera() {
    // create a camera and insert it into the virtual world
    m_camera = new chai3d::cCamera(m_world.get());
    m_world->addChild(m_camera);
    
    // position and orient the camera
    m_camera->set(cVector3d (2.47, 0.0, 0.95),    // camera position (eye)
                  cVector3d (0.0, 0.0, 0.01),    // look at position (target)
                  cVector3d (0.0, 0.0, 1.0));   // direction of the (up) vector
    
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

void PongScene::createLight() {
    // create a directional light source
    chai3d::cDirectionalLight *dirLight = new cDirectionalLight(m_world.get());
    
    // insert light source inside world
    m_world->addChild(dirLight);
    
    // enable light source
    dirLight->setEnabled(true);
    
    // define direction of light beam
    dirLight->setDir(-7 , 5, -3);
    
    // set lighting conditions
    dirLight->m_ambient.set(0.4f, 0.4f, 0.4f);
    dirLight->m_diffuse.set(0.45f, 0.45f, 0.45f);
    dirLight->m_specular.set(0.2f, 0.2f, 0.2f);
    
    
    // create a spot light source
    cSpotLight *spotLight = new cSpotLight(m_world.get());
    
    // attach light to camera
    //m_world->addChild(spotLight);
    
    // enable light source
    spotLight->setEnabled(true);
    
    // position the light source
    spotLight->setLocalPos(2.5, 0.0, 2.0);
    
    // define the direction of the light beam
    spotLight->setDir(-1.0, 0.0, -1.0);
    
    // enable this light source to generate shadows
    spotLight->setShadowMapEnabled(true);
    
    // set the resolution of the shadow map
    spotLight->m_shadowMap->setResolutionMedium();
    
    // set light cone half angle
    spotLight->setCutOffAngleDeg(25);
    
    // set lighting conditions
    spotLight->m_ambient.set(0.0f, 0.0f, 0.0f);
    spotLight->m_diffuse.set(0.9f, 0.9f, 0.9f);
    spotLight->m_specular.set(0.7f, 0.7f, 0.7f);
}

void PongScene::createTable()
{
    // create cMultiMesh
    cMultiMesh* table = new cMultiMesh();
    
    // load an object file
    bool fileload;
    fileload = table->loadFromFile("../gfx/table.obj");
    
    if (!fileload)
    {
        std::cout << "Error - 3D Model failed to load correctly" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    
    // add gfx objects to world
    m_world->addChild(table);
    
    // Since we don't need to see our polygons from both sides, we enable culling.
    table->setUseCulling(true);
    
    // enable display list for faster graphic rendering (need tp recompute if tranlated?)
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
    
    m_table = std::make_shared<Table>(table, nullptr);
    
}