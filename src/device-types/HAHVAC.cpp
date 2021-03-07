#include "HAHVAC.h"
#include "../ArduinoHADefines.h"
#include "../HAMqtt.h"
#include "../HADevice.h"
#include "../HAUtils.h"

const char* HAHVAC::ActionTopic = "at";
const char* HAHVAC::AuxCommandTopic = "act";
const char* HAHVAC::AuxStateTopic = "ast";
const char* HAHVAC::AwayCommandTopic = "amct";
const char* HAHVAC::AwayStateTopic = "amst";
const char* HAHVAC::HoldCommandTopic = "hct";
const char* HAHVAC::HoldStateTopic = "hst";
const char* HAHVAC::TargetTemperatureCommandTopic = "ttct";
const char* HAHVAC::TargetTemperatureStateTopic = "ttst";
const char* HAHVAC::CurrentTemperatureTopic = "ctt";

HAHVAC::HAHVAC(
    const char* uniqueId,
    Features features,
    HAMqtt& mqtt
) :
    BaseDeviceType("climate", uniqueId),
    _uniqueId(uniqueId),
    _features(features),
    _temperatureUnit(DefaultUnit),
    _action(OffAction),
    _auxHeatingCallback(nullptr),
    _auxHeatingState(false),
    _awayCallback(nullptr),
    _awayState(false),
    _holdCallback(nullptr),
    _holdState(false),
    _currentTemperature(255),
    _minTemp(255),
    _maxTemp(255),
    _tempStep(1),
    _targetTemperature(255),
    _targetTempCallback(nullptr),
    _label(nullptr)
{

}

void HAHVAC::onMqttConnected()
{
    if (strlen(name()) == 0) {
        return;
    }

    publishConfig();
    publishAction(_action);
    publishAuxHeatingState(_auxHeatingState);
    publishAwayState(_awayState);
    publishHoldState(_holdState);
    publishCurrentTemperature(_currentTemperature);
    publishTargetTemperature(_targetTemperature);
    subscribeTopics();
}

void HAHVAC::onMqttMessage(
    const char* topic,
    const uint8_t* payload,
    const uint16_t& length
)
{
    if (isMyTopic(topic, AuxCommandTopic)) {
        bool state = (length == strlen(DeviceTypeSerializer::StateOn));
        setAuxHeatingState(state);
    } else if (isMyTopic(topic, AwayCommandTopic)) {
        bool state = (length == strlen(DeviceTypeSerializer::StateOn));
        setAwayState(state);
    } else if (isMyTopic(topic, HoldCommandTopic)) {
        bool state = (length == strlen(DeviceTypeSerializer::StateOn));
        setHoldState(state);
    } else if (isMyTopic(topic, TargetTemperatureCommandTopic)) {
        char src[length + 1];
        memset(src, 0, sizeof(src));
        memcpy(src, payload, length);

        setTargetTemperature(HAUtils::strToTemp(src));
    }

    // parse topics:
    // mode_command_topic
}

bool HAHVAC::setAction(Action action)
{
    if (_action == action) {
        return true;
    }

    if (publishAction(action)) {
        _action = action;
        return true;
    }

    return false;
}

bool HAHVAC::setAuxHeatingState(bool state)
{
    if (!(_features & AuxHeatingFeature)) {
        return false;
    }

    if (_auxHeatingState == state) {
        return true;
    }

    if (publishAuxHeatingState(state)) {
        _auxHeatingState = state;

        if (_auxHeatingCallback) {
            _auxHeatingCallback(_auxHeatingState);
        }

        return true;
    }

    return false;
}

bool HAHVAC::setAwayState(bool state)
{
    if (!(_features & AwayModeFeature)) {
        return;
    }

    if (_awayState == state) {
        return true;
    }

    if (publishAwayState(state)) {
        _awayState = state;

        if (_awayCallback) {
            _awayCallback(_awayState);
        }

        return true;
    }

    return false;
}

bool HAHVAC::setHoldState(bool state)
{
    if (!(_features & HoldFeature)) {
        return;
    }

    if (_holdState == state) {
        return true;
    }

    if (publishHoldState(state)) {
        _holdState = state;

        if (_holdCallback) {
            _holdCallback(_holdState);
        }

        return true;
    }

    return false;
}

bool HAHVAC::setCurrentTemperature(double temperature)
{
    if (!(_features & CurrentTemperatureFeature)) {
        return;
    }

    if (_currentTemperature == temperature) {
        return true;
    }

    if (publishCurrentTemperature(temperature)) {
        _currentTemperature = temperature;
        return true;
    }

    return false;
}

