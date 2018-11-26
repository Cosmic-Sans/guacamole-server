#ifndef _UNISTD_H
#define _UNISTD_H

#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t long
#endif

#endif