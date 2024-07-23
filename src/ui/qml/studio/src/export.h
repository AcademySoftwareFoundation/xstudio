
#ifndef STUDIO_QML_EXPORT_H
#define STUDIO_QML_EXPORT_H

#ifdef STUDIO_QML_STATIC_DEFINE
#  define STUDIO_QML_EXPORT
#  define STUDIO_QML_NO_EXPORT
#else
#  ifndef STUDIO_QML_EXPORT
#    ifdef studio_qml_EXPORTS
        /* We are building this library */
#      define STUDIO_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define STUDIO_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef STUDIO_QML_NO_EXPORT
#    define STUDIO_QML_NO_EXPORT 
#  endif
#endif

#ifndef STUDIO_QML_DEPRECATED
#  define STUDIO_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef STUDIO_QML_DEPRECATED_EXPORT
#  define STUDIO_QML_DEPRECATED_EXPORT STUDIO_QML_EXPORT STUDIO_QML_DEPRECATED
#endif

#ifndef STUDIO_QML_DEPRECATED_NO_EXPORT
#  define STUDIO_QML_DEPRECATED_NO_EXPORT STUDIO_QML_NO_EXPORT STUDIO_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef STUDIO_QML_NO_DEPRECATED
#    define STUDIO_QML_NO_DEPRECATED
#  endif
#endif

#endif /* STUDIO_QML_EXPORT_H */