bool HAHVAC::setMinTemp(double minTemp)
{
    if (minTemp >= 255 || minTemp <= -255) {
        return false;
    }

    _minTemp = minTemp;
    return true;
}

bool HAHVAC::setMaxTemp(double maxTemp)
{
    if (maxTemp >= 255 || maxTemp <= -255) {
        return false;
    }

    _maxTemp = maxTemp;
    return true;
}

bool HAHVAC::setTempStep(double tempStep)
{
    if (tempStep <= 0 || tempStep >= 255) {
        return false;
    }

    _tempStep = tempStep;
    return true;
}

bool HAHVAC::setTargetTemperature(double temperature)
{
    if (_targetTemperature == temperature) {
        return true;
    }

    if (publishTargetTemperature(temperature)) {
        _targetTemperature = temperature;

        if (_targetTempCallback) {
            _targetTempCallback(_targetTemperature);
        }

        return true;
    }

    return false;
}

void HAHVAC::publishConfig()
{
    const HADevice* device = mqtt()->getDevice();
    if (device == nullptr) {
        return;
    }

    const uint16_t& deviceLength = device->calculateSerializedLength();
    if (deviceLength == 0) {
        return;
    }

    char serializedDevice[deviceLength];
    if (device->serialize(serializedDevice) == 0) {
        return;
    }

    const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
        componentName(),
        name(),
        DeviceTypeSerializer::ConfigTopic
    );
    const uint16_t& dataLength = calculateSerializedLength(serializedDevice);

    if (topicLength == 0 || dataLength == 0) {
        return;
    }

    char topic[topicLength];
    DeviceTypeSerializer::generateTopic(
        topic,
        componentName(),
        name(),
        DeviceTypeSerializer::ConfigTopic
    );

    if (strlen(topic) == 0) {
        return;
    }

    if (mqtt()->beginPublish(topic, dataLength, true)) {
        writeSerializedData(serializedDevice);
        mqtt()->endPublish();
    }
}

bool HAHVAC::publishAction(Action action)
{
    if (strlen(name()) == 0) {
        return false;
    }

    char actionStr[8];
    switch (action) {
        case OffAction:
            strcpy(actionStr, "off");
            break;

        case HeatingAction:
            strcpy(actionStr, "heating");
            break;

        case CoolingAction:
            strcpy(actionStr, "cooling");
            break;

        case DryingAction:
            strcpy(actionStr, "drying");
            break;

        case IdleAction:
            strcpy(actionStr, "idle");
            break;

        case FanAction:
            strcpy(actionStr, "fan");
            break;

        default:
            return false;
    }

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        ActionTopic,
        actionStr
    );
}

bool HAHVAC::publishAuxHeatingState(bool state)
{
    if (!(_features & AuxHeatingFeature) ||
            strlen(name()) == 0) {
        return false;
    }

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        AuxStateTopic,
        (
            state ?
            DeviceTypeSerializer::StateOn :
            DeviceTypeSerializer::StateOff
        )
    );
}

bool HAHVAC::publishAwayState(bool state)
{
    if (!(_features & AwayModeFeature) ||
            strlen(name()) == 0) {
        return false;
    }

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        AwayStateTopic,
        (
            state ?
            DeviceTypeSerializer::StateOn :
            DeviceTypeSerializer::StateOff
        )
    );
}

bool HAHVAC::publishHoldState(bool state)
{
    if (!(_features & HoldFeature) ||
            strlen(name()) == 0) {
        return false;
    }

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        HoldStateTopic,
        (
            state ?
            DeviceTypeSerializer::StateOn :
            DeviceTypeSerializer::StateOff
        )
    );
}

bool HAHVAC::publishCurrentTemperature(double temperature)
{
    if (!(_features & CurrentTemperatureFeature) ||
            strlen(name()) == 0) {
        return false;
    }

    if (temperature >= 255) {
        return false;
    }

    char str[AHA_SERIALIZED_TEMP_SIZE];
    HAUtils::tempToStr(str, temperature);

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        CurrentTemperatureTopic,
        str
    );
}

bool HAHVAC::publishTargetTemperature(double temperature)
{
    if (strlen(name()) == 0) {
        return false;
    }

    if (temperature >= 255) {
        return false;
    }

    char str[AHA_SERIALIZED_TEMP_SIZE];
    HAUtils::tempToStr(str, temperature);

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        TargetTemperatureStateTopic,
        str
    );
}

