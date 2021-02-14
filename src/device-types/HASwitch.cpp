#include "HASwitch.h"
#include "../ArduinoHADefines.h"
#include "../HAMqtt.h"
#include "../HADevice.h"
#include "../HAUtils.h"

HASwitch::HASwitch(const char* name, bool initialState, HAMqtt& mqtt) :
    BaseDeviceType("switch", name),
    _stateCallback(nullptr),
    _currentState(initialState)
{

}

void HASwitch::onMqttConnected()
{
    if (strlen(name()) == 0) {
        return;
    }

    publishConfig();
    publishState(_currentState);
    publishAvailability();

    DeviceTypeSerializer::mqttSubscribeTopic(
        this,
        DeviceTypeSerializer::CommandTopic
    );
}

void HASwitch::onMqttMessage(
    const char* topic,
    const uint8_t* payload,
    const uint16_t& length
)
{
    if (strlen(name()) == 0) {
        return;
    }

    static const char Slash[] PROGMEM = {"/"};
    // name + cmd topic + two slashes + null terminator
    uint8_t suffixLength = strlen(name()) + strlen(DeviceTypeSerializer::CommandTopic) + 3;
    char suffix[suffixLength];

    strcpy_P(suffix, Slash);
    strcat(suffix, name());
    strcat_P(suffix, Slash);
    strcat(suffix, DeviceTypeSerializer::CommandTopic);

    if (HAUtils::endsWith(topic, suffix)) {
        bool onState = (length == strlen(DeviceTypeSerializer::StateOn));
        setState(onState);
    }
}

bool HASwitch::setState(bool state)
{
    if (_currentState == state) {
        return true;
    }

    if (publishState(state)) {
        _currentState = state;

        if (_stateCallback) {
            _stateCallback(state, this);
        }

        return true;
    }

    return false;
}

void HASwitch::publishConfig()
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

bool HASwitch::publishState(bool state)
{
    if (strlen(name()) == 0) {
        return false;
    }

    return DeviceTypeSerializer::mqttPublishMessage(
        this,
        DeviceTypeSerializer::StateTopic,
        (
            state ?
            DeviceTypeSerializer::StateOn :
            DeviceTypeSerializer::StateOff
        )
    );
}

uint16_t HASwitch::calculateSerializedLength(const char* serializedDevice) const
{
    if (serializedDevice == nullptr) {
        return 0;
    }

    const HADevice* device = mqtt()->getDevice();
    if (device == nullptr) {
        return 0;
    }

    uint16_t size = 0;
    size += DeviceTypeSerializer::calculateBaseJsonDataSize();
    size += DeviceTypeSerializer::calculateNameFieldSize(name());
    size += DeviceTypeSerializer::calculateUniqueIdFieldSize(device, name());
    size += DeviceTypeSerializer::calculateDeviceFieldSize(serializedDevice);
    size += DeviceTypeSerializer::calculateAvailabilityFieldSize(this);

    // cmd topic
    {
        const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
            componentName(),
            name(),
            DeviceTypeSerializer::CommandTopic,
            false
        );

        if (topicLength == 0) {
            return 0;
        }

        // Field format: "cmd_t":"[TOPIC]"
        size += topicLength + 10; // 10 - length of the JSON decorators for this field
    }

    // state topic
    {
        const uint16_t& topicLength = DeviceTypeSerializer::calculateTopicLength(
            componentName(),
            name(),
            DeviceTypeSerializer::StateTopic,
            false
        );

        if (topicLength == 0) {
            return 0;
        }

        // Field format: ,"stat_t":"[TOPIC]"
        size += topicLength + 12; // 12 - length of the JSON decorators for this field
    }

    return size; // exludes null terminator
}

bool HASwitch::writeSerializedData(const char* serializedDevice) const
{
    if (serializedDevice == nullptr) {
        return false;
    }

    DeviceTypeSerializer::mqttWriteBeginningJson();

    // command topic
    {
        static const char Prefix[] PROGMEM = {"\"cmd_t\":\""};
        DeviceTypeSerializer::mqttWriteTopicField(
            this,
            Prefix,
            DeviceTypeSerializer::CommandTopic
        );
    }

    // state topic
    {
        static const char Prefix[] PROGMEM = {",\"stat_t\":\""};
        DeviceTypeSerializer::mqttWriteTopicField(
            this,
            Prefix,
            DeviceTypeSerializer::StateTopic
        );
    }

    DeviceTypeSerializer::mqttWriteNameField(name());
    DeviceTypeSerializer::mqttWriteUniqueIdField(name());
    DeviceTypeSerializer::mqttWriteAvailabilityField(this);
    DeviceTypeSerializer::mqttWriteDeviceField(serializedDevice);
    DeviceTypeSerializer::mqttWriteEndJson();

    return true;
}
