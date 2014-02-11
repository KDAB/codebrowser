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
    * <a href="http://woboq.com">and a \c url</a>
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
    DocMyClass();

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

        return nullptr;
       /**
        * \fn DocMyClass *DocMyClass::returnPointer();
        *  *
        */
    }

};



