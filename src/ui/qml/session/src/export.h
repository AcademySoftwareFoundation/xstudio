
#ifndef SESSION_QML_EXPORT_H
#define SESSION_QML_EXPORT_H

#ifdef SESSION_QML_STATIC_DEFINE
#  define SESSION_QML_EXPORT
#  define SESSION_QML_NO_EXPORT
#else
#  ifndef SESSION_QML_EXPORT
#    ifdef session_qml_EXPORTS
        /* We are building this library */
#      define SESSION_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define SESSION_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef SESSION_QML_NO_EXPORT
#    define SESSION_QML_NO_EXPORT 
#  endif
#endif

#ifndef SESSION_QML_DEPRECATED
#  define SESSION_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef SESSION_QML_DEPRECATED_EXPORT
#  define SESSION_QML_DEPRECATED_EXPORT SESSION_QML_EXPORT SESSION_QML_DEPRECATED
#endif

#ifndef SESSION_QML_DEPRECATED_NO_EXPORT
#  define SESSION_QML_DEPRECATED_NO_EXPORT SESSION_QML_NO_EXPORT SESSION_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef SESSION_QML_NO_DEPRECATED
#    define SESSION_QML_NO_DEPRECATED
#  endif
#endif

#endif /* SESSION_QML_EXPORT_H */
