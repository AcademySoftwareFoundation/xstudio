
#ifndef TAG_QML_EXPORT_H
#define TAG_QML_EXPORT_H

#ifdef TAG_QML_STATIC_DEFINE
#  define TAG_QML_EXPORT
#  define TAG_QML_NO_EXPORT
#else
#  ifndef TAG_QML_EXPORT
#    ifdef tag_qml_EXPORTS
        /* We are building this library */
#      define TAG_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define TAG_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef TAG_QML_NO_EXPORT
#    define TAG_QML_NO_EXPORT 
#  endif
#endif

#ifndef TAG_QML_DEPRECATED
#  define TAG_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef TAG_QML_DEPRECATED_EXPORT
#  define TAG_QML_DEPRECATED_EXPORT TAG_QML_EXPORT TAG_QML_DEPRECATED
#endif

#ifndef TAG_QML_DEPRECATED_NO_EXPORT
#  define TAG_QML_DEPRECATED_NO_EXPORT TAG_QML_NO_EXPORT TAG_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef TAG_QML_NO_DEPRECATED
#    define TAG_QML_NO_DEPRECATED
#  endif
#endif

#endif /* TAG_QML_EXPORT_H */
