#include <string>

namespace NS {
    namespace Foo {
        namespace Bar {
            typedef int Type;
        }
    }
}

using namespace NS::Foo::Bar;

/* comment 1 (NOT doc) */  /* comment 2 (NOT doc) */
struct MyBase {
    /* comment 3 (NOT doc) */
    /* comment 4 (doc) */
    Type member;
};

// documentation for MyClass begin {

//  } end
struct MyClass /* ??? */ :  MyBase {

    std::string string;  // documentation for string
    std::string string2; /*  and for string 2 */ /* more on string 2 */

    union {
        //this is an integer
/*and this */      int integer; // plop
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

    static std::string function(int hi,
                                bool ho,
                                double = 4 + 5,
                                std::string nothiung = std::string("hell") + "0" )
    { return MyClass().string2; }

    virtual std::string m() const {
        std::string local = string;
        return function(1,2,2);
    }

    int outofline(std::string foo);
};


int MyClass::outofline(std::string foo)
{
    return foo.size();
}

//Some enum
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
    struct InAnonymousNamspace {
        int someFunction();
        int member;
    };
}


/* doc */
int InAnonymousNamspace::someFunction()
{
    return member;
}
