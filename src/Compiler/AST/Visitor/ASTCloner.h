/*
 * ASTCloner.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_AST_CLONER_H
#define XSC_AST_CLONER_H


#include "Visitor.h"
#include "TypeDenoter.h"
#include <stdexcept>
#include <type_traits>


namespace Xsc
{


/*
Deep-clones an AST subtree.

All owning AST edges are cloned while semantic references and cached type
denoters are reset so the result can be analyzed independently. Type-denoter
cloning is customizable for transformations such as template substitution.
*/
class ASTCloner : private Visitor
{

    public:

        virtual ~ASTCloner() = default;

        template <typename T>
        std::shared_ptr<T> Clone(const std::shared_ptr<T>& ast)
        {
            static_assert(std::is_base_of<AST, T>::value, "ASTCloner can only clone AST nodes");

            if (!ast)
                return nullptr;

            clonedAST_.reset();
            ast->Visit(this);

            auto clone = std::dynamic_pointer_cast<T>(clonedAST_);
            if (!clone)
                throw std::logic_error("AST cloner visitor did not produce the expected node type");
            return clone;
        }

        template <typename T>
        std::vector<std::shared_ptr<T>> Clone(const std::vector<std::shared_ptr<T>>& astList)
        {
            std::vector<std::shared_ptr<T>> clones;
            clones.reserve(astList.size());
            for (const auto& ast : astList)
                clones.push_back(Clone(ast));
            return clones;
        }

    protected:

        // Override this to transform types while cloning an AST subtree.
        virtual TypeDenoterPtr CloneTypeDenoter(const TypeDenoterPtr& typeDenoter);

    private:

        template <typename T>
        std::shared_ptr<T> CloneLeaf(const T* ast)
        {
            return std::make_shared<T>(*ast);
        }

        template <typename T>
        std::shared_ptr<T> CloneTypedLeaf(const T* ast)
        {
            auto clone = CloneLeaf(ast);
            clone->ResetTypeDenoter();
            return clone;
        }

        template <typename T>
        std::shared_ptr<T> CloneTypeDenoterAs(const std::shared_ptr<T>& typeDenoter)
        {
            if (!typeDenoter)
                return nullptr;

            auto clone = std::dynamic_pointer_cast<T>(CloneTypeDenoter(typeDenoter));
            if (!clone)
                throw std::logic_error("AST cloner type transformation changed an owning type-denoter kind");
            return clone;
        }

        void CloneStatementBase(const Stmnt* source, Stmnt* clone);

        /* ----- Visitor implementation ----- */

        DECL_VISIT_PROC( Program           );
        DECL_VISIT_PROC( CodeBlock         );
        DECL_VISIT_PROC( Attribute         );
        DECL_VISIT_PROC( SwitchCase        );
        DECL_VISIT_PROC( SamplerValue      );
        DECL_VISIT_PROC( Register          );
        DECL_VISIT_PROC( PackOffset        );
        DECL_VISIT_PROC( ArrayDimension    );
        DECL_VISIT_PROC( TypeSpecifier     );

        DECL_VISIT_PROC( VarDecl           );
        DECL_VISIT_PROC( BufferDecl        );
        DECL_VISIT_PROC( SamplerDecl       );
        DECL_VISIT_PROC( StructDecl        );
        DECL_VISIT_PROC( AliasDecl         );
        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( UniformBufferDecl );

        DECL_VISIT_PROC( BufferDeclStmnt   );
        DECL_VISIT_PROC( SamplerDeclStmnt  );
        DECL_VISIT_PROC( VarDeclStmnt      );
        DECL_VISIT_PROC( AliasDeclStmnt    );
        DECL_VISIT_PROC( BasicDeclStmnt    );

        DECL_VISIT_PROC( NullStmnt         );
        DECL_VISIT_PROC( CodeBlockStmnt    );
        DECL_VISIT_PROC( ForLoopStmnt      );
        DECL_VISIT_PROC( WhileLoopStmnt    );
        DECL_VISIT_PROC( DoWhileLoopStmnt  );
        DECL_VISIT_PROC( IfStmnt           );
        DECL_VISIT_PROC( ElseStmnt         );
        DECL_VISIT_PROC( SwitchStmnt       );
        DECL_VISIT_PROC( ExprStmnt         );
        DECL_VISIT_PROC( ReturnStmnt       );
        DECL_VISIT_PROC( CtrlTransferStmnt );

        DECL_VISIT_PROC( NullExpr          );
        DECL_VISIT_PROC( SequenceExpr      );
        DECL_VISIT_PROC( LiteralExpr       );
        DECL_VISIT_PROC( TypeSpecifierExpr );
        DECL_VISIT_PROC( TernaryExpr       );
        DECL_VISIT_PROC( BinaryExpr        );
        DECL_VISIT_PROC( UnaryExpr         );
        DECL_VISIT_PROC( PostUnaryExpr     );
        DECL_VISIT_PROC( CallExpr          );
        DECL_VISIT_PROC( BracketExpr       );
        DECL_VISIT_PROC( AssignExpr        );
        DECL_VISIT_PROC( ObjectExpr        );
        DECL_VISIT_PROC( ArrayExpr         );
        DECL_VISIT_PROC( CastExpr          );
        DECL_VISIT_PROC( InitializerExpr   );

        ASTPtr clonedAST_;

};


} // /namespace Xsc


#endif

