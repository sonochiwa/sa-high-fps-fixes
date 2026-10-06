#include "scenarios.h"

#include "scenario_tools.h"

#include <array>
#include <cstring>

namespace scenarios {

const harness::Scenario* Find(const char* name) {
    const std::array<ScenarioList, 4> lists{
        WorldScenarios(), VehicleScenarios(), AircraftScenarios(), PedScenarios()
    };
    for (const ScenarioList& list : lists) {
        for (size_t i = 0; i < list.count; ++i) {
            if (std::strcmp(list.first[i].name, name) == 0) {
                return &list.first[i];
            }
        }
    }
    return nullptr;
}

}  // namespace scenarios
