
#ifndef EVENT_QML_EXPORT_H
#define EVENT_QML_EXPORT_H

#ifdef EVENT_QML_STATIC_DEFINE
#  define EVENT_QML_EXPORT
#  define EVENT_QML_NO_EXPORT
#else
#  ifndef EVENT_QML_EXPORT
#    ifdef event_qml_EXPORTS
        /* We are building this library */
#      define EVENT_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define EVENT_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef EVENT_QML_NO_EXPORT
#    define EVENT_QML_NO_EXPORT 
#  endif
#endif

#ifndef EVENT_QML_DEPRECATED
#  define EVENT_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef EVENT_QML_DEPRECATED_EXPORT
#  define EVENT_QML_DEPRECATED_EXPORT EVENT_QML_EXPORT EVENT_QML_DEPRECATED
#endif

#ifndef EVENT_QML_DEPRECATED_NO_EXPORT
#  define EVENT_QML_DEPRECATED_NO_EXPORT EVENT_QML_NO_EXPORT EVENT_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef EVENT_QML_NO_DEPRECATED
#    define EVENT_QML_NO_DEPRECATED
#  endif
#endif

#endif /* EVENT_QML_EXPORT_H */
