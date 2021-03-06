#include <lfortran/ast_to_src.h>

using LFortran::AST::expr_t;
using LFortran::AST::Name_t;
using LFortran::AST::Num_t;
using LFortran::AST::BinOp_t;
using LFortran::AST::operatorType;
using LFortran::AST::BaseVisitor;


namespace LFortran {

static inline AST::Num_t* EXPR_NUM(const AST::ast_t *f)
{
    LFORTRAN_ASSERT(f->type == AST::astType::expr);
    AST::expr_t *t = (AST::expr_t *)f;
    LFORTRAN_ASSERT(t->type == AST::exprType::Num);
    return (AST::Num_t*)t;
}

namespace {

    std::string op2str(const operatorType type)
    {
        switch (type) {
            case (operatorType::Add) : return " + ";
            case (operatorType::Sub) : return " - ";
            case (operatorType::Mul) : return "*";
            case (operatorType::Div) : return "/";
            case (operatorType::Pow) : return "**";
        }
        throw LFortranException("Unknown type");
    }

    std::string boolop2str(const AST::boolopType type)
    {
        switch (type) {
            case (AST::boolopType::And) : return ".and.";
            case (AST::boolopType::Or) : return ".or.";
            case (AST::boolopType::Eqv) : return ".eqv.";
            case (AST::boolopType::NEqv) : return ".neqv.";
        }
        throw LFortranException("Unknown type");
    }

    std::string cmpop2str(const AST::cmpopType type)
    {
        switch (type) {
            case (AST::cmpopType::Eq) : return "==";
            case (AST::cmpopType::Gt) : return ">";
            case (AST::cmpopType::GtE) : return ">=";
            case (AST::cmpopType::Lt) : return "<";
            case (AST::cmpopType::LtE) : return "<=";
            case (AST::cmpopType::NotEq) : return "/=";
        }
        throw LFortranException("Unknown type");
    }

}

namespace AST {

class ASTToSRCVisitor : public BaseVisitor<ASTToSRCVisitor>
{
public:
    std::string s;
    bool use_colors;
    int indent_level;
    std::string indent;
    int indent_spaces;
    bool indent_unit;

    // Syntax highlighting groups
    enum gr {
        // Type
        UnitHeader,  // program, end program
        Type,        // integer, real, character

        // Constant
        String,      // "some string"
        Integer,     // 234
        Real,        // 5.5_dp
        Logical,     // .true., .false.

        // Functions
        Call,        // call
        Intrinsic,   // exp, sin, cos

        // Statements
        Conditional, // if, then, else, end if, where, select case
        Repeat,      // do, while, forall
        Keyword,     // any other keyword: print

        // Reset
        Reset,       // resets any previously set formatting
    };
    std::string syn_reset;

public:
    ASTToSRCVisitor(bool color, int indent, bool indent_unit)
            : use_colors{color}, indent_level{0},
            indent_spaces{indent}, indent_unit{indent_unit}
        { }

    std::string syn(const gr &g=gr::Reset) {
        std::string syn_color;
        if (use_colors) {
            switch (g) {
                case (gr::UnitHeader) : {
                    syn_color = color(fgB::cyan);
                    syn_reset = color(fg::reset);
                    break;
                }

                case (gr::Type) : {
                    syn_color = color(fgB::green);
                    syn_reset = color(fg::reset);
                    break;
                }

                case (gr::String) :
                case (gr::Integer) :
                case (gr::Real) :
                case (gr::Logical) : {
                    syn_color = color(fg::magenta);
                    syn_reset = color(fg::reset);
                    break;
                }

                case (gr::Call) :
                case (gr::Intrinsic) : {
                    syn_color = color(fgB::cyan) + color(style::bold);
                    syn_reset = color(fg::reset) + color(style::reset);
                    break;
                }

                case (gr::Conditional) :
                case (gr::Repeat) :
                case (gr::Keyword) : {
                    syn_color = color(fg::yellow);
                    syn_reset = color(fg::reset);
                    break;
                }

                case (gr::Reset) : {
                    syn_color = syn_reset;
                    syn_reset = "";
                    break;
                } ;

                default : {
                    throw LFortranException("Syntax Group not implemented");
                }
            }
        }
        return syn_color;
    }

