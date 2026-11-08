/*
 * InlineRayTracingFieldAnalyzer.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_INLINE_RAY_TRACING_FIELD_ANALYZER_H
#define XSC_INLINE_RAY_TRACING_FIELD_ANALYZER_H


#include "Visitor.h"
#include "InlineRayTracingHLSL.h"
#include <set>
#include <vector>


namespace Xsc
{


/*
Finds the fields of the built-in structures RayCandidate and RayHit that a program reads.

A field is read when an expression of the structure's type is accessed with that member. A value of the structure's
type may also be passed to a function, copied into a local variable or parameter, returned from a function that isn't
an entry point, or put in parentheses or a ternary expression. Any other use (storing it in a global, a structure
member or a resource, casting it, passing it to an intrinsic, returning it from an entry point, ...) counts as reading
every field.
*/
class InlineRayTracingFieldAnalyzer : private Visitor
{

    public:

        InlineRayTracingFieldAnalyzer(Program& program);

        // Returns the RayHit fields that the reachable functions of the program read.
        InlineRayTracingHLSL::FieldUsage AnalyzeHitFields();

        // Returns the RayCandidate fields that the specified function, and every function it calls, read.
        InlineRayTracingHLSL::FieldUsage AnalyzeCandidateFields(FunctionDecl* funcDecl);

    private:

        DECL_VISIT_PROC( FunctionDecl    );
        DECL_VISIT_PROC( VarDecl         );

        DECL_VISIT_PROC( ExprStmnt       );
        DECL_VISIT_PROC( ReturnStmnt     );

        DECL_VISIT_PROC( SequenceExpr    );
        DECL_VISIT_PROC( TernaryExpr     );
        DECL_VISIT_PROC( CallExpr        );
        DECL_VISIT_PROC( BracketExpr     );
        DECL_VISIT_PROC( AssignExpr      );
        DECL_VISIT_PROC( ObjectExpr      );
        DECL_VISIT_PROC( ArrayExpr       );
        DECL_VISIT_PROC( CastExpr        );
        DECL_VISIT_PROC( InitializerExpr );

        // Resets the analysis state for the built-in structure with the specified identifier.
        void Reset(const std::string& structIdent);

        // Returns true if the type of the specified expression is the analyzed structure (or an array of it, if 'allowArray' is true).
        bool IsAnalyzedType(Expr* expr, bool allowArray) const;

        // Returns true if the specified expression names a local variable or a parameter of the current function, or an element of one.
        bool IsLocalStorage(Expr* expr) const;

        // Marks the whole value of the specified expression as used in a way that doesn't read every field.
        void AllowWholeValue(const Expr* expr);

        // Counts every field as read if the specified expression has the analyzed type and isn't allowed as a whole value.
        void Check(Expr* expr);

    private:

        Program&                            program_;
        StructDecl*                         structDecl_         = nullptr;  // Analyzed built-in structure; null if the program doesn't declare it.
        InlineRayTracingHLSL::FieldUsage    usage_;
        std::set<const Expr*>               wholeValueExprs_;               // Expressions whose whole value may be used without reading every field.
        bool                                followCalls_        = false;    // Analyze the functions that analyzed functions call, instead of every reachable function.
        std::vector<FunctionDecl*>          pendingFuncDecls_;
        std::set<FunctionDecl*>             visitedFuncDecls_;
        FunctionDecl*                       currentFuncDecl_    = nullptr;
        bool                                isEntryPoint_       = false;    // The current function is an entry point.
        std::set<const VarDecl*>            localVarDecls_;                 // Local variables and parameters of the current function.

};


} // /namespace Xsc


#endif



// ================================================================================
