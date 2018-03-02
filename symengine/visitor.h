/**
 *  \file visitor.h
 *  Class Visitor
 *
 **/
#ifndef SYMENGINE_VISITOR_H
#define SYMENGINE_VISITOR_H

#include <symengine/polys/uintpoly_flint.h>
#include <symengine/polys/uintpoly_piranha.h>
#include <symengine/polys/uexprpoly.h>
#include <symengine/polys/msymenginepoly.h>
#include <symengine/polys/uratpoly.h>
#include <symengine/complex_mpc.h>
#include <symengine/series_generic.h>
#include <symengine/series_piranha.h>
#include <symengine/series_flint.h>
#include <symengine/series_generic.h>
#include <symengine/series_piranha.h>
#include <symengine/sets.h>
#include <symengine/fields.h>
#include <symengine/logic.h>
#include <symengine/infinity.h>
#include <symengine/nan.h>
#include <symengine/matrix.h>
#include <symengine/symengine_casts.h>

namespace SymEngine
{

class Visitor
{
public:
#define SYMENGINE_ENUM(TypeID, Class) virtual void visit(const Class &) = 0;
#define SYMENGINE_ENUM_TEMPLATE(TypeID, Class) SYMENGINE_ENUM(TypeID, Class)
#include "symengine/type_codes.inc"
#undef SYMENGINE_ENUM_TEMPLATE
#undef SYMENGINE_ENUM
};

void preorder_traversal(const Basic &b, Visitor &v);
void postorder_traversal(const Basic &b, Visitor &v);

template <class Derived, class Base = Visitor>
class BaseVisitor : public Base
{

public:
#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ < 8
    // Following two ctors can be replaced by `using Base::Base` if inheriting
    // constructors are allowed by the compiler. GCC 4.8 is the earliest
    // version supporting this.
    template <typename... Args,
              typename
              = enable_if_t<std::is_constructible<Base, Args...>::value>>
    BaseVisitor(Args &&... args) : Base(std::forward<Args>(args)...)
    {
    }

    BaseVisitor() : Base()
    {
    }
#else
    using Base::Base;
#endif

#define SYMENGINE_ENUM(TypeID, Class)                                          \
    virtual void visit(const Class &x)                                         \
    {                                                                          \
        down_cast<Derived *>(this)->bvisit(x);                                 \
    };
#define SYMENGINE_ENUM_TEMPLATE(TypeID, Class) SYMENGINE_ENUM(TypeID, Class)
#include "symengine/type_codes.inc"
#undef SYMENGINE_ENUM_TEMPLATE
#undef SYMENGINE_ENUM
};

class StopVisitor : public Visitor
{
public:
    bool stop_;
};

class LocalStopVisitor : public StopVisitor
{
public:
    bool local_stop_;
};

void preorder_traversal_stop(const Basic &b, StopVisitor &v);
void postorder_traversal_stop(const Basic &b, StopVisitor &v);
void preorder_traversal_local_stop(const Basic &b, LocalStopVisitor &v);

class HasSymbolVisitor : public BaseVisitor<HasSymbolVisitor, StopVisitor>
{
protected:
    Ptr<const Symbol> x_;
    bool has_;

public:
    HasSymbolVisitor(Ptr<const Symbol> x) : x_(x)
    {
    }

    void bvisit(const Symbol &x)
    {
        if (x_->__eq__(x)) {
            has_ = true;
            stop_ = true;
        }
    }

    void bvisit(const Basic &x){};

    bool apply(const Basic &b)
    {
        has_ = false;
        stop_ = false;
        preorder_traversal_stop(b, *this);
        return has_;
    }
};

bool has_symbol(const Basic &b, const Symbol &x);

class CoeffVisitor : public BaseVisitor<CoeffVisitor, StopVisitor>
{
protected:
    Ptr<const Basic> x_;
    Ptr<const Basic> n_;
    RCP<const Basic> coeff_;

public:
    CoeffVisitor(Ptr<const Basic> x, Ptr<const Basic> n) : x_(x), n_(n)
    {
    }

    void bvisit(const Add &x)
    {
        umap_basic_num dict;
        RCP<const Number> coef = zero;
        for (auto &p : x.get_dict()) {
            p.first->accept(*this);
            if (neq(*coeff_, *zero)) {
                Add::coef_dict_add_term(outArg(coef), dict, p.second, coeff_);
            }
        }
        coeff_ = Add::from_dict(coef, std::move(dict));
    }

    void bvisit(const Mul &x)
    {
        for (auto &p : x.get_dict()) {
            if (eq(*p.first, *x_) and eq(*p.second, *n_)) {
                map_basic_basic dict = x.get_dict();
                dict.erase(p.first);
                coeff_ = Mul::from_dict(x.get_coef(), std::move(dict));
                return;
            }
        }
        coeff_ = zero;
    }

    void bvisit(const Pow &x)
    {
        if (eq(*x.get_base(), *x_) and eq(*x.get_exp(), *n_)) {
            coeff_ = one;
        } else {
            coeff_ = zero;
        }
    }

    void bvisit(const Symbol &x)
    {
        if (eq(x, *x_) and eq(*one, *n_)) {
            coeff_ = one;
        } else {
            coeff_ = zero;
        }
    }

    void bvisit(const FunctionSymbol &x)
    {
        if (eq(x, *x_) and eq(*one, *n_)) {
            coeff_ = one;
        } else {
            coeff_ = zero;
        }
    }

    void bvisit(const Basic &x)
    {
        coeff_ = zero;
    }

    RCP<const Basic> apply(const Basic &b)
    {
        coeff_ = zero;
        b.accept(*this);
        return coeff_;
    }
};

RCP<const Basic> coeff(const Basic &b, const Basic &x, const Basic &n);

set_basic free_symbols(const Basic &b);

set_basic free_symbols(const MatrixBase &m);

class TransformVisitor : public BaseVisitor<TransformVisitor>
{
protected:
    RCP<const Basic> result_;

public:
    TransformVisitor()
    {
    }

    virtual RCP<const Basic> apply(const RCP<const Basic> &x);

    void bvisit(const Basic &x);
    void bvisit(const Add &x);
    void bvisit(const Mul &x);
    void bvisit(const Pow &x);
    void bvisit(const OneArgFunction &x);

    template <class T>
    void bvisit(const TwoArgBasic<T> &x)
    {
        auto farg1 = x.get_arg1(), farg2 = x.get_arg2();
        auto newarg1 = apply(farg1), newarg2 = apply(farg2);
        if (farg1 != newarg1 or farg2 != newarg2) {
            result_ = x.create(newarg1, newarg2);
        } else {
            result_ = x.rcp_from_this();
        }
    }

    void bvisit(const MultiArgFunction &x);
};

} // SymEngine

#endif