    void inc_indent() {
        indent_level++;
        indent = std::string(indent_level*indent_spaces, ' ');
    }

    void dec_indent() {
        indent_level--;
        indent = std::string(indent_level*indent_spaces, ' ');
    }

    void visit_TranslationUnit(const TranslationUnit_t &x) {
        std::string r;
        for (size_t i=0; i<x.n_items; i++) {
            this->visit_ast(*x.m_items[i]);
            r += s;
            if (i < x.n_items-1) r.append("\n");
        }
        s = r;
    }

    void visit_Module(const Module_t &x) {
        std::string r = "";
        r += syn(gr::UnitHeader);
        r.append("module");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");
        if (indent_unit) inc_indent();
        for (size_t i=0; i<x.n_use; i++) {
            this->visit_unit_decl1(*x.m_use[i]);
            r.append(s);
        }
        r.append("\n");
        for (size_t i=0; i<x.n_decl; i++) {
            this->visit_unit_decl2(*x.m_decl[i]);
            r.append(s);
        }
        r.append("\n");
        if (x.n_contains > 0) {
            r += "\n\n";
            r += syn(gr::UnitHeader);
            r.append("contains");
            r += syn();
            r += "\n\n";
            for (size_t i=0; i<x.n_contains; i++) {
                this->visit_program_unit(*x.m_contains[i]);
                r.append(s);
                r.append("\n");
            }
        }
        if (indent_unit) dec_indent();
        r += syn(gr::UnitHeader);
        r.append("end module");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");
        s = r;
    }

    void visit_Program(const Program_t &x) {
        std::string r;
        r += syn(gr::UnitHeader);
        r.append("program");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");

        r += format_unit_body(x);
        r += syn(gr::UnitHeader);
        r.append("end program");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");

        s = r;
    }

    void visit_Subroutine(const Subroutine_t &x) {
        std::string r = indent;
        r += syn(gr::UnitHeader);
        r.append("subroutine");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_arg(x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) r.append(", ");
        }
        r.append(")");
        r.append("\n");

        r += format_unit_body(x);
        r += indent;
        r += syn(gr::UnitHeader);
        r.append("end subroutine");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");
        s = r;
    }

    template <typename T>
    std::string format_unit_body(const T &x) {
        std::string r;
        if (indent_unit) inc_indent();
        for (size_t i=0; i<x.n_use; i++) {
            this->visit_unit_decl1(*x.m_use[i]);
            r.append(s);
        }
        for (size_t i=0; i<x.n_decl; i++) {
            this->visit_unit_decl2(*x.m_decl[i]);
            r.append(s);
        }
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r.append(s);
        }
        if (x.n_contains > 0) {
            r += "\n";
            r += syn(gr::UnitHeader);
            r.append("contains");
            r += syn();
            r += "\n\n";
            if (!indent_unit) inc_indent();
            for (size_t i=0; i<x.n_contains; i++) {
                this->visit_program_unit(*x.m_contains[i]);
                r.append(s);
                r.append("\n");
            }
            if (!indent_unit) dec_indent();
        }
        if (indent_unit) dec_indent();
        return r;
    }

    void visit_Function(const Function_t &x) {
        std::string r = indent;
        if (x.m_return_type) {
            r += syn(gr::Type);
            r.append(x.m_return_type);
            r += syn();
            r.append(" ");
        }
        r += syn(gr::UnitHeader);
        r.append("function");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_arg(x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) r.append(", ");
        }
        r.append(")");
        if (x.m_return_var) {
            r += " ";
            r += syn(gr::UnitHeader);
            r.append("result");
            r += syn();
            r += "(";
            this->visit_expr(*x.m_return_var);
            r.append(s);
            r.append(")");
        }
        r.append(" ");
        if (x.m_bind) {
            this->visit_tbind(*x.m_bind);
            r.append(s);
        }
        r.append("\n");

        r += format_unit_body(x);
        r += indent;
        r += syn(gr::UnitHeader);
        r.append("end function");
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("\n");

        s = r;
    }

    void visit_Use(const Use_t &x) {
        std::string r = indent;
        r += syn(gr::UnitHeader);
        r += "use";
        r += syn();
        r += " ";
        r.append(x.m_module);
        if (x.n_symbols > 0) {
            r.append(", ");
            r += syn(gr::UnitHeader);
            r += "only";
            r += syn();
            r += ": ";
            for (size_t i=0; i<x.n_symbols; i++) {
                this->visit_use_symbol(*x.m_symbols[i]);
                r.append(s);
                if (i < x.n_symbols-1) r.append(", ");
            }
        }
        r += "\n";
        s = r;
    }

    void visit_Declaration(const Declaration_t &x) {
        std::string r = "";
        for (size_t i=0; i<x.n_vars; i++) {
            this->visit_decl(x.m_vars[i]);
            r.append(s);
        }
        s = r;
    }

    void visit_Assignment(const Assignment_t &x) {
        std::string r = indent;
        this->visit_expr(*x.m_target);
        r.append(s);
        r.append(" = ");
        this->visit_expr(*x.m_value);
        r.append(s);
        r += "\n";
        s = r;
    }

    void visit_Associate(const Associate_t &x) {
        std::string r = indent;
        this->visit_expr(*x.m_target);
        r.append(s);
        r.append(" => ");
        this->visit_expr(*x.m_value);
        r.append(s);
        r += "\n";
        s = r;
    }

    void visit_SubroutineCall(const SubroutineCall_t &x) {
        std::string r = indent;
        r += syn(gr::Call);
        r += "call";
        r += syn();
        r += " ";
        r.append(x.m_name);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_expr(*x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) r.append(", ");
        }
        r.append(")\n");
        s = r;
    }

    void visit_BuiltinCall(const BuiltinCall_t &x) {
        std::string r = indent;
        r += x.m_name;
        r += "(";
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_expr(*x.m_args[i]);
            r += s;
            if (i < x.n_args-1) s.append(", ");
        }
        r += ")\n";
        s = r;
    }

    void visit_If(const If_t &x) {
        std::string r = indent;
        r += syn(gr::Conditional);
        r += "if";
        r += syn();
        r += " (";
        this->visit_expr(*x.m_test);
        r += s;
        r += ") ";
        r += syn(gr::Conditional);
        r += "then";
        r += syn();
        r += "\n";
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r += s;
        }
        dec_indent();
        if (x.n_orelse > 0) {
            r += indent;
            r += syn(gr::Conditional);
            r += "else";
            r += syn();
            r += "\n";
            inc_indent();
            for (size_t i=0; i<x.n_orelse; i++) {
                this->visit_stmt(*x.m_orelse[i]);
                r += s;
            }
            dec_indent();
        }
        r += indent;
        r += syn(gr::Conditional);
        r += "end if";
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Where(const Where_t &x) {
        std::string r = indent;
        r += syn(gr::Repeat);
        r += "where";
        r += syn();
        r += " (";
        this->visit_expr(*x.m_test);
        r += s;
        r += ")\n";
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r += s;
        }
        dec_indent();
        if (x.n_orelse > 0) {
            r += indent;
            r += syn(gr::Conditional);
            r += "else where";
            r += syn();
            r += "\n";
            inc_indent();
            for (size_t i=0; i<x.n_orelse; i++) {
                this->visit_stmt(*x.m_orelse[i]);
                r += s;
            }
            dec_indent();
        }
        r += indent;
        r += syn(gr::Repeat);
        r += "end where";
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Stop(const Stop_t &x) {
        std::string r = indent;
        r += syn(gr::Keyword);
        r.append("stop");
        r += syn();
        if (x.m_code) {
            this->visit_expr(*x.m_code);
            r += " " + s;
        }
        r += "\n";
        s = r;
    }

    void visit_ErrorStop(const ErrorStop_t &/*x*/) {
        std::string r = indent;
        r += syn(gr::Keyword);
        r.append("error stop");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_DoLoop(const DoLoop_t &x) {
        std::string r = indent;
        r += syn(gr::Repeat);
        r += "do";
        r += syn();
        if (x.m_var) {
            r.append(" ");
            r.append(x.m_var);
            r.append(" = ");
        }
        if (x.m_start) {
            this->visit_expr(*x.m_start);
            r.append(s);
            r.append(", ");
        }
        if (x.m_end) {
            this->visit_expr(*x.m_end);
            r.append(s);
        }
        if (x.m_increment) {
            r.append(", ");
            this->visit_expr(*x.m_increment);
            r.append(s);
        }
        r.append("\n");
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r.append(s);
        }
        dec_indent();
        r += indent;
        r += syn(gr::Repeat);
        r.append("end do");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_DoConcurrentLoop(const DoConcurrentLoop_t &x) {
        if (x.n_control != 1) {
            throw SemanticError("Do concurrent: exactly one control statement is required for now",
            x.base.base.loc);
        }
        AST::ConcurrentControl_t &h = *(AST::ConcurrentControl_t*) x.m_control[0];
        std::string r = indent;
        r += syn(gr::Repeat);
        r += "do concurrent";
        r += syn();
        if (h.m_var) {
            r.append(" (");
            r.append(h.m_var);
            r.append(" = ");
        }
        if (h.m_start) {
            this->visit_expr(*h.m_start);
            r.append(s);
            r.append(":");
        }
        if (h.m_end) {
            this->visit_expr(*h.m_end);
            r.append(s);
        }
        if (h.m_increment) {
            r.append(":");
            this->visit_expr(*h.m_increment);
            r.append(s);
        }
        r.append(")\n");
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r.append(s);
        }
        dec_indent();
        r += indent;
        r += syn(gr::Repeat);
        r.append("end do");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Cycle(const Cycle_t &/*x*/) {
        std::string r = indent;
        r += syn(gr::Keyword);
        r.append("cycle");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Exit(const Exit_t &/*x*/) {
        std::string r = indent;
        r += syn(gr::Keyword);
        r.append("exit");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Return(const Return_t &/*x*/) {
        std::string r = indent;
        r += syn(gr::Keyword);
        r.append("return");
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_WhileLoop(const WhileLoop_t &x) {
        std::string r = indent;
        r += syn(gr::Repeat);
        r += "do while";
        r += syn();
        r += " (";
        this->visit_expr(*x.m_test);
        r += s;
        r += ")\n";
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r += s;
        }
        dec_indent();
        r += indent;
        r += syn(gr::Repeat);
        r += "end do";
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_Print(const Print_t &x) {
        std::string r=indent;
        r += syn(gr::Keyword);
        r += "print";
        r += syn();
        r += " ";
        if (x.m_fmt) {
            r += x.m_fmt;
        } else {
            r += "*";
        }
        if (x.n_values > 0) {
            r += ", ";
            for (size_t i=0; i<x.n_values; i++) {
                this->visit_expr(*x.m_values[i]);
                r += s;
                if (i < x.n_values-1) r += ", ";
            }
        }
        r += "\n";
        s = r;
    }

    void visit_BoolOp(const BoolOp_t &x) {
        this->visit_expr(*x.m_left);
        std::string left = std::move(s);
        this->visit_expr(*x.m_right);
        std::string right = std::move(s);
        s = "(" + left + ")" + boolop2str(x.m_op) + "(" + right + ")";
    }

    void visit_BinOp(const BinOp_t &x) {
        this->visit_expr(*x.m_left);
        std::string left = std::move(s);
        this->visit_expr(*x.m_right);
        std::string right = std::move(s);
        s = "(" + left + ")" + op2str(x.m_op) + "(" + right + ")";
    }

    void visit_UnaryOp(const UnaryOp_t &x) {
        this->visit_expr(*x.m_operand);
        if (x.m_op == AST::unaryopType::USub) {
            s = "-(" + s + ")";
        } else if (x.m_op == AST::unaryopType::UAdd) {
            // pass
            // s = s;
        } else if (x.m_op == AST::unaryopType::Not) {
            s = ".not.(" + s + ")";
        } else {
            throw LFortranException("Unary op type not implemented");
        }
    }

    void visit_Compare(const Compare_t &x) {
        this->visit_expr(*x.m_left);
        std::string left = std::move(s);
        this->visit_expr(*x.m_right);
        std::string right = std::move(s);
        s = "(" + left + ")" + cmpop2str(x.m_op) + "(" + right + ")";
    }

    void visit_FuncCall(const FuncCall_t &x) {
        std::string r;
        r.append(x.m_func);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_expr(*x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) s.append(", ");
        }
        for (size_t i=0; i<x.n_keywords; i++) {
            this->visit_keyword(x.m_keywords[i]);
            r.append(s);
            if (i < x.n_keywords-1) r.append(", ");
        }
        r.append(")");
        s = r;
    }

    void visit_FuncCallOrArray(const FuncCallOrArray_t &x) {
        std::string r;
        r.append(x.m_func);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_expr(*x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) s.append(", ");
        }
        for (size_t i=0; i<x.n_keywords; i++) {
            this->visit_keyword(x.m_keywords[i]);
            r.append(s);
            if (i < x.n_keywords-1) r.append(", ");
        }
        r.append(")");
        s = r;
    }

    void visit_Array(const Array_t &x) {
        std::string r;
        r.append(x.m_name);
        r.append("(");
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_array_index(*x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) s.append(", ");
        }
        r.append(")");
        s = r;
    }

    void visit_ArrayInitializer(const ArrayInitializer_t &x) {
        std::string r = "[";
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_expr(*x.m_args[i]);
            r.append(s);
            if (i < x.n_args-1) r.append(", ");
        }
        r.append("]");
        s = r;
    }

    void visit_Num(const Num_t &x) {
        s = syn(gr::Integer);
        s += std::to_string(x.m_n);
        s += syn();
    }

    void visit_Real(const Real_t &x) {
        s = syn(gr::Real);
        s += x.m_n;
        s += syn();
    }

    void visit_Str(const Str_t &x) {
        s = syn(gr::String);
        s += "\"";
        s += x.m_s;
        s += "\"";
        s += syn();
    }

    void visit_Name(const Name_t &x) {
        s = std::string(x.m_id);
    }

    void visit_Constant(const Constant_t &x) {
        s = syn(gr::Logical);
        if (x.m_value) {
            s += ".true.";
        } else {
            s += ".false.";
        }
        s += syn();
    }

    void visit_decl(const decl_t &x) {
        std::string r = indent;
        r += syn(gr::Type);
        r += std::string(x.m_sym_type);
        r += syn();
        if (x.n_attrs > 0) {
            for (size_t i=0; i<x.n_attrs; i++) {
                r.append(", ");
                this->visit_attribute(*x.m_attrs[i]);
                r.append(s);
            }
        }
        r.append(" :: ");
        r.append(x.m_sym);
        if (x.n_dims > 0) {
            r.append("(");
            for (size_t i=0; i<x.n_dims; i++) {
                this->visit_dimension(x.m_dims[i]);
                r.append(s);
                if (i < x.n_dims-1) r.append(",");
            }
            r.append(")");
        }
        if (x.m_initializer) {
            r.append("=");
            this->visit_expr(*x.m_initializer);
            r.append(s);
        }
        r += "\n";
        s = r;
    }

    void visit_dimension(const dimension_t &x) {
        std::string left;
        bool left_is_one=false;
        if (x.m_start) {
            this->visit_expr(*x.m_start);
            left = s;
            if (x.m_start->type == AST::exprType::Num) {
                left_is_one = (EXPR_NUM((AST::ast_t*)x.m_start)->m_n == 1);
            };
        }
        std::string right;
        if (x.m_end) {
            this->visit_expr(*x.m_end);
            right = s;
        }
        if (left_is_one && right != "") {
            s = right;
        } else {
            s = left + ":" + right;
        }
    }

    void visit_Attribute(const Attribute_t &x) {
        std::string r;
        r += syn(gr::Type);
        r.append(x.m_name);
        r += syn();
        if (x.n_args > 0) {
            r.append("(");
            for (size_t i=0; i<x.n_args; i++) {
                this->visit_attribute_arg(x.m_args[i]);
                r.append(s);
                if (i < x.n_args-1) r.append(", ");
            }
            r.append(")");
        }
        s = r;
    }

    void visit_attribute_arg(const attribute_arg_t &x) {
        s = syn(gr::Type);
        s += x.m_arg;
        s += syn();

    }

    void visit_arg(const arg_t &x) {
        s = std::string(x.m_arg);
    }

    void visit_keyword(const keyword_t &/*x*/) {
        throw LFortranException("keyword not implemented");
    }

    void visit_Bind(const Bind_t &x) {
        std::string r;
        r += syn(gr::UnitHeader);
        r.append("bind");
        r += syn();
        r += "(";
        for (size_t i=0; i<x.n_args; i++) {
            this->visit_keyword(x.m_args[i]);
            r += s;
            if (i < x.n_args-1) r.append(", ");
        }
        r += ")";
        s = r;
    }

    void visit_ArrayIndex(const ArrayIndex_t &x) {
        std::string r;
        if (x.m_left) {
            this->visit_expr(*x.m_left);
            r += s;
        }
        r.append(":");
        if (x.m_right) {
            this->visit_expr(*x.m_right);
            r += s;
        }
        if (x.m_step) {
            r.append(":");
            this->visit_expr(*x.m_step);
            r += s;
        };
        s = r;
    }

    void visit_UseSymbol(const UseSymbol_t &x) {
        s = "";
        if (x.m_rename) {
            s.append(x.m_rename);
            s.append(" => ");
        }
        s.append(x.m_sym);
    }

    void visit_Select(const AST::Select_t &x) {
        std::string r = indent;
        r += syn(gr::Conditional);
        r += "select case";
        r += syn();
        r += " (";
        this->visit_expr(*x.m_test);
        r += s;
        r += ")\n";
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_case_stmt(*x.m_body[i]);
            r += s;
        }
        dec_indent();
        r += indent;
        r += syn(gr::Conditional);
        r += "end select";
        r += syn();
        r += "\n";
        s = r;
    }

    void visit_CaseStmt(const AST::CaseStmt_t &x) {
        std::string r = indent;
        r += syn(gr::Conditional);
        r += "case";
        r += syn();
        r += " (";
        for (size_t i=0; i<x.n_test; i++) {
            this->visit_expr(*x.m_test[i]);
            r += s;
            if (i < x.n_test-1) r += ", ";
        }
        r += ")\n";
        inc_indent();
        for (size_t i=0; i<x.n_body; i++) {
            this->visit_stmt(*x.m_body[i]);
            r += s;
        }
        dec_indent();
        s = r;
    }
};

}

std::string ast_to_src(AST::TranslationUnit_t &ast, bool color, int indent,
        bool indent_unit) {
    AST::ASTToSRCVisitor v(color, indent, indent_unit);
    v.visit_ast((AST::ast_t &)ast);
    return v.s;
}

}