void HAHVAC::subscribeTopics()
{
    // aux heating
    if (_features & AuxHeatingFeature) {
        DeviceTypeSerializer::mqttSubscribeTopic(
            this,
            AuxCommandTopic
        );
    }

    // away mode
    if (_features & AwayModeFeature) {
        DeviceTypeSerializer::mqttSubscribeTopic(
            this,
            AwayCommandTopic
        );
    }

    // hold
    if (_features & HoldFeature) {
        DeviceTypeSerializer::mqttSubscribeTopic(
            this,
            HoldCommandTopic
        );
    }

    DeviceTypeSerializer::mqttSubscribeTopic(
        this,
        TargetTemperatureCommandTopic
    );
}

uint16_t HAHVAC::calculateSerializedLength(const char* serializedDevice) const
{
    if (serializedDevice == nullptr || _uniqueId == nullptr) {
        return 0;
    }

    const HADevice* device = mqtt()->getDevice();
    if (device == nullptr) {
        return 0;
    }

    uint16_t size = 0;
    size += DeviceTypeSerializer::calculateBaseJsonDataSize();
    size += DeviceTypeSerializer::calculateUniqueIdFieldSize(_uniqueId);
    size += DeviceTypeSerializer::calculateDeviceFieldSize(serializedDevice);
    size += DeviceTypeSerializer::calculateAvailabilityFieldSize(this);

    // action topic
    {
        const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
            componentName(),
            name(),
            ActionTopic,
            false
        );

        if (topicLength == 0) {
            return 0;
        }

        // Field format: "act_t":"[TOPIC]"
        size += topicLength + 10; // 10 - length of the JSON decorators for this field
    }

    // aux heating
    if (_features & AuxHeatingFeature) {
        // command topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                AuxCommandTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"aux_cmd_t":"[TOPIC]"
            size += topicLength + 15; // 15 - length of the JSON decorators for this field
        }

        // state topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                AuxStateTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"aux_stat_t":"[TOPIC]"
            size += topicLength + 16; // 16 - length of the JSON decorators for this field
        }
    }

    // away mode
    if (_features & AwayModeFeature) {
        // command topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                AwayCommandTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"away_mode_cmd_t":"[TOPIC]"
            size += topicLength + 21; // 21 - length of the JSON decorators for this field
        }

        // state topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                AwayStateTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"away_mode_stat_t":"[TOPIC]"
            size += topicLength + 22; // 22 - length of the JSON decorators for this field
        }
    }

    // hold
    if (_features & HoldFeature) {
        // command topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                HoldCommandTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"hold_cmd_t":"[TOPIC]"
            size += topicLength + 16; // 16 - length of the JSON decorators for this field
        }

        // state topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                HoldStateTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"hold_stat_t":"[TOPIC]"
            size += topicLength + 17; // 17 - length of the JSON decorators for this field
        }
    }

    // current temperature
    if (_features & CurrentTemperatureFeature) {
        const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
            componentName(),
            name(),
            CurrentTemperatureTopic,
            false
        );

        if (topicLength == 0) {
            return 0;
        }

        // Field format: ,"curr_temp_t":"[TOPIC]"
        size += topicLength + 17; // 17 - length of the JSON decorators for this field
    }

    // min temp
    if (_minTemp != 255) {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _minTemp);

        // Field format: ,"min_temp":"[TEMP]"
        size += strlen(str) + 14; // 14 - length of the JSON decorators for this field
    }

    // max temp
    if (_maxTemp != 255) {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _maxTemp);

        // Field format: ,"max_temp":"[TEMP]"
        size += strlen(str) + 14; // 14 - length of the JSON decorators for this field
    }

    // temp step
    {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _tempStep);

        // Field format: ,"temp_step":"[TEMP]"
        size += strlen(str) + 15; // 15 - length of the JSON decorators for this field
    }

    // name
    if (_label != nullptr) {
        // Field format: ,"name":"[NAME]"
        size += strlen(_label) + 10; // 15 - length of the JSON decorators for this field
    }

    // target temperature
    {
        // command topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                TargetTemperatureCommandTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"temp_cmd_t":"[TOPIC]"
            size += topicLength + 16; // 16 - length of the JSON decorators for this field
        }

        // state topic
        {
            const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
                componentName(),
                name(),
                TargetTemperatureStateTopic,
                false
            );

            if (topicLength == 0) {
                return 0;
            }

            // Field format: ,"temp_stat_t":"[TOPIC]"
            size += topicLength + 17; // 17 - length of the JSON decorators for this field
        }
    }

    // temperature unit
    if (_temperatureUnit != DefaultUnit) {
        // Field format: ,"temp_unit":"[UNIT]"
        // UNIT may C or F
        size += 15 + 1; // 15 - length of the JSON decorators for this field
    }

    return size; // exludes null terminator
}

bool HAHVAC::writeSerializedData(const char* serializedDevice) const
{
    if (serializedDevice == nullptr || _uniqueId == nullptr) {
        return false;
    }

    DeviceTypeSerializer::mqttWriteBeginningJson();

    // action topic
    {
        static const char Prefix[] PROGMEM = {"\"act_t\":\""};
        DeviceTypeSerializer::mqttWriteTopicField(
            this,
            Prefix,
            ActionTopic
        );
    }

    // aux heating
    if (_features & AuxHeatingFeature) {
        // command topic
        {
            static const char Prefix[] PROGMEM = {",\"aux_cmd_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                AuxCommandTopic
            );
        }

        // state topic
        {
            static const char Prefix[] PROGMEM = {",\"aux_stat_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                AuxStateTopic
            );
        }
    }

    // away mode
    if (_features & AwayModeFeature) {
        // command topic
        {
            static const char Prefix[] PROGMEM = {",\"away_mode_cmd_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                AwayCommandTopic
            );
        }

        // state topic
        {
            static const char Prefix[] PROGMEM = {",\"away_mode_stat_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                AwayStateTopic
            );
        }
    }

    // hold
    if (_features & HoldFeature) {
        // command topic
        {
            static const char Prefix[] PROGMEM = {",\"hold_cmd_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                HoldCommandTopic
            );
        }

        // state topic
        {
            static const char Prefix[] PROGMEM = {",\"hold_stat_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                HoldStateTopic
            );
        }
    }

    // current temperature topic
    if (_features & CurrentTemperatureFeature) {
        static const char Prefix[] PROGMEM = {",\"curr_temp_t\":\""};
        DeviceTypeSerializer::mqttWriteTopicField(
            this,
            Prefix,
            CurrentTemperatureTopic
        );
    }

    // min temp
    if (_minTemp != 255) {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _minTemp);

        static const char Prefix[] PROGMEM = {",\"min_temp\":\""};
        DeviceTypeSerializer::mqttWriteConstCharField(
            Prefix,
            str
        );
    }

    // max temp
    if (_maxTemp != 255) {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _maxTemp);

        static const char Prefix[] PROGMEM = {",\"max_temp\":\""};
        DeviceTypeSerializer::mqttWriteConstCharField(
            Prefix,
            str
        );
    }

    // temp step
    {
        char str[AHA_SERIALIZED_TEMP_SIZE];
        HAUtils::tempToStr(str, _tempStep);

        static const char Prefix[] PROGMEM = {",\"temp_step\":\""};
        DeviceTypeSerializer::mqttWriteConstCharField(
            Prefix,
            str
        );
    }

    // label (name)
    if (_label != nullptr) {
        static const char Prefix[] PROGMEM = {",\"name\":\""};
        DeviceTypeSerializer::mqttWriteConstCharField(
            Prefix,
            _label
        );
    }

    // target temperature
    {
        // command topic
        {
            static const char Prefix[] PROGMEM = {",\"temp_cmd_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                TargetTemperatureCommandTopic
            );
        }

        // state topic
        {
            static const char Prefix[] PROGMEM = {",\"temp_stat_t\":\""};
            DeviceTypeSerializer::mqttWriteTopicField(
                this,
                Prefix,
                TargetTemperatureStateTopic
            );
        }
    }

    // temperature unit
    if (_temperatureUnit != DefaultUnit) {
        static const char Prefix[] PROGMEM = {",\"temp_unit\":\""};
        DeviceTypeSerializer::mqttWriteConstCharField(
            Prefix,
            (_temperatureUnit == CelsiusUnit ? "C" : "F")
        );
    }

    DeviceTypeSerializer::mqttWriteUniqueIdField(_uniqueId);
    DeviceTypeSerializer::mqttWriteAvailabilityField(this);
    DeviceTypeSerializer::mqttWriteDeviceField(serializedDevice);
    DeviceTypeSerializer::mqttWriteEndJson();
}
