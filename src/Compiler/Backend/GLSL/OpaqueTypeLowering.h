/*
 * OpaqueTypeLowering.h
 *
 * Typed analysis and AST lowering for values that contain GLSL/SPIR-V opaque
 * resources. The public surface is deliberately a single internal facade; all
 * layouts, contracts, data-flow state, and rewrite machinery live in the
 * implementation file.
 */

#ifndef XSC_OPAQUE_TYPE_LOWERING_H
#define XSC_OPAQUE_TYPE_LOWERING_H


#include <Xsc/Xsc.h>


namespace Xsc
{


struct Program;


class OpaqueTypeLowering
{

    public:

        void Run(Program& program, const NameMangling& nameMangling);

};


} // /namespace Xsc


#endif



// ================================================================================
