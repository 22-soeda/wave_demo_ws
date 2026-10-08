#ifndef FEETECH_DRIVER__VISIBILITY_CONTROL_H_
#define FEETECH_DRIVER__VISIBILITY_CONTROL_H_

// This logic was borrowed (then namespaced) from the examples on the gcc wiki:
//     https://gcc.gnu.org/wiki/Visibility

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define FEETECH_DRIVER_EXPORT __attribute__ ((dllexport))
    #define FEETECH_DRIVER_IMPORT __attribute__ ((dllimport))
  #else
    #define FEETECH_DRIVER_EXPORT __declspec(dllexport)
    #define FEETECH_DRIVER_IMPORT __declspec(dllimport)
  #endif
  #ifdef FEETECH_DRIVER_BUILDING_LIBRARY
    #define FEETECH_DRIVER_PUBLIC FEETECH_DRIVER_EXPORT
  #else
    #define FEETECH_DRIVER_PUBLIC FEETECH_DRIVER_IMPORT
  #endif
  #define FEETECH_DRIVER_PUBLIC_TYPE FEETECH_DRIVER_PUBLIC
  #define FEETECH_DRIVER_LOCAL
#else
  #define FEETECH_DRIVER_EXPORT __attribute__ ((visibility("default")))
  #define FEETECH_DRIVER_IMPORT
  #if __GNUC__ >= 4
    #define FEETECH_DRIVER_PUBLIC __attribute__ ((visibility("default")))
    #define FEETECH_DRIVER_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define FEETECH_DRIVER_PUBLIC
    #define FEETECH_DRIVER_LOCAL
  #endif
  #define FEETECH_DRIVER_PUBLIC_TYPE
#endif

#endif  // FEETECH_DRIVER__VISIBILITY_CONTROL_H_
