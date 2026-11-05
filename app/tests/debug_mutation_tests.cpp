#include "cycode/DebugInspection.h"
#include "cycode/DebugMutations.h"
#include <functional>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void rejects(const std::function<void()> &action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid debugger mutation contract was accepted");
}
} // namespace

void debugMutationContracts() {
    using namespace cycode;
    const Json caps = {{"supportsSetVariable", true},
                       {"supportsSetExpression", true},
                       {"supportsWriteMemoryRequest", true},
                       {"supportsReadMemoryRequest", true}};
    const Json variable = {{"sessionGeneration", 3},
                           {"stopGeneration", 7},
                           {"variablesReference", 17},
                           {"name", "value"},
                           {"value", ""}};
    const Json expression = {{"sessionGeneration", 3},
                             {"stopGeneration", 7},
                             {"frameId", 9},
                             {"expression", "this->value"},
                             {"value", "other()"}};
    const auto assign = debugMutationArguments("debug.setVariable", variable, caps, 9);
    require(assign == Json({{"variablesReference", 17}, {"name", "value"}, {"value", ""}}),
            "Variable assignment lost empty expressions or leaked host guards to the adapter");
    require(debugMutationArguments("debug.setExpression", expression, caps, 9) ==
                Json({{"expression", "this->value"}, {"value", "other()"}, {"frameId", 9}}),
            "Expression assignment changed its captured frame or expression");
    rejects([&] { debugMutationArguments("debug.setExpression", expression, caps, 10); });
    for (const auto *method : {"debug.setVariable", "debug.setExpression", "debug.writeMemory"})
        require(debugMutationOperation(method) && stoppedDebugOperation(method),
                "A target mutation bypassed the paused-stop guard");
    require(!debugMutationOperation("debug.evaluate"), "Inspection was classified as target assignment");
    for (const auto &update :
         {Json{{"sessionGeneration", 0}}, Json{{"stopGeneration", -1}}, Json{{"sessionGeneration", true}},
          Json{{"stopGeneration", "7"}}, Json{{"variablesReference", 0}}, Json{{"variablesReference", true}},
          Json{{"name", ""}}, Json{{"name", std::string("x\0y", 3)}}, Json{{"value", std::string(8193, 'x')}},
          Json{{"value", 17}}, Json{{"frameId", 9}}, Json{{"command", "evaluate"}}}) {
        auto invalid = variable;
        invalid.update(update);
        rejects([&] { debugMutationArguments("debug.setVariable", invalid, caps, 9); });
    }
    for (const auto *key : {"sessionGeneration", "stopGeneration", "name", "value", "variablesReference"}) {
        auto invalid = variable;
        invalid.erase(key);
        rejects([&] { debugMutationArguments("debug.setVariable", invalid, caps, 9); });
    }
    for (const auto &update :
         {Json{{"frameId", -1}}, Json{{"frameId", true}}, Json{{"expression", ""}},
          Json{{"expression", std::string(4097, 'x')}}, Json{{"variablesReference", 17}}}) {
        auto invalid = expression;
        invalid.update(update);
        rejects([&] { debugMutationArguments("debug.setExpression", invalid, caps, 9); });
    }
    auto noFrame = expression;
    noFrame.erase("frameId");
    rejects([&] { debugMutationArguments("debug.setExpression", noFrame, caps, 9); });
    for (const auto *cap : {"supportsSetVariable", "supportsSetExpression"}) {
        auto missing = caps;
        missing.erase(cap);
        const bool isVariable = std::string(cap) == "supportsSetVariable";
        rejects([&] {
            debugMutationArguments(isVariable ? "debug.setVariable" : "debug.setExpression",
                                   isVariable ? variable : expression, missing, 9);
        });
    }
    Json writable = {{"name", "value"}, {"evaluateName", "this->value"}, {"value", "1"}};
    require(debugVariableEditMethod(writable, caps) == "debug.setExpression",
            "A usable evaluateName did not select the DAP-preferred expression setter");
    writable.erase("evaluateName");
    require(debugVariableEditMethod(writable, caps) == "debug.setVariable",
            "A child without evaluateName lost variable assignment");
    writable["presentationHint"] = {{"attributes", Json::array({"readOnly"})}};
    require(debugVariableEditMethod(writable, caps).empty(), "The UI offered to edit a readOnly variable");
    for (const auto &hint : {Json(nullptr), Json(17), Json{{"attributes", true}}}) {
        writable["presentationHint"] = hint;
        require(debugVariableEditMethod(writable, caps).empty(),
                "Malformed presentation metadata enabled assignment");
    }
    writable.erase("presentationHint");
    require(debugVariableEditMethod(writable, Json::object()).empty(),
            "Assignment was offered without a capability");
    const auto value = debugMutationResult("debug.setExpression",
                                           {{"value", "42"},
                                            {"type", "int"},
                                            {"variablesReference", 123},
                                            {"memoryReference", "opaque"},
                                            {"namedVariables", 1},
                                            {"indexedVariables", 2}},
                                           expression);
    require(value == Json({{"value", "42"}, {"type", "int"}}),
            "Mutation responses exposed references that expire during inspection refresh");
    for (const auto &body :
         {Json::object(), Json{{"value", 42}}, Json{{"value", "1"}, {"type", true}},
          Json{{"value", "1"}, {"variablesReference", -1}}, Json{{"value", "1"}, {"memoryReference", 4}},
          Json{{"value", std::string("x\0y", 3)}}, Json{{"value", std::string(65537, 'x')}}})
        rejects([&] { debugMutationResult("debug.setVariable", body, variable); });
    Json memory = {{"sessionGeneration", 3},
                   {"stopGeneration", 7},
                   {"memoryReference", "opaque:allocation"},
                   {"offset", -16},
                   {"bytes", Json::array({0, 255, 42})},
                   {"expectedBytes", Json::array({1, 2, 3})}};
    require(debugMutationArguments("debug.writeMemory", memory, caps, 9) ==
                Json({{"memoryReference", "opaque:allocation"},
                      {"offset", -16},
                      {"data", "AP8q"},
                      {"allowPartial", false}}),
            "Memory writes changed opaque addressing, lost bytes or enabled partial writes");
    for (const auto &sample : {Json{{"bytes", Json::array({0})}, {"data", "AA=="}},
                               Json{{"bytes", Json::array({0, 255})}, {"data", "AP8="}},
                               Json{{"bytes", Json::array({0, 255, 42, 128})}, {"data", "AP8qgA=="}}}) {
        auto request = memory;
        request["bytes"] = request["expectedBytes"] = sample.at("bytes");
        const auto wire = debugMutationArguments("debug.writeMemory", request, caps, 9);
        require(wire.at("data") == sample.at("data"),
                "Memory byte encoding used invalid padding or ordering");
        require(debugInspectionResult("debug.memory", {{"address", "0x100"}, {"data", wire.at("data")}},
                                      {{"count", request.at("bytes").size()}})
                        .at("bytes") == request.at("bytes"),
                "Memory reads and writes disagree on target byte values");
    }
    for (const auto &update :
         {Json{{"bytes", Json::array()}}, Json{{"bytes", Json::array({256, 0, 0})}},
          Json{{"bytes", Json::array({-1, 0, 0})}}, Json{{"bytes", Json::array({true, 0, 0})}},
          Json{{"expectedBytes", Json::array({0})}}, Json{{"expectedBytes", "00 00 00"}},
          Json{{"offset", -1048577}}, Json{{"offset", 1048577}}, Json{{"offset", true}},
          Json{{"memoryReference", ""}}, Json{{"allowPartial", true}}, Json{{"data", "raw-base64"}}}) {
        auto invalid = memory;
        invalid.update(update);
        rejects([&] { debugMutationArguments("debug.writeMemory", invalid, caps, 9); });
    }
    for (const auto *cap : {"supportsReadMemoryRequest", "supportsWriteMemoryRequest"}) {
        auto missing = caps;
        missing.erase(cap);
        rejects([&] { debugMutationArguments("debug.writeMemory", memory, missing, 9); });
    }
    Json large = Json::array();
    for (int index = 0; index < 4096; ++index)
        large.push_back(index % 256);
    auto maximum = memory;
    maximum["bytes"] = maximum["expectedBytes"] = large;
    require(
        debugMutationArguments("debug.writeMemory", maximum, caps, 9).at("data").get<std::string>().size() ==
            5464,
        "The full 4096-byte memory write bound was not supported");
    maximum["bytes"].push_back(0);
    maximum["expectedBytes"].push_back(0);
    rejects([&] { debugMutationArguments("debug.writeMemory", maximum, caps, 9); });
    require(debugMutationResult("debug.writeMemory", Json::object(), memory).empty(),
            "An omitted adapter byte count was fabricated");
    require(debugMutationResult("debug.writeMemory", {{"bytesWritten", 1}, {"offset", -16}}, memory)
                    .at("bytesWritten") == 1,
            "A partial memory result was changed to full success");
    for (const auto &body :
         {Json{{"bytesWritten", -1}}, Json{{"bytesWritten", 4}}, Json{{"bytesWritten", true}},
          Json{{"offset", 0}}, Json{{"offset", true}}, Json::array()})
        rejects([&] { debugMutationResult("debug.writeMemory", body, memory); });
    require(debugHexBytes("00 fF\t2a\r\n80") == Json::array({0, 255, 42, 128}),
            "Hex input changed byte values or ignored uppercase/whitespace syntax");
    for (const auto &input :
         {std::string{}, std::string(" \n"), std::string("0"), std::string("0xFF"), std::string("AA G0"),
          std::string("FF,00"), std::string(8194, '0'), std::string(16385, ' ')})
        rejects([&] { debugHexBytes(input); });
}
