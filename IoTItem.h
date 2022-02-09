#pragma once

#include <string>

struct IoTValue
{
    float valD = 0;
    std::string valS = "";
    bool isDecimal = true;
};


class IoTItem {
   public:
    IoTItem(std::string parameters);
    ~IoTItem();

    void loop();
    virtual void doByInterval();
    virtual IoTValue* getValue();
    virtual void execute(std::string command, std::string param);

    void regEvent(std::string value, std::string consoleInfo);
    void regEvent(float value, std::string consoleInfo);

    std::string getSubtype();
    std::string getID();

    unsigned long currentMillis;
    unsigned long prevMillis;
    unsigned long difference;

   protected:
    std::string _subtype;
    std::string _id;
    unsigned long _interval;
    
    float _multiply;  // умножаем на значение
    float _plus;  // увеличиваем на значение
    int _map1;
    int _map2;
    int _map3;
    int _map4;
    int _round;  // 1, 10, 100, 1000, 10000

    IoTValue value;
};
