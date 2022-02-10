#include "IoTItem.h"

IoTItem::IoTItem(std::string parameters) {
    // jsonRead(parameters, "int", _interval);
    // _interval = _interval * 1000;
    // jsonRead(parameters, "subtype", _subtype);
    // jsonRead(parameters, "id", _id);
    // jsonRead(parameters, "multiply", _multiply);
    // jsonRead(parameters, "plus", _plus);
    // jsonRead(parameters, "round", _round);

    // std::string map;
    // jsonRead(parameters, "map", map);
    // if (map != "") {
    //     // _map1 = selectFromMarkerToMarker(map, ",", 0).toInt();
    //     // _map2 = selectFromMarkerToMarker(map, ",", 1).toInt();
    //     // _map3 = selectFromMarkerToMarker(map, ",", 2).toInt();
    //     // _map4 = selectFromMarkerToMarker(map, ",", 3).toInt();
    // }
}
IoTItem::~IoTItem() {}

std::string IoTItem::getSubtype() {
    return _subtype;
}

std::string IoTItem::getID() {
    return _id;
};

void IoTItem::loop() {
    // currentMillis = millis();
    // difference = currentMillis - prevMillis;
    // if (difference >= _interval) {
    //     prevMillis = millis();
    //     this->doByInterval();
    // }
}

void IoTItem::regEvent(std::string value, std::string consoleInfo = "") {
    // eventGen2(_id, value);
    // jsonWriteStr(paramsFlashJson, _id, value);
    // publishStatusMqtt(_id, value);
    // publishStatusWs(_id, value);
    // SerialPrint("I", "Sensor " + consoleInfo, "'" + _id + "' data: " + value + "'");
}

void IoTItem::regEvent(float value, std::string consoleInfo = "") {
    // if (_multiply) value = value * _multiply;
    // if (_plus) value = value + _multiply;
    // if (_round != 0) {
    //     if (value > 0) {
    //         value = (int)(value * _round + 0.5F);
    //         value = value / _round;
    //     }
    //     if (value < 0) {
    //         value = (int)(value * _round - 0.5F);
    //         value = value / _round;
    //     }
    // }
    // if (_map1 != _map2) value = map(value, _map1, _map2, _map3, _map4);

    // // убираем лишние нули
    // char buf[20];
    // sprintf(buf, "%g", value);
    // regEvent((std::string)buf, consoleInfo);
}

void IoTItem::doByInterval() {}

IoTValue* IoTItem::getValue() { value.valD = 10; return &value;}  // заглушка для получения значения переменных в сценарии

IoTValue IoTItem::execute(std::string command, std::string param) { return IoTValue{20, "", true}} // заглушка для получения значения результата работы команды

IoTItem* myIoTItem;