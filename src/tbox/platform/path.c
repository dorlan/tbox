/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2015, ruki All rights reserved.
 *
 * @author      ruki
 * @path        path.c
 * @ingroup     platform
 *
 */

/* //////////////////////////////////////////////////////////////////////////////////////
 * trace
 */
#define TB_TRACE_MODULE_NAME                "path"
#define TB_TRACE_MODULE_DEBUG               (0)

/* //////////////////////////////////////////////////////////////////////////////////////
 * includes
 */
#include "path.h"
#include "directory.h"
#include "../libc/libc.h"

/* //////////////////////////////////////////////////////////////////////////////////////
 * macros
 */

// the path separator
#ifdef TB_CONFIG_OS_WINDOWS
#   define TB_PATH_SEPARATOR        '\\'
#else
#   define TB_PATH_SEPARATOR        '/'
#endif

// is path separator?
#define tb_path_is_separator(c)     ((c) == '/' || (c) == '\\')

/* //////////////////////////////////////////////////////////////////////////////////////
 * implementation
 */
tb_size_t tb_path_translate(tb_char_t* path, tb_size_t size, tb_size_t maxn)
{
    // check
    tb_assert_and_check_return_val(path, 0);

    // file://?
    tb_char_t* p = path;
    if (!tb_strnicmp(p, "file:", 5)) p += 5;
    // is user directory?
    else if (path[0] == '~')
    {
        // get the home directory
        tb_char_t home[TB_PATH_MAXN];
        tb_size_t home_size = tb_directory_home(home, sizeof(home) - 1);
        tb_assert_and_check_return_val(home_size, 0);

        // check the path space
        tb_size_t path_size = size? size : tb_strlen(path);
        tb_assert_and_check_return_val(home_size + path_size - 1 < maxn, 0);

        // move the path and ensure the enough space for the home directory
        tb_memmov(path + home_size, path + 1, path_size - 1);

        // copy the home directory 
        tb_memcpy(path, home, home_size);
        path[home_size + path_size] = '\0';
    }

    // remove repeat separator
    tb_char_t*  q = path;
    tb_size_t   repeat = 0;
    for (; *p; p++)
    {
        if (tb_path_is_separator(*p))
        {
            // save the separator if not exists
            if (!repeat) *q++ = TB_PATH_SEPARATOR;

            // repeat it
            repeat++;
        }
        else 
        {
            // save character
            *q++ = *p;

            // clear repeat
            repeat = 0;
        }
    }

    // remove the tail separator
    if (q > path && *(q - 1) == TB_PATH_SEPARATOR) q--;

    // end
    *q = '\0';

    // is windows path? 
    if (q - path > 1 && tb_isalpha(path[0]) && path[1] == ':')
    {
        // get the upper drive prefix
        path[0] = tb_toupper(path[0]);

        // append the drive separator if not exists
        if (q - path == 2)
        {
            *q++ = TB_PATH_SEPARATOR;
            *q = '\0';
        }
    }

    // trace
    tb_trace_d("translate: %s", path);

    // ok
    return q - path;
}
tb_bool_t tb_path_is_absolute(tb_char_t const* path)
{
    // check
    tb_assert_and_check_return_val(path, tb_false);

    // is absolute?
#ifdef TB_CONFIG_OS_WINDOWS
    return (    path[0] == '~'
            ||  (tb_isalpha(path[0]) && path[1] == ':' && tb_path_is_separator(path[2])));
#else
    return (    path[0] == '/'
            ||  path[0] == '\\'
            ||  path[0] == '~'
            ||  !tb_strnicmp(path, "file:", 5));
#endif
}
tb_char_t const* tb_path_absolute(tb_char_t const* path, tb_char_t* data, tb_size_t maxn)
{
    return tb_path_absolute_to(tb_null, path, data, maxn);
}
tb_char_t const* tb_path_absolute_to(tb_char_t const* root, tb_char_t const* path, tb_char_t* data, tb_size_t maxn)
{
    // check
    tb_assert_and_check_return_val(path && data && maxn, tb_null);

    // trace
    tb_trace_d("path: %s", path);

    // the path is absolute?
    if (tb_path_is_absolute(path))
    {
        // copy it
        tb_strlcpy(data, path, maxn - 1);
        data[maxn - 1] = '\0';

        // translate it
        return tb_path_translate(data, 0, maxn)? data : tb_null;
    }

    // get the root directory
    tb_size_t size = 0;
    if (root)
    {
        // get the root size
        size = tb_strlen(root);
        tb_assert_and_check_return_val(size < maxn, tb_null);

        // copy it
        tb_strlcpy(data, root, size);
        data[size] = '\0';
    }
    else
    {
        // get the current directory
        if (!(size = tb_directory_current(data, maxn))) return tb_null;
    }

    // translate the root directory
    size = tb_path_translate(data, size, maxn);

    // trace
    tb_trace_d("root: %s, size: %lu", data, size);

    // is windows path? skip the drive prefix
    tb_char_t* absolute = data;
    if (size > 2 && tb_isalpha(absolute[0]) && absolute[1] == ':' && absolute[2] == TB_PATH_SEPARATOR)
    {
        // skip it
        absolute    += 2;
        size        -= 2;
    }

    // path => data
    tb_char_t const*    p = path;
    tb_char_t const*    t = p;
    tb_char_t*          q = absolute + size;
    tb_char_t const*    e = absolute + maxn - 1;
    while (1)
    {
        if (tb_path_is_separator(*p) || !*p)
        {
            // the item size
            tb_size_t n = p - t;

            // ..? remove item
            if (n == 2 && t[0] == '.' && t[1] == '.')
            {
                // find the last separator
                for (; q > absolute && *q != TB_PATH_SEPARATOR; q--) ;

                // strip it
                *q = '\0';
            }
            // .? continue it
            else if (n == 1 && t[0] == '.') ;
            // append item
            else if (n && q + 1 + n < e)
            {
                *q++ = TB_PATH_SEPARATOR;
                tb_strlcpy(q, t, n);
                q += n;
            }
            // empty item? remove repeat
            else if (!n) ;
            // too small?
            else 
            {
                // trace
                tb_trace_e("the data path is too small for %s", path);
                return tb_null;
            }

            // break
            tb_check_break(*p);

            // next
            t = p + 1;
        }

        // next
        p++;
    }

    // end
    if (q > absolute) *q = '\0';
    // root?
    else
    {
        *q++ = TB_PATH_SEPARATOR;
        *q = '\0';
    }

    // trace    
    tb_trace_d("absolute: %s", data);
    
    // ok?
    return data;
}
tb_char_t const* tb_path_relative(tb_char_t const* path, tb_char_t* data, tb_size_t maxn)
{
    return tb_path_relative_to(tb_null, path, data, maxn);
}
tb_char_t const* tb_path_relative_to(tb_char_t const* root, tb_char_t const* path, tb_char_t* data, tb_size_t maxn)
{
    // check
    tb_assert_and_check_return_val(path && data && maxn, tb_null);

    // trace
    tb_trace_d("path: %s", path);

    // the root is the current and the path is absolute? return path directly
    if (!root && !tb_path_is_absolute(path))
    {
        // copy it
        tb_strlcpy(data, path, maxn - 1);
        data[maxn - 1] = '\0';

        // translate it
        return tb_path_translate(data, 0, maxn)? data : tb_null;
    }

    // get the absolute path
    tb_size_t path_size = 0;
    tb_char_t path_absolute[TB_PATH_MAXN];
    tb_size_t path_maxn = sizeof(path_absolute);
    path        = tb_path_absolute(path, path_absolute, path_maxn);
    path_size   = tb_strlen(path);
    tb_assert_and_check_return_val(path && path_size && path_size < path_maxn, tb_null);

    // trace
    tb_trace_d("path_absolute: %s", path);

    // get the absolute root
    tb_size_t root_size = 0;
    tb_char_t root_absolute[TB_PATH_MAXN];
    tb_size_t root_maxn = sizeof(root_absolute);
    if (root) 
    {
        // get the absolute root
        root        = tb_path_absolute(root, root_absolute, root_maxn);
        root_size   = tb_strlen(root);
    }
    else
    {
        // get the current directory
        if (!(root_size = tb_directory_current(root_absolute, root_maxn))) return tb_null;

        // translate it
        if (!(root_size = tb_path_translate(root_absolute, root_size, root_maxn))) return tb_null;
        root = root_absolute;
    }
    tb_assert_and_check_return_val(root && root_size && root_size < root_maxn, tb_null);

    // trace
    tb_trace_d("root_absolute: %s", root);

    // same directory? return "."
    if (path_size == root_size && !tb_strncmp(path, root, root_size)) 
    {
        // check
        tb_assert_and_check_return_val(maxn >= 2, ".");

        // return "."
        data[0] = '.';
        data[1] = '\0';
        return data;
    }

    // append separator
    if (path_size + 1 < path_maxn)
    {
        path_absolute[path_size++] = TB_PATH_SEPARATOR;
        path_absolute[path_size] = '\0';
    }
    if (root_size + 1 < root_maxn) 
    {
        root_absolute[root_size++] = TB_PATH_SEPARATOR;
        root_absolute[root_size] = '\0';
    }

    // find the common leading directory
    tb_char_t const*    p = path;
    tb_char_t const*    q = root;
    tb_long_t           last = -1;
    for (; *p && *q && *p == *q; q++, p++)
    {
        // save the last separator
        if (*p == TB_PATH_SEPARATOR) last = q - root;
    }

    // is different directory or outside the windows drive root? using the absolute path
    if (last <= 0 || (last == 2 && root[1] == ':'))
    {
        // the path size
        tb_size_t size = tb_min(path_size - 1, maxn);

        // copy it
        tb_strlcpy(data, path, size);
        data[size] = '\0';
    }
    // exists same root?
    else
    {
        // count the remaining levels in root
        tb_size_t count = 0;
        tb_char_t const* l = root + last + 1;
        for (; *l; l++)
        {
            if (*l == TB_PATH_SEPARATOR) count++;
        }

        // append "../" or "..\\"
        tb_char_t* d = data;
        tb_char_t* e = data + maxn;
        while (count--)
        {
            if (d + 3 < e)
            {
                d[0] = '.';
                d[1] = '.';
                d[2] = TB_PATH_SEPARATOR;
                d += 3;
            }
        }

        // append the left path
        l = path + last + 1;
        while (*l && d < e) *d++ = *l++;

        // remove the last separator
        if (d > data) d--;

        // end
        *d = '\0';
    }

    // trace    
    tb_trace_d("relative: %s", data);
    
    // ok?
    return data;
}
