#ifndef COMPAT_H
#define COMPAT_H

#if !GLIB_CHECK_VERSION(2, 32, 0)
#define g_thread_new(name, func, data)  g_thread_create(func, data, TRUE, NULL)
#endif

#endif
