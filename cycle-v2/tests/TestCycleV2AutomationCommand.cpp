#include <catch2/catch_test_macros.hpp>

#include "App/CycleV2AutomationCommand.h"

using namespace CycleV2;

TEST_CASE("Automation command registry owns aliases",
        "[cycle-v2][automation][commands]") {
    REQUIRE(automationCommandForName("connect")
            == CycleV2AutomationCommand::ConnectPorts);
    REQUIRE(automationCommandForName("connectPorts")
            == CycleV2AutomationCommand::ConnectPorts);
    REQUIRE(automationCommandForName("openMeshPopup")
            == CycleV2AutomationCommand::OpenNodeEditor);
    REQUIRE(automationCommandForName("removeGuideCurve")
            == CycleV2AutomationCommand::DeleteGuideCurve);
    REQUIRE(automationCommandForName("recreateCanvasOpenGLContext")
            == CycleV2AutomationCommand::RecreateCanvasOpenGLContext);
    REQUIRE_FALSE(automationCommandForName("missing").has_value());
}
