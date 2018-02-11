#include <string.h>

// https://stackoverflow.com/questions/23999797/implementing-strnstr
char *strnstr(const char *haystack, const char *needle, size_t len)
{
    int i;
    size_t needle_len;

    if (0 == (needle_len = strnlen(needle, len)))
        return (char *)haystack;

    for (i=0; i<=(int)(len-needle_len); i++)
    {
        if ((haystack[0] == needle[0]) &&
            (0 == strncmp(haystack, needle, needle_len)))
            return (char *)haystack;

        haystack++;
    }
    return NULL;
}

// https://sourceforge.net/p/mingw/msys2-runtime/ci/bee9494d815a228b6ba217811f3df289da18cb35/tree/newlib/libc/string/strcasestr.c
char *
strcasestr(s, find)
	const char *s, *find;
{
  /* Less code size, but quadratic performance in the worst case.  */
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}