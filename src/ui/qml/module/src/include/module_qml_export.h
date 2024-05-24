
#ifndef MODULE_QML_EXPORT_H
#define MODULE_QML_EXPORT_H

#ifdef MODULE_QML_STATIC_DEFINE
#  define MODULE_QML_EXPORT
#  define MODULE_QML_NO_EXPORT
#else
#  ifndef MODULE_QML_EXPORT
#    ifdef module_qml_EXPORTS
        /* We are building this library */
#      define MODULE_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define MODULE_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef MODULE_QML_NO_EXPORT
#    define MODULE_QML_NO_EXPORT 
#  endif
#endif

#ifndef MODULE_QML_DEPRECATED
#  define MODULE_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef MODULE_QML_DEPRECATED_EXPORT
#  define MODULE_QML_DEPRECATED_EXPORT MODULE_QML_EXPORT MODULE_QML_DEPRECATED
#endif

#ifndef MODULE_QML_DEPRECATED_NO_EXPORT
#  define MODULE_QML_DEPRECATED_NO_EXPORT MODULE_QML_NO_EXPORT MODULE_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef MODULE_QML_NO_DEPRECATED
#    define MODULE_QML_NO_DEPRECATED
#  endif
#endif

#endif /* MODULE_QML_EXPORT_H */
