#include <vector>
#include <string>


   /**
    * \class DocMyClass
    * \brief the class
    *
    * @code
    *  Here comes some code
    *  * Continued
    * @endcode
    *
    * <a href="https://woboq.com">and a \c url</a>
    *
    * some more description here, i should really write a lot
    *
    * Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam lectus. Sed sit amet
    * ipsum mauris. Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit. Donec
    * et mollis dolor. Praesent et diam eget libero egestas mattis sit amet vitae augue. Nam
    * tincidunt congue enim, ut porta lorem lacinia consectetur. Donec ut libero sed arcu vehicula
    * ultricies a non tortor. Lorem ipsum dolor @c sit amet, consectetur adipiscing elit. Aenean ut
    * gravida lorem. Ut turpis felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque
    * auctor nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit pharetra
    * tincidunt feugiat nisl imperdiet. Ut convallis libero in urna ultrices accumsan. Donec sed
    * odio eros. Donec viverra mi quis quam pulvinar at malesuada arcu rhoncus. Cum sociis natoque
    * penatibus et magnis dis parturient montes, nascetur ridiculus mus. In rutrum accumsan
    * ultricies. Mauris vitae nisi at sem facilisis semper ac in est.
    *
    * Class aptent taciti sociosqu ad litora torquent per conubia nostra, per inceptos himenaeos.
    * In euismod ultrices facilisis. Vestibulum porta sapien adipiscing augue congue id pretium
    * lectus molestie. Proin quis dictum nisl. Morbi id quam sapien, sed vestibulum sem. Duis
    * elementum rutrum mauris sed convallis. Proin vestibulum magna mi. Aenean tristique hendrerit
    * magna, ac facilisis nulla hendrerit ut. Sed non tortor sodales quam auctor elementum. Donec
    * hendrerit nunc eget elit pharetra pulvinar. Suspendisse id tempus tortor. Aenean luctus,
    * elit commodo laoreet commodo, justo nisi consequat massa, sed vulputate quam urna quis eros.
    * Donec vel.
    *
    * \note foobar
    *
    * FIXME yoyo
    * @fixme fixme
    * @todo todo
    *
    */

int noDocumentationPlease();

class DocMyClass {
private: void somePrivateMember();
protected: void someProtectedMember();
public:


/**
 * \fn DocMyClass::somePrivateMember
 * private member
 */
/**
 * \fn DocMyClass::someProtectedMember
 * protected member
 */

    virtual ~DocMyClass();
    DocMyClass() : Buffer (0) {} ;

    /**
     * \fn DocMyClass::~DocMyClass
     *  Destructor
     */


    /**
     * \fn DocMyClass::DocMyClass
     *  Constructor
     */


    int fooOverload();
    int fooOverload(int);

    int constOverload(int);
    int constOverload(int) const;

/**
 * \fn DocMyClass::fooOverload()
 *  A
 */
/**
 * \fn DocMyClass::fooOverload(int)
 *  B
 */
/**
 * \fn DocMyClass::constOverload(int)
 *  C
 */
/**
 * \fn DocMyClass::constOverload(int) const
 *  D
 */

    DocMyClass *returnPointer() {
        Capacity = 8;
        return nullptr;
       /**
        * \fn DocMyClass *DocMyClass::returnPointer();
        *  *
        */
    }

private:

    /// Doc for buffer;
    /// continued
    char *Buffer;

    /* doc for capacity */
    int Capacity;

    // doc for index
    int Index;
};


/*
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
 */
int wow;


template <class T>
class SuperList {
public:
    SuperList();
    SuperList(T);
    SuperList(int, int);
    ~SuperList();
    int at(int);
};

/*! \class SuperList  */

/*! \fn SuperList::SuperList()  */
/*! \fn SuperList::SuperList(T)  */
/*! \fn SuperList::at  */


enum DocumentedEnum {
    A, Hello, Yolla
};

/*!  \enum DocumentedEnum
 * \value A some value
 * \value Hello this is some other value that is verry interesting so the text is a
 *        bit logner but it's still one
 * \value Yolla that's another value
 *
 * and here some more text about the enum
 */