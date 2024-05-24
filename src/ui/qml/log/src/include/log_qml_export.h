
#ifndef LOG_QML_EXPORT_H
#define LOG_QML_EXPORT_H

#ifdef LOG_QML_STATIC_DEFINE
#  define LOG_QML_EXPORT
#  define LOG_QML_NO_EXPORT
#else
#  ifndef LOG_QML_EXPORT
#    ifdef log_qml_EXPORTS
        /* We are building this library */
#      define LOG_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define LOG_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef LOG_QML_NO_EXPORT
#    define LOG_QML_NO_EXPORT 
#  endif
#endif

#ifndef LOG_QML_DEPRECATED
#  define LOG_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef LOG_QML_DEPRECATED_EXPORT
#  define LOG_QML_DEPRECATED_EXPORT LOG_QML_EXPORT LOG_QML_DEPRECATED
#endif

#ifndef LOG_QML_DEPRECATED_NO_EXPORT
#  define LOG_QML_DEPRECATED_NO_EXPORT LOG_QML_NO_EXPORT LOG_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef LOG_QML_NO_DEPRECATED
#    define LOG_QML_NO_DEPRECATED
#  endif
#endif

#endif /* LOG_QML_EXPORT_H */
