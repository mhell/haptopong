#include "pch.h"
#include "LocalRulesManager.h"
#include "PongScene.h"

#include "Hello.h"
#include "Welcome.h"
#include "UpdatePos.h"
#include "BallEvent.h"
#include "BallState.h"
#include "MetaGameInfo.h"

LocalRulesManager::LocalRulesManager(GameRulesPtr gameRules, int port) :
	m_gameRules(gameRules),
	m_port(port)
{
	m_buffer = new unsigned char[2048];
}

LocalRulesManager::~LocalRulesManager(void)
{
	delete[] m_buffer;
	enet_host_destroy(m_server);
}

void LocalRulesManager::initialize()
{
	m_address.host = ENET_HOST_ANY;
	m_address.port = m_port;

	m_server = enet_host_create(&m_address,
		32, 2, 0, 0);

	if(m_server != nullptr) 
	{
		std::cout<<"Server listening to port: "<<m_port<<std::endl;
		std::cout<<"Waiting for client to connect..."<<std::endl;
	}
	else
		std::cout<<"Failed to create server"<<std::endl;
}

void LocalRulesManager::onBallHitTable(const Ball& ball, const Table& table)
{
	PlayerId side = ball.getPosition().x() < 0.0f ? getOpponentId() : getPlayerId();
	m_gameRules->onBallHitTable(side);

	checkGameRules();
}

void LocalRulesManager::onBallHitRacket(const Ball& ball, const Racket& racket, float hitMagnitude)
{
	m_gameRules->onBallHitRacket(racket.getPlayerId());
	checkGameRules();

	btTransform transform;
	ball.getBody()->getMotionState()->getWorldTransform(transform);

	btVector3 vel = ball.getVelocity();
	btVector3 angVel = ball.getAngularVelocity();

	sendMessage(MessagePtr(new BallState(transform.getOrigin(), vel, angVel, hitMagnitude)), ENET_PACKET_FLAG_RELIABLE);
}

void LocalRulesManager::onServeStart(const Ball& ball)
{
	btTransform transform;
	ball.getBody()->getMotionState()->getWorldTransform(transform);

	btVector3 vel = ball.getVelocity();
	btVector3 angVel = ball.getAngularVelocity();

	sendMessage(MessagePtr(new BallState(transform.getOrigin(), vel, angVel, 0.0, 1)), ENET_PACKET_FLAG_RELIABLE);

	m_gameRules->onNewRound();
}

void LocalRulesManager::onBallOut(const Ball& ball)
{
	m_gameRules->onBallOut();
	checkGameRules();

}

void LocalRulesManager::checkGameRules()
{
	if(m_gameRules->isRoundOver())
	{
		if(!m_gameRules->isGameOver())
		{
			sendMessage(MessagePtr(
				new MetaGameInfo(
				m_gameRules->winner(), 
				MetaGameInfo::META_ROUND_START,
				m_gameRules->nextServe(),
				m_gameRules->getScore().getScore(PLAYER_LOCAL), 
				m_gameRules->getScore().getScore(PLAYER_REMOTE))), 
				ENET_PACKET_FLAG_RELIABLE);

			m_pongScene->onNewRound(
				m_gameRules->getScore().getScore(PLAYER_LOCAL), 
				m_gameRules->getScore().getScore(PLAYER_REMOTE),
				m_gameRules->nextServe(), 
				m_gameRules->winner());

			
		}
		else
		{
			sendMessage(MessagePtr(
				new MetaGameInfo(
				m_gameRules->winner(), 
				MetaGameInfo::META_GAME_OVER,
				PlayerId::NO_PLAYER,
				m_gameRules->getScore().getScore(PLAYER_LOCAL), 
				m_gameRules->getScore().getScore(PLAYER_REMOTE))), 
				ENET_PACKET_FLAG_RELIABLE);	

			m_pongScene->onGameOver(
				m_gameRules->getScore().getScore(PLAYER_LOCAL), 
				m_gameRules->getScore().getScore(PLAYER_REMOTE),
				m_gameRules->winner());
		}
	}
}

void LocalRulesManager::update(const double& timeStep)
{	
	ENetEvent event;

	while(enet_host_service(m_server, &event, 0) > 0)
	{
		switch (event.type)
		{
		case ENET_EVENT_TYPE_CONNECT:
			std::cout<<"Client connected: "<< event.peer->address.host << " : " << event.peer->address.port<<std::endl;
			break;

		case ENET_EVENT_TYPE_RECEIVE:
			updateMessages(event.packet->data, event.packet->dataLength);

			for(auto msg : m_messageQueue)
			{
				switch(msg->getType())
				{
				case C_HELLO:
					{
						sendMessage(MessagePtr(new Welcome("Server")), ENET_PACKET_FLAG_RELIABLE);
						m_isWaiting = false;
					}
					break;
				case G_UPDATE_POS:
					m_pongScene->updateOpponentPos(((UpdatePos*)msg.get())->getPosition());
					break;
				case G_BALLEVENT:
					{
						//std::cout<<"Ball event: " << (int)((BallEvent*)msg.get())->getEventType() << "  "<< (int)((BallEvent*)msg.get())->getPlayerId() << std::endl;
						BallEvent* evt = (BallEvent*)msg.get();
						switch(evt->getEventType())
						{
						case BallEvent::BALLEVENT_TABLE:	
							m_gameRules->onBallHitTable(evt->getPlayerId());
							break;
						case BallEvent::BALLEVENT_RACKET:	
							m_gameRules->onBallHitRacket(evt->getPlayerId());
							break;
						case BallEvent::BALLEVENT_OUTSIDE:	
							m_gameRules->onBallOut();
							break;
						}

						checkGameRules();
					}
					break;
				case G_BALLSTATE:
					{
						BallState* ballState = (BallState*)msg.get();
						m_pongScene->updateBallState(
							ballState->getPosition(), 
							ballState->getVelocity(), 
							ballState->getAngularVelocity(), 
							ballState->getServe(),
							ballState->getHitMagnitude());
						if(ballState->getServe() != 0)
							m_gameRules->onNewRound();
					}
					break;
				}
			}
			m_messageQueue.clear();

			//std::cout<<"Got data: "<< event.packet->dataLength << std::endl;
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
			std::cout<<"Client disconnected: "<< event.peer->address.host << " : " << event.peer->address.port<<std::endl;
			break;
		}
	}
}

void LocalRulesManager::updatePlayerPos(const btVector3& position)
{
	if(isWaiting())
		return;

	sendMessage(MessagePtr(new UpdatePos(position)), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
}

void LocalRulesManager::sendMessage(MessagePtr msg, enet_uint32 reliability)
{
	unsigned char* buf = m_buffer;
	msg->addToBuffer(buf);

	ENetPacket * packet = enet_packet_create (m_buffer, 
		msg->getSize(), 
		reliability);

	enet_host_broadcast (m_server, 0, packet);
	enet_host_flush (m_server);
}