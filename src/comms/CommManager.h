#ifndef DRONEUTILS_H
#define DRONEUTILS_H

#include "Arduino.h"
#include <inttypes.h>

#include "comms/NMEA.h"
#include "comms/Protocol.h"
#include "math/GreatCircle.h"
#include "math/Waypoint.h"
#include "storage/List.h"
#include "storage/SRAMlist.h"
#include "storage/Storage.h"
#include "util/byteConv.h"

using namespace Protocol;

const uint8_t BUFF_LEN = 32;

//Settings -- container supplied by outside world
//write setting
//read setting

//Messages -- send and forget message passing system
//send message
//attach message callback

//Waypoints -- uses self contained List implementation
//interface unchanged for now


class CommManager{
	HardwareSerial 		*stream;
	uint8_t 			buf[BUFF_LEN];
	uint8_t 			bufPos;
	boolean 			isLooped;
	List<Waypoint>*		waypoints;
	Storage<float>*		storage;
	Waypoint   			cachedTarget;
	uint16_t 			targetIndex;
	bool				waypointsLooped;
	void (*connectCallback)(void);
	void (*eStopCallback)(void);
public:
	CommManager(HardwareSerial *inStream, Storage<float> *settings);
	Waypoint getWaypoint(int index);
	Waypoint getTargetWaypoint();
	void  	update();
	void  	requestResync();
	void	sendTelem(uint8_t id, float value);
	void    setSetting(uint8_t id,   float input);
	float   getSetting(uint8_t id);
	int     getTargetIndex();
	void    setTargetIndex(int index);
	int 	numWaypoints();
	bool 	loopWaypoints();
	void 	clearWaypointList();
	void    advanceTargetIndex();
	void    retardTargetIndex();
	void	setConnectCallback(void (*call)(void));
	void	setEStopCallback(void (*call)(void));
private:
	void	onConnect();
	void	sendCommand(uint8_t id, uint8_t data);
	void	handleCommand(commandType command, uint8_t data);
	void	sendSync();
	void	sendSyncResponse();
	void    sendSetting(uint8_t id, float value);
	void    inputSetting(uint8_t id, float input);
	void    sendTargetIndex();
	void    processMessage(uint8_t* msg, uint8_t length);
	void    sendConfirm(uint16_t digest);
	boolean rightMatch(const uint8_t* lhs, const uint8_t llen,
					   const uint8_t* rhs, const uint8_t rlen);
	boolean recieveWaypoint(waypointSubtype type, uint8_t index, Waypoint point);
};

void sendGPSMessage(uint8_t Type, uint8_t ID, uint16_t len, const uint8_t* buf);
void updateGPSchecksum(const uint8_t *msg, uint8_t len, uint8_t &c_a, uint8_t &c_b);
#endif
