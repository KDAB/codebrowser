#include <string>
#include <boost/concept_check.hpp>

#include "notexist.g"

#include "doc.h"


namespace NS {
    namespace Foo {
        namespace Bar {
            typedef int Type;

            class T {
                int m( T *t) {
                    t->Foo::Bar::T::~T();
                }
            };
        }
    }
}

using namespace NS::Foo::Bar;
using namespace NS;

/* comment 1 (NOT doc) */  /* comment 2 (NOT doc) */
struct MyBase {
    /* comment 3 (NOT doc) */
    /* comment 4 (doc) */
    Type member;
    static Type static_member;
    MyBase();

    virtual ~MyBase() = default;
};

MyBase::MyBase() { }

/* here is a static member */
Type MyBase::static_member = 8;

// documentation for MyClass begin {

//  } end
struct MyClass /* ??? */ :  MyBase {

    XXXXX undefined_type;

    std::string string;  /// documentation for string
    std::string string2; /**  and for string 2 */ /* more on string 2 */

    union {
        //this is an integer
/*!and this */      int integer; // plop
                                // plop
/* what is this */    double dbl;
    };

    MyClass() :
        MyBase() ,         // type
        string("hello"),   //member
        dbl(0.6)           //ref
    {   member = 3;       //member
        bool a, b, c, d, e, f, g, h, i, j, k, l;
    }

    ~ MyClass();

    static std::string function(int hi,
                                bool ho,
                                double = 4 + 5,
                                std::string nothiung = std::string("hell") + "0" )
    { return MyClass().string2; }

    virtual std::string m() const {
        std::string local = string;
        return function(1,2,2);
    }

    inline int hey() {
        return m().size();
    }

    int outofline(std::string foo);

    operator std::string() const { return m(); }
};

/**
 * Regression for getFieldOffset(), founded during processing `struct _IO_FILE`
 */
struct ForwardDeclareWillBeDeclaredAfter;
struct ForwardDeclareWillBeDeclaredAfter
{
    int mustBailHere;
};

//http://code.woboq.org this is an url

/*!
 * yo
 * @param foo bar foo
 *
 * <a href="http://google.com">yo</a>
 *
 * @code
 *  foo bar foo
 * @endcode
 *
 * @c hello world  @c{yoyo m}
 * @li djq skql qslk
 *
 * dsd
 *
 * (https://woboq.com/)
 * visit https://woboq.com/blog.
 * https://en.wikipedia.org/wiki/Qt_(software)
 *
 */

int MyClass::outofline(std::string foo)
{
    return foo.size();
}

MyClass::~ /* random  comment  */ MyClass()
{

}

///Some enum
/// @brief Just an enum

enum MyEnum { Val1, //comment1
//comment2
Val2 = 3 + 3 };
MyEnum e = Val2;

class Annotator;

#define DECLARE_FUNCTION(TYPE, NAME, ARG1)  TYPE NAME (ARG1)

#define myMacro(MOO) MOO

//Some macro
DECLARE_FUNCTION(MyEnum, yoyo, MyClass&a) {

    myMacro(a.m());
    return a.member ?
                Val1 :
                Val2;
}

int function() {
    struct {
        int m = 5;
        int operator +(
            int i) {
            return i+m;
        }
    }foo;

    return foo
        + //FIXME
      foo.m;
}

extern __typeof (function) function_alias1 __attribute__ ((weak, alias ("function")));
extern __typeof (function) function_alias2 __attribute__ ((alias ("function")));

std::string toString() {
    function();
    function_alias1();
    function_alias2();
    MyClass c;
    c.function(noDocumentationPlease(), !c.outofline(std::string("T")), MyEnum::Val1, toString());
#define STRIGNIFY(X) #X
    return STRIGNIFY(true && "hello \"world\"")
}

int hasOwnProperty() {
    function()
        + function();
    std::string foo("GHG");
    return hasOwnProperty();
}


struct MyClassDerived : MyClass {
    virtual std::string m() const override;
};

std::string MyClassDerived::m() const
{
    extern int* oh();
    oh();
    return MyClass::m();
}

#define BUILD_NS(A, B) A::B
#define BUILD_NS2(A, B) ::A::B
BUILD_NS(NS::Foo::Bar ,  Type) tst_mac1;
BUILD_NS2(NS::Foo::Bar ,  Type) tst_mac2;
BUILD_NS(NS::Foo, Bar::Type) tst_mac3;
NS::BUILD_NS(Foo, Bar)::Type tst_mac4;
NS BUILD_NS2(Foo, Bar)::Type tst_mac5;

void test_macros() {
    typedef int TTFoo;
    TTFoo a;
#define TTFoo char
    TTFoo b;
#undef TTFoo
    TTFoo c;
};

//DECLARE_SOMETHING Macro
#define DECLARE_SOMETHING(WHAT) WHAT mySomething \
        { int foo(int a); }

/* This is  mySomething struct */
DECLARE_SOMETHING(struct);

int mySomething::foo(int a) {
    return a;
}


namespace {
    /* class
       that is annonymus */
    struct InAnonymousNamspace {
        int someFunction();
        /* member
         * doc */
        int member;
    };
}

/* doc
   */
int InAnonymousNamspace::someFunction()
{
    return member;
}
    /**
     * @deprecated foobar
     *
     */

static int some_static_func() {
    extern int extern_val;
    static int local_static;
    InAnonymousNamspace  a;
    local_static ++;
    return a.someFunction() + extern_val + local_static;
}

static int static_val;
int nonstatic_val;

int *oh() {
    extern int extern_val;
    goto label;
    nonstatic_val += extern_val;
    static_val += some_static_func();
label:
    int q = [&]() {
       extern_val++;
       static_val++;
       nonstatic_val++;
label:
       static std::string local_static;
       local_static = "hello";
       int loc = 4;
       struct MyStruct : MyBase {
           void operator+=(int q) {
label:
               static_member += q;
               goto label;
           }
           /* that's magic */
           int foo() { return 4; }
       };

       goto label;
       MyStruct s;
       goto label;
       s+=loc;
       return loc + s.foo();
    }();
    if (q) {
        goto label;
        return &extern_val;
    }
    return &nonstatic_val;
}

class ForwardDeclare;
int func() {
    ForwardDeclare thisIsAnError(34);
}

struct ClassWithForward {
    ForwardDeclare anotherError;
};
int func2() {
    ClassWithForward yo;
}


#if 1

#if false && \
      true && false
int foo_a;
#elif truee && false
int foo_r;
#else
int foo_d;
#endif
int foo_b:
#endif
#if some

#endif

#endif


namespace rel_ops = std::rel_ops;


extern int value;
struct Val { int v;
    void operator+=(const Val &);
} valval;
const Val operator+(const Val&, const Val&);
struct Uses {
    void operator[](int);
    void refFunc(int&);
    int hello;
    Val hello2;
    Uses(int i) : hello(i) {}
    int r() {
        auto v = value;
        v = value;
        v = { value };
        v = Uses{value}.hello;
        Val vvv = valval;
        vvv = valval;
        vvv = hello2;
        (*this)[value];
        while(value) { }
        return value;
    }
    int w() {
        Val q;
        valval = q + q;
        q.v = 4;
        (&q)->v = 45 + 45;
        hello = 4;
        hello2 = Val() + Val();
        Uses *u = this;
        u->hello2 = Val() + Val();
        (*u).hello2 = Val() + Val();
        hello2 += Val() + Val();
        value = 2;
        value += 2;
        value++;
        ++value;
        switch(0) { case 0: value = 1; }
        return 12;
    }
    int &a() {
        auto *val = &valval;
        auto &v1 = value;
        auto v2 = &value;
        auto &xx = (&valval)->v;
        auto *hi = &hello;
        refFunc(value);
        return value;
    }
};

int &func(int &x, int &y)
{ return x == 1 ? func(y, func(y, func(y, x))) : x; }

namespace SUPERlongNAMeSpace12345679814563efrqslkdjfq__hsdqsfsdqsdfqdsfqsdfqsdfqsdfgsgfhfjhsgs {

template<typename T> struct Long123456789123456789123456789FIFI {
    enum ValSUPERLdfqsdfqsdfqsdfqsdfqdfqsdfqsdfqsdfqsdfqsdfqsdf { Plop = sizeof(T) };

    template <typename T2> std::vector<T2> FuncLong12345678912345678912345678 (const T2&) {
        return {};
    }

    ValSUPERLdfqsdfqsdfqsdfqsdfqdfqsdfqsdfqsdfqsdfqsdfqsdf dsqsdqsdfqsdfkjlqsdflkqhsdflkqhsdflkqjhsdflkqjhsdflqkjhsflkqsjdlk;
};

int testLong() {
    Long123456789123456789123456789FIFI<Long123456789123456789123456789FIFI<std::vector<Long123456789123456789123456789FIFI<std::string>>>> ff;
    ff.FuncLong12345678912345678912345678( &Long123456789123456789123456789FIFI<std::string>::FuncLong12345678912345678912345678<decltype(ff)> );

    ff.FuncLong12345678912345678912345678(ff.dsqsdqsdfqsdfkjlqsdflkqhsdflkqhsdflkqjhsdflkqjhsdflqkjhsflkqsjdlk);
}

}


