/*
 * ReferenceAnalyzer.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_REFERENCE_ANALYZER_H
#define XSC_REFERENCE_ANALYZER_H


#include "VisitorTracker.h"
#include "Token.h"
#include "SymbolTable.h"
#include "ReportHandler.h"
#include <Xsc/Targets.h>


namespace Xsc
{


/*
Object reference analyzer.
This helper class for the context analyzer marks all functions
which are used (or rather referenced) from the beginning of the shader entry point.
All other functions will be ignored by the code generator.

For ray tracing programs it also checks, on everything the entry point reaches, which intrinsics each stage allows and
what inline ray tracing callbacks can't do. It works on the AST the HLSL front end builds, independently of the output backend.
*/
class ReferenceAnalyzer : private VisitorTracker
{

    public:

        ReferenceAnalyzer(Log* log = nullptr);

        // Marks all declarational AST nodes (i.e. function decl, structure decl etc.) that are reachable from the specififed entry point.
        void MarkReferencesFromEntryPoint(Program& program, const ShaderTarget shaderTarget);

        /*
        Marks references like MarkReferencesFromEntryPoint, and checks the ray tracing rules on everything the entry point reaches,
        including the callback methods of RayTraceClosest and RayTraceAny calls. Returns false if an error was reported.
        */
        bool ValidateRayTracing(Program& program, const ShaderTarget shaderTarget);

    private:

        /*
        Marks the specified AST node as reachable and returns false if the AST node has already been marked as reachable.
        Inside a ray tracing callback, nodes are visited once more (with their own flag), so the callback rules see them too.
        */
        bool Reachable(AST* ast);

        // Reports a ray tracing rule violation, with the call chain from the entry point.
        void ErrorRayTracing(const std::string& msg, const AST* ast);

        // Adds the call chain from the entry point as hints for the next report.
        void HintCallStack();

        // Checks the specified intrinsic call against the ray tracing stage and callback rules.
        void CheckRayTracingIntrinsic(const Intrinsic intrinsic, const std::string& ident, const AST* ast);

        // Visits the callback methods of an inline ray tracing call, as if they were called from it.
        void VisitInlineRayTraceCallbacks(CallExpr* callExpr);

        // Marks declarations referenced by a type and its nested element types.
        void MarkTypeDenoterReferences(const TypeDenoter* typeDenoter);

        void VisitStmntList(const std::vector<StmntPtr>& stmnts);

        void MarkLValueExpr(const Expr* expr);
        void MarkLValueExprObject(const ObjectExpr* objectExpr);

        /* ----- Visitor implementation ----- */

        DECL_VISIT_PROC( CodeBlock         );
        DECL_VISIT_PROC( SwitchCase        );
        DECL_VISIT_PROC( TypeSpecifier     );

        DECL_VISIT_PROC( VarDecl           );
        DECL_VISIT_PROC( StructDecl        );
        DECL_VISIT_PROC( BufferDecl        );
        DECL_VISIT_PROC( SamplerDecl       );

        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( UniformBufferDecl );
        DECL_VISIT_PROC( BufferDeclStmnt   );
        DECL_VISIT_PROC( SamplerDeclStmnt  );
        DECL_VISIT_PROC( VarDeclStmnt      );

        DECL_VISIT_PROC( CtrlTransferStmnt );

        DECL_VISIT_PROC( UnaryExpr         );
        DECL_VISIT_PROC( PostUnaryExpr     );
        DECL_VISIT_PROC( CallExpr          );
        DECL_VISIT_PROC( ObjectExpr        );
        DECL_VISIT_PROC( AssignExpr        );

        /* === Members === */

        ReportHandler               reportHandler_;

        Program*                    program_            = nullptr;
        ShaderTarget                shaderTarget_       = ShaderTarget::VertexShader;
        bool                        validateRayTracing_ = false;

        std::vector<CallExpr*>      callExprStack_;
        std::vector<FunctionDecl*>  callbackStack_;     // Inline ray tracing callback methods being visited, innermost last.

};


} // /namespace Xsc


#endif



// ================================================================================
