#include "cycode/DataBreakpoints.h"
#include <functional>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void rejects(const std::function<void()> &work) {
    bool failed = false; try { work(); } catch (const std::exception &) { failed = true; }
    require(failed,"Invalid data breakpoint contract was accepted");
}
}
void dataBreakpointContracts() {
    using cycode::DataBreakpoints;
    const Json caps = {{"supportsDataBreakpoints",true},{"supportsConditionalBreakpoints",true},{"supportsHitConditionalBreakpoints",true}};
    DataBreakpoints store;
    const auto arguments = DataBreakpoints::arguments({{"name","value"}},caps,17);
    require(arguments == Json{{"name","value"},{"frameId",17}},"Expression was not bound to the selected paused frame");
    require(DataBreakpoints::arguments({{"name","child"},{"variablesReference",9}},caps,17) == Json{{"name","child"},{"variablesReference",9}},"Variable container was replaced by a frame");
    rejects([&]{DataBreakpoints::arguments({{"name","value"}},Json::object(),17);});
    rejects([&]{DataBreakpoints::arguments({{"name","value"},{"variablesReference",true}},caps,17);});
    const Json body = {{"dataId","opaque-target"},{"description","value"},{"accessTypes",{"read","write"}},{"canPersist",true}};
    const auto info = store.capture(arguments,body,3,4);
    require(info.at("available") == true && !info.contains("dataId") && info.at("canPersist") == true,"Raw adapter identity leaked or metadata changed");
    const auto points = Json::array({{{"id",info.at("id")},{"accessType","write"},{"condition","value > 2"},{"hitCondition","3"}}});
    const auto plan = store.prepare(points,caps,3,4);
    require(plan.at("arguments").at("breakpoints")[0].at("dataId") == "opaque-target","Issued target did not resolve to actual dataId");
    rejects([&]{store.prepare(points,caps,3,5);});
    rejects([&]{store.prepare(points,caps,4,4);});
    auto unsupported = points; unsupported[0]["accessType"] = "readWrite";
    rejects([&]{store.prepare(unsupported,caps,3,4);});
    auto forbidden = points; forbidden[0]["dataId"] = "forged";
    rejects([&]{store.prepare(forbidden,caps,3,4);});
    auto unconditioned = caps; unconditioned["supportsConditionalBreakpoints"] = false;
    rejects([&]{store.prepare(points,unconditioned,3,4);});
    const auto alias = store.capture(arguments,body,3,4);
    rejects([&]{store.prepare(Json::array({{{"id",info.at("id")}},{{"id",alias.at("id")}}}),caps,3,4);});
    store.beginReplace(0);
    require(store.snapshot().at("dataBreakpointsSynchronized") == false,"In-flight mutation was reported synchronized");
    rejects([&]{store.beginReplace(0);});
    rejects([&]{store.complete(plan,{{"breakpoints",Json::array()}});});
    require(store.snapshot().at("dataBreakpointsSynchronized") == false,"Malformed verification restored certainty");
    store.complete(plan,{{"breakpoints",Json::array({{{"id",20},{"verified",false},{"message","hardware limit"}}})}});
    require(store.snapshot().at("dataBreakpoints")[0].at("verification").at("verified") == false,"Unverified target was reported armed");
    require(store.breakpointEvent({{"id",20},{"verified",true}}),"Deferred verification was not matched");
    store.expire();
    require(store.snapshot().at("dataBreakpoints").size() == 1,"Resume erased installed breakpoints");
    rejects([&]{store.prepare(points,caps,3,4);});
    require(store.breakpointEvent({{"id",20}},true) && store.snapshot().at("dataBreakpoints")[0].at("verification").at("verified") == false,"Removed event kept a breakpoint verified");
    const auto empty = store.prepare(Json::array(),caps,3,5);
    store.beginReplace(store.snapshot().at("dataBreakpointRevision"));
    store.complete(empty,{{"breakpoints",Json::array()}});
    require(store.snapshot().at("dataBreakpoints").empty() && store.snapshot().at("dataBreakpointsSynchronized") == true,"Clear did not replace the full list");
    const auto missing = store.capture(arguments,{{"dataId",nullptr},{"description","not addressable"}},3,5);
    require(missing.at("available") == false && missing.at("id") == "","Unavailable data target received a handle");
    const auto defaults = store.capture(arguments,{{"dataId","default-access"},{"description","value"}},3,5);
    const auto defaultPlan = store.prepare(Json::array({{{"id",defaults.at("id")}}}),caps,3,5);
    require(!defaultPlan.at("arguments").at("breakpoints")[0].contains("accessType"),"Missing access catalogue invented a write mode");
    rejects([&]{store.prepare(Json::array({{{"id",defaults.at("id")},{"accessType","write"}}}),caps,3,5);});
    for (const auto &bad : {Json{{"dataId",true},{"description","x"}},Json{{"dataId",""},{"description","x"}},
                           Json{{"dataId","x"},{"description","x"},{"accessTypes",{"write","write"}}},
                           Json{{"dataId","x"},{"description","x"},{"canPersist",1}}}) rejects([&]{store.capture(arguments,bad,3,5);});
    auto tooMany = Json::array(); for (int i=0;i<33;++i) tooMany.push_back({{"id",defaults.at("id")}});
    rejects([&]{store.prepare(tooMany,caps,3,5);});
    DataBreakpoints bounded;
    const auto firstBounded = bounded.capture(arguments,body,1,1);
    for (int i=1;i<128;++i) bounded.capture(arguments,body,1,1);
    rejects([&]{bounded.capture(arguments,body,1,1);});
    bounded.reset();
    require(bounded.snapshot().at("dataBreakpoints").empty(),"Reset preserved data targets across sessions");
    const auto next = bounded.capture(arguments,body,1,1);
    require(next.at("id") != firstBounded.at("id"),"Issued handle sequence was reused after reset");
    DataBreakpoints byteBounded;
    const Json large = {{"dataId",std::string(8192,'x')},{"description",std::string(8192,'y')}};
    int accepted = 0;
    for (; accepted < 128; ++accepted) {
        bool failed = false; try { byteBounded.capture(arguments,large,1,1); } catch (const std::exception &) { failed = true; }
        if (failed) break;
    }
    require(accepted > 0 && accepted < 128,"Aggregate inspection byte budget was not enforced");
}
