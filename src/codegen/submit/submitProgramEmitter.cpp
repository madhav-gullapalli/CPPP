#include "submitProgramEmitter.h"

#include "programEmitter.h"
#include "submitPostProcessor.h"
#include "typesCppp.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::set<std::string> requiredContainerTypes(const CompileContext& context) {
    std::set<std::string> types;
    const auto inspect = [&](const std::vector<GeneratedLine>& lines) {
        for (const GeneratedLine& line : lines) {
            if (line.text.find("CPPPPair<") != std::string::npos) types.insert("CPPPPair");
            if (line.text.find("CPPPList<") != std::string::npos) types.insert("CPPPList");
            if (line.text.find("CPPPStack<") != std::string::npos) types.insert("CPPPStack");
            if (line.text.find("CPPPQueue<") != std::string::npos) types.insert("CPPPQueue");
            if (line.text.find("CPPPDeque<") != std::string::npos) types.insert("CPPPDeque");
            if (line.text.find("CPPPHeap<") != std::string::npos) types.insert("CPPPHeap");
            if (line.text.find("CPPPSet<") != std::string::npos) types.insert("CPPPSet");
            if (line.text.find("CPPPMap<") != std::string::npos) types.insert("CPPPMap");
        }
    };
    inspect(context.generatedTopLevelLines);
    inspect(context.generatedFunctionLines);
    inspect(context.generatedMainLines);
    return types;
}

std::set<std::string> requiredMembers(const CompileContext& context) {
    std::set<std::string> members = requiredContainerMembers();
    const auto inspect = [&](const std::vector<GeneratedLine>& lines) {
        for (const GeneratedLine& line : lines) {
            const std::vector<std::string> types = {
                "CPPPPair", "CPPPList", "CPPPStack", "CPPPQueue", "CPPPDeque", "CPPPHeap", "CPPPSet", "CPPPMap"
            };
            for (const std::string& type : types) {
                if (line.text.find(type + "<") == std::string::npos) continue;
                if (line.text.find("= {}") != std::string::npos || line.text.find(">{}") != std::string::npos) {
                    members.insert(type + ".ctor_default");
                }
                if ((type == "CPPPList" || type == "CPPPSet" || type == "CPPPMap") &&
                    line.text.find(">{") != std::string::npos && line.text.find(">{}") == std::string::npos) {
                    members.insert(type + ".ctor_init");
                }
                if (type == "CPPPPair" &&
                    (line.text.find("= {") != std::string::npos || line.text.find(">(") != std::string::npos)) {
                    members.insert(type + ".ctor_values");
                }
            }
            if (line.text.find(" = CPPPList<") != std::string::npos && line.text.find(">(") != std::string::npos) {
                members.insert("CPPPList.ctor_size");
                members.insert("CPPPList.ctor_default");
                if (line.text.find("CPPPMap<") != std::string::npos) members.insert("CPPPMap.ctor_default");
                if (line.text.find("CPPPSet<") != std::string::npos) members.insert("CPPPSet.ctor_default");
                if (line.text.find("CPPPPair<") != std::string::npos) members.insert("CPPPPair.ctor_default");
            }
        }
    };
    inspect(context.generatedTopLevelLines);
    inspect(context.generatedFunctionLines);
    inspect(context.generatedMainLines);
    return members;
}
}

void emitSubmitProgram(std::ostream& output, CompileContext& context) {
    const std::vector<std::string> preamble = typeSupportPreambleForSubmit(
        requiredRuntimeHelpers(),
        requiredContainerTypes(context),
        requiredMembers(context)
    );
    if (context.options.readableSubmit) {
        emitLoweredProgram(output, context, preamble, false);
        return;
    }
    std::ostringstream readable;
    emitLoweredProgram(readable, context, preamble, false);
    output << compactSubmitCpp(readable.str());
}
