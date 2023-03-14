#if defined(UCA_API_WINDOWS_IMPORT)
# define UCA_API __declspec(dllimport)
#elif defined(UCA_API_WINDOWS_EXPORT)
# define UCA_API __declspec(dllexport)
#else
# define UCA_API
#endif
