#include <QtGui/QtGui>
#include <QtTest/QSignalSpy>

class MyObjectPrivate {
public:
    void myPrivateSlot();
};

class MyObject : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MyObject)
public slots:

    void superSlot1(const QString &myString, const uint s);

    // this slot can be connected to everythinig
    void superSlot2(void * = nullptr);

    void anotherSlot(const QString &str1, unsigned int i1, const QMap<int, QString> &map);

signals:
    // this one contains unsigned
    void mySignal1(const QString &str1, unsigned int i1, const QMap<int, QString> &map);

    // with a default argument
    void mySignal2(QString, uint c = 0);

    void pointerSignal(const QObject *);

    void myBoolSignal(bool);

//    void invalidSignal(InvalidTypeGoesHere);

    void downloadMetaData(QList<QPair<QByteArray,QByteArray> >,int,QString,bool,QSharedPointer<char>,qint64);

public:
    QPainter *p;

};


void MyObject::superSlot1(const QString& myString, const uint s)
{
    MyObject stackOj;
    connect(&stackOj, SIGNAL(mySignal1(QString,   uint   ,QMap<int,QString>)), SLOT(anotherSlot(QString,uint,QMap<int,QString>)));

    connect(&stackOj, SIGNAL(mySignal2( const QString & ,uint)), this, SLOT(superSlot1(  const  QString &, const unsigned int & )));

    connect(this, SIGNAL    (   destroyed(QObject*)   ), this, SLOT(superSlot2()));

    stackOj.connect(this, SIGNAL(mySignal1(QString,uint,QMap<int,QString>)), SLOT(superSlot1(QString,uint)));

    connect(this, SIGNAL(pointerSignal(const QObject*)) ,this, SLOT(myPrivateSlot()));
    connect(this, SIGNAL(myBoolSignal(bool)) ,this, SLOT(deleteLater()));
    connect(this, SIGNAL(downloadMetaData(QList<QPair<QByteArray,QByteArray> >,int,QString,bool,QSharedPointer<char>,qint64)),
            this, SLOT(deleteLater()));
}


#define DO( _, X ) X

#define CONNECT  connect(sender(), SIGNAL(objectNameChanged(QString)), qApp, SLOT(quit()))

void MyObject::superSlot2(void*)
{
    QTimer::singleShot(154, this, SLOT(superSlot2()));

    DO( _, connect(sender(), SIGNAL( destroyed(QObject*)), qApp,  SLOT(quit())) );

    CONNECT;

    QSignalSpy s(qApp, SIGNAL(applicationNameChanged()));
}


void MyObject::anotherSlot(const QString& str1, unsigned int i1, const QMap< int, QString >& map)
{
    QScopedPointer<QTimer> t(new QTimer);

    connect(t.data(), SIGNAL(timeout()), &*t, SLOT(start()));

    t->connect(this, SIGNAL(destroyed(QObject*)), SLOT(stop()));

    t->connect(this, SIGNAL(invalidSignal(ErrorType)), SLOT(deletaLater()));

    disconnect(this, SIGNAL(mySignal1(QString,uint,QMap<int,QString>)), 0, 0);

    disconnect(this, SIGNAL(pointerSignal(const QObject*)), 0, 0);
}


class OverLoadTest : public QObject {
  Q_OBJECT
signals:
    void ovr(int = 4);
    void ovr(const QString &);
    void ovr(const QString &, int);

public slots:
    void doThings() {
        connect(this, SIGNAL(ovr()), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(int)), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(QString)), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(QString, int)), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(QString*)), this, SLOT(doThings())); //error on purpose
    }
};

class OverrideTest : public OverLoadTest {
signals:
    void ovr(int);  // from reimpl

public slots:
    void doThings(int = 4) {
        connect(this, SIGNAL(ovr()), this, SLOT(doThings())); //ovr should NOT be from reimpl
        connect(this, SIGNAL(ovr(int)), this, SLOT(doThings(int))); // should be from reimpl
        connect(this, SIGNAL(ovr(QString)), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(QString, int)), this, SLOT(doThings()));
        connect(this, SIGNAL(ovr(QString*)), this, SLOT(doThings())); //error on purpose
    }
};


namespace TestNS {

class C : public QObject {
    Q_OBJECT
signals:
    void sig1(C *foo);
    void sig2(TestNS::C *foo);
public slots:
    void MySlot() {
        connect(this, SIGNAL(sig1(C*)), this, SLOT(deleteLater()));
        connect(this, SIGNAL(sig2(TestNS::C*)), this, SLOT(deleteLater()));
    }
};

}

int ndbug(TestNS::C *c) {
#define S1GNAL(X) "2" #X
#define SL0T(X) "1" #X
    QObject::connect(c, S1GNAL(sig2(TestNS::C*)), c, SL0T(MySlot()));
}


class QQQQQ {
public:
    void operator+(int) {}
};
void dontcrash() {
    QQQQQ q; q + 4;
}
