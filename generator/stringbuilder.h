#include <string>
#include <cstring>

template<typename T> struct string_builder_helper;

template <typename A, typename B> struct string_builder {
    typedef string_builder_helper<A> HA;
    typedef string_builder_helper<B> HB;
    operator std::string() const {
        std::string s;
        s.reserve(size());
        HA::append_to(s, a);
        HB::append_to(s, b);
        return s;
    }
    unsigned int size() const { return HA::size(a) + HB::size(b); }

    string_builder(const A &a, const B &b) : a(a), b(b) {}
    const A &a;
    const B &b;
};

template<> struct string_builder_helper<std::string> {
    typedef std::string T;
    static unsigned int size(const std::string &s) { return s.size(); }
    static void append_to(std::string &s, const std::string &a) { s+=a; }
};

template<> struct string_builder_helper<const char *> {
    typedef const char *T;
    static unsigned int size(const char *s) { return std::strlen(s); }
    static void append_to(std::string &s, const char *a) { s+=a; }
};

template<typename A, typename B> struct string_builder_helper<string_builder<A,B>> {
    typedef string_builder<A,B> T;
    static unsigned int size(const T &t)  { return t.size(); }
    static void append_to(std::string &s, const T &t) {
        T::HA::append_to(s, t.a);
        T::HB::append_to(s, t.b);
    }
};

template<int N> struct string_builder_helper<char[N]> {
    typedef char T[N];
    static unsigned int size(const char *s) { return N-1; }
    static void append_to(std::string &s, const char *a) { s.append(a, N-1); }
};

template<typename A, typename B>
string_builder<typename string_builder_helper<A>::T, typename string_builder_helper<B>::T>
operator %(const A &a, const B &b) {
    return {a, b};
}

template<typename T> std::string &operator %=(std::string &s, const T &t) {
    typedef string_builder_helper<T> H;
    s.reserve(s.size() + H::size(t));
    H::append_to(s, t);
    return s;
}

#include <llvm/ADT/StringRef.h>
template<> struct string_builder_helper<llvm::StringRef> {
  typedef llvm::StringRef T;
  static unsigned int size(llvm::StringRef s) { return s.size(); }
  static void append_to(std::string &s, llvm::StringRef a) { s+=a; }
};
