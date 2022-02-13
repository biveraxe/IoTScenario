#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>

#include <map>
#include <vector>
#include "IoTScenario.h"
#include "IoTItem.h"




//===----------------------------------------------------------------------===//
// Main driver code (Код основной программы)
//===----------------------------------------------------------------------===//

IoTScenario iotScen;

int main() {
  
  iotScen.loadScenario("scenario.txt");
  iotScen.ExecScenario();

// имитируем обновление сценария после изменения
  iotScen.loadScenario("d:\\IoTScenario\\scenario.txt");
  iotScen.ExecScenario();

// имитируем обновление сценария после изменения
  iotScen.loadScenario("d:\\IoTScenario\\scenario.txt");
  iotScen.ExecScenario();

  return 0;
}