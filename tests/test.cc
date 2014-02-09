#include <string>
#include <boost/concept_check.hpp>

#include "notexist.g"

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
 * whaaat?
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
    nonstatic_val += extern_val;
    static_val += some_static_func();
    int q = [&]() {
       extern_val++;
       static_val++;
       nonstatic_val++;
       static std::string local_static;
       local_static = "hello";
       int loc = 4;
       struct MyStruct : MyBase {
           void operator+=(int q) {
               static_member += q;
           }
           /* that's magic */
           int foo() { return 4; }
       };

       MyStruct s;
       s+=loc;
       return loc + s.foo();
    }();
    if (q) {
        return &extern_val;
    }
    return &nonstatic_val;
}


/**
 * \class MyClassDerived
 *
 * out of line comment for MyClassDerived
 *
 */

/**
 * \fn MyClassDerived::m
 *
 * out of line comment for m
 *
 */