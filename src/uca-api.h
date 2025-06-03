#ifndef UCA_API_H
#define UCA_API_H

#if defined(UCA_API_MSVC_IMPORT)
# define UCA_API __declspec(dllimport)
#elif defined(UCA_API_MSVC_EXPORT)
# define UCA_API __declspec(dllexport)
#else
# define UCA_API
#endif

#endif
