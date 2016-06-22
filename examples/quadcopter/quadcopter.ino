#include "Wire.h"
#include "SPI.h"
#include "MINDS-i-Drone.h"
#include "platforms/Quadcopter.h"

boolean calibrated = false;
uint32_t calStartTime;
const uint8_t CHANNEL_MIN = 32;
const uint8_t CHANNEL_MAX = 148;
float yawTarget = 0.0;

enum State { DISARMED, CALIBRATE, FLYING } state;
const char* stateString[] = {
    [DISARMED] = "DISARMED", [CALIBRATE] = "CALIBRATE", [FLYING] = "FLYING" };
enum RadioChannel{ RADIO_PITCH = 0, RADIO_ROLL = 1, RADIO_THROTTLE = 2,
                   RADIO_YAW   = 3, RADIO_GEAR = 4, RADIO_AUX      = 5 };
/*
time tools idea:
static Interval sendTelem = Interval::every(milliseconds(50));
if(sendTelem){
}
*/
class StateTimer{
private:
    bool (*stateF)(void);
    uint32_t enterTime;
    bool lastState;
public:
    StateTimer(bool (*stateF)(void)): stateF(stateF) {}
    void update() {
        bool state = stateF();
        if(state != lastState){
            lastState = state;
            enterTime = millis();
        }
    }
    bool trueFor(uint32_t interval) {
        update();
        return (lastState) && ((millis() - enterTime) > interval);
    }
    bool falseFor(uint32_t interval) {
        update();
        return (!lastState) && ((millis() - enterTime) > interval);
    }
};

StateTimer radioDownRight([](){
    bool down  = APMRadio::get(RADIO_THROTTLE) <= CHANNEL_MIN;
    bool right = APMRadio::get(RADIO_YAW)      <= CHANNEL_MIN;
    return down && right;
});

StateTimer radioDownLeft([](){
    bool down  = APMRadio::get(RADIO_THROTTLE) <= CHANNEL_MIN;
    bool left  = APMRadio::get(RADIO_YAW)      >= CHANNEL_MAX;
    return down && left;
});

void setup() {
    setupQuad();
    setState(DISARMED);
}

void setState(State s){
    state = s;
    comms.sendString(stateString[s]);
}

void loop() {
    //always running updates
    loopQuad();
    sendTelemetry();

    //flight mode state machine
    switch(state){
        /*#DISARMED Hold Down and to the right on
         * the Throttle stick for two seconds to begin the arming process
        **/
        case DISARMED:
            if(radioDownRight.trueFor(2000)) {
                if(safe()){
                    calStartTime = millis();
                    setState(CALIBRATE);
                } else {
                    /*#NOFLY Flight aborted due to error status */
                    comms.sendString("NOFLY");
                }
            }
            break;
        /*#CALIBRATE Calibrating the gyroscope.
         * This lasts for two seconds. Keep the quadcopter as still as possible.
        **/
        case CALIBRATE:
            orientation.calibrate(true);
            if(calStartTime + 2000 < millis()) {
                orientation.calibrate(false);
                yawTarget = orientation.getYaw();
                output.standby();
                setState(FLYING);
            }
            break;
        /*#FLYING
         * Hold down and to the left on the throttle stick to disarm
        **/
        case FLYING:
            fly();
            if(radioDownLeft.trueFor(750)) {
                output.disable();
                setState(DISARMED);
            }
            break;
    }

}

void fly(){
    const float pitchCmd = ((float)APMRadio::get(RADIO_PITCH)-90) /-70.0;
    const float rollCmd  = ((float)APMRadio::get(RADIO_ROLL)-90)  /-70.0;
    const float yawCmd   = ((float)APMRadio::get(RADIO_YAW)-90)   / 90.0;
    const float throttle = ((float)APMRadio::get(RADIO_THROTTLE)-25)/130.0;
    const bool  gearCmd  = (APMRadio::get(4) > 90);

    if(APMRadio::get(RADIO_THROTTLE) <= CHANNEL_MIN){
        output.standby();
        return;
    } else {
        output.enable();
    }

    if(fabs(yawCmd) > 0.1){
        yawTarget += yawCmd/8;
        yawTarget = truncateRadian(yawTarget);
    }

    float throttleOut = throttleCurve.get(throttle);

    comms.sendTelem(VOLTAGE+2  , throttleOut);
    horizon.set(pitchCmd, rollCmd, yawTarget, throttleOut);
}

void sendTelemetry(){
    static uint32_t sendTime = millis();
    if(sendTime < millis()){
        sendTime += 50;

        float voltage  = float(analogRead(67)/1024.l*5.l*10.1f);

        using namespace Protocol;
        comms.sendTelem(LATITUDE   , state);
        comms.sendTelem(LONGITUDE  , ((float)APMRadio::get(RADIO_THROTTLE)-25)/130.0 );
        comms.sendTelem(HEADING    , toDeg(orientation.getYaw()));
        comms.sendTelem(PITCH      , toDeg(orientation.getPitch()));
        comms.sendTelem(ROLL       , toDeg(orientation.getRoll()));
        comms.sendTelem(GROUNDSPEED, profileTime(0));
        comms.sendTelem(VOLTAGE    , voltage);
        comms.sendTelem(AMPERAGE   , safe());
        //comms.sendTelem(ALTITUDE   , altitude.get());

        Serial.println();
        Serial.flush();
    }
}
