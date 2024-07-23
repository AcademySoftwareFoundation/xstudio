
#ifndef VIEWPORT_QML_EXPORT_H
#define VIEWPORT_QML_EXPORT_H

#ifdef VIEWPORT_QML_STATIC_DEFINE
#  define VIEWPORT_QML_EXPORT
#  define VIEWPORT_QML_NO_EXPORT
#else
#  ifndef VIEWPORT_QML_EXPORT
#    ifdef viewport_qml_EXPORTS
        /* We are building this library */
#      define VIEWPORT_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define VIEWPORT_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef VIEWPORT_QML_NO_EXPORT
#    define VIEWPORT_QML_NO_EXPORT 
#  endif
#endif

#ifndef VIEWPORT_QML_DEPRECATED
#  define VIEWPORT_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef VIEWPORT_QML_DEPRECATED_EXPORT
#  define VIEWPORT_QML_DEPRECATED_EXPORT VIEWPORT_QML_EXPORT VIEWPORT_QML_DEPRECATED
#endif

#ifndef VIEWPORT_QML_DEPRECATED_NO_EXPORT
#  define VIEWPORT_QML_DEPRECATED_NO_EXPORT VIEWPORT_QML_NO_EXPORT VIEWPORT_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef VIEWPORT_QML_NO_DEPRECATED
#    define VIEWPORT_QML_NO_DEPRECATED
#  endif
#endif

#endif /* VIEWPORT_QML_EXPORT_H */
