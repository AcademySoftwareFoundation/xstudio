
#ifndef HELPER_QML_EXPORT_H
#define HELPER_QML_EXPORT_H

#ifdef HELPER_QML_STATIC_DEFINE
#  define HELPER_QML_EXPORT
#  define HELPER_QML_NO_EXPORT
#else
#  ifndef HELPER_QML_EXPORT
#    ifdef helper_qml_EXPORTS
        /* We are building this library */
#      define HELPER_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define HELPER_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef HELPER_QML_NO_EXPORT
#    define HELPER_QML_NO_EXPORT 
#  endif
#endif

#ifndef HELPER_QML_DEPRECATED
#  define HELPER_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef HELPER_QML_DEPRECATED_EXPORT
#  define HELPER_QML_DEPRECATED_EXPORT HELPER_QML_EXPORT HELPER_QML_DEPRECATED
#endif

#ifndef HELPER_QML_DEPRECATED_NO_EXPORT
#  define HELPER_QML_DEPRECATED_NO_EXPORT HELPER_QML_NO_EXPORT HELPER_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef HELPER_QML_NO_DEPRECATED
#    define HELPER_QML_NO_DEPRECATED
#  endif
#endif

#endif /* HELPER_QML_EXPORT_H */
