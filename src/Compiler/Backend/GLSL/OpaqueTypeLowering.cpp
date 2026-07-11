/*
 * OpaqueTypeLowering.cpp
 *
 * Driver for the opaque-type lowering pass. The pass intentionally does not
 * encode aggregate locations as strings. A declaration-backed layout is
 * built first (OpaqueTypeLayout.*), symbolic resource bindings are solved
 * against the untouched program (OpaqueProgramAnalyzer.*), and only then is
 * the AST rewritten (OpaqueASTLowerer.*). The data structures exchanged
 * between the two phases live in OpaqueProgramPlan.*.
 */

#include "OpaqueTypeLowering.h"
#include "OpaqueASTLowerer.h"
#include "OpaqueProgramAnalyzer.h"
#include "OpaqueProgramPlan.h"
#include "OpaqueTypeLayout.h"


namespace Xsc
{


void OpaqueTypeLowering::Run(Program& program, const NameMangling&)
{
    OpaqueLowering::OpaqueTypeLayoutCache layouts;
    OpaqueLowering::OpaqueProgramPlan plan;

    OpaqueLowering::OpaqueProgramAnalyzer analyzer { program, layouts, plan };
    analyzer.BuildAndAnalyze();

    OpaqueLowering::OpaqueASTLowerer lowerer { program, layouts, plan };
    lowerer.Lower();
}


} // /namespace Xsc



// ================================================================================