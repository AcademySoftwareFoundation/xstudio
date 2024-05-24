
#ifndef PLAYHEAD_QML_EXPORT_H
#define PLAYHEAD_QML_EXPORT_H

#ifdef PLAYHEAD_QML_STATIC_DEFINE
#  define PLAYHEAD_QML_EXPORT
#  define PLAYHEAD_QML_NO_EXPORT
#else
#  ifndef PLAYHEAD_QML_EXPORT
#    ifdef playhead_qml_EXPORTS
        /* We are building this library */
#      define PLAYHEAD_QML_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define PLAYHEAD_QML_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef PLAYHEAD_QML_NO_EXPORT
#    define PLAYHEAD_QML_NO_EXPORT 
#  endif
#endif

#ifndef PLAYHEAD_QML_DEPRECATED
#  define PLAYHEAD_QML_DEPRECATED __declspec(deprecated)
#endif

#ifndef PLAYHEAD_QML_DEPRECATED_EXPORT
#  define PLAYHEAD_QML_DEPRECATED_EXPORT PLAYHEAD_QML_EXPORT PLAYHEAD_QML_DEPRECATED
#endif

#ifndef PLAYHEAD_QML_DEPRECATED_NO_EXPORT
#  define PLAYHEAD_QML_DEPRECATED_NO_EXPORT PLAYHEAD_QML_NO_EXPORT PLAYHEAD_QML_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PLAYHEAD_QML_NO_DEPRECATED
#    define PLAYHEAD_QML_NO_DEPRECATED
#  endif
#endif

#endif /* PLAYHEAD_QML_EXPORT_H */
