/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 yetist <yetist@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <windows.h>
#include <wchar.h>
#include <shlwapi.h>

LPCWSTR chinese_linux_fonts[] = {
    L"wqy-microhei.ttc",
    L"SourceHanSansCN-Regular.otf",
    L"NotoSerifCJK-Regular.ttc",
    L"NotoSansCJK-Regular.ttc",
    NULL
};

static void console_printf (LPCWSTR format, ...)
{
    DWORD count;
    WCHAR buf[1024];
    va_list arg_list;

    va_start (arg_list, format) ;
    vsnwprintf (buf, sizeof(buf) / sizeof (WCHAR), format, arg_list);
    va_end (arg_list) ;

    if (!WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), buf, wcslen(buf), &count, NULL))
    {
        DWORD len;
        char  *msg;

        /* WriteConsole() fails on Windows if its output is redirected. If this occurs,
         * we should call WriteFile() with OEM code page.
         */
        len = WideCharToMultiByte(GetOEMCP(), 0, buf, wcslen(buf), NULL, 0, NULL, NULL);
        msg = malloc(len);
        if (!msg)
            return;

        WideCharToMultiByte(GetOEMCP(), 0, buf, wcslen(buf), msg, len, NULL, NULL);
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, len, &count, FALSE);
        free(msg);
    }
}

static HKEY open_reg_key(HKEY root_key, LPCWSTR sub_key)
{
    HKEY key = NULL;
    LONG result;

    result = RegOpenKeyExW(root_key, sub_key, 0, KEY_ALL_ACCESS, &key);

    if (result == ERROR_FILE_NOT_FOUND)
    {
        result = RegCreateKeyExW(root_key, sub_key, 0, NULL, REG_OPTION_NON_VOLATILE,
                                 KEY_ALL_ACCESS, NULL, &key, NULL);
        if (result) {
            console_printf(L"Error: %ld: Could not create %ls\n", result,  sub_key);
        }
    }

    return key;
}

static BOOL write_reg_key(HKEY hkey, LPCWSTR name, LPCWSTR value)
{
    LONG result;
    DWORD type;
    DWORD size;

    result = RegQueryValueExW(hkey, name, 0, &type, 0, &size);
    if (result == ERROR_FILE_NOT_FOUND ) {
        type = REG_SZ;
        result = RegSetValueExW(hkey,
                                name,
                                0,
                                type,
                                (LPBYTE)value,
                                (DWORD)(wcslen(value) + 1) * sizeof(WCHAR));
        if (result == ERROR_SUCCESS)
            return TRUE;
        else
            return FALSE;
    } else if (result == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_NONE) {
            result = RegSetValueExW(hkey,
                                    name,
                                    0,
                                    type,
                                    (LPBYTE)value,
                                    (DWORD)(wcslen(value) + 1) * sizeof(WCHAR));
            if (result == ERROR_SUCCESS) {
                return TRUE;
            } else {
                return FALSE;
            }
        } else if (type == REG_MULTI_SZ) {
            size_t vlen, bufsize;
            LPWSTR s = NULL, buf = NULL;
            LPWSTR value_data = NULL;

            if (!(value_data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + sizeof(WCHAR)))) {
                return FALSE;
            }
            result = RegQueryValueExW(hkey, name, 0, 0, (LPBYTE)value_data, &size);
            if (result != ERROR_SUCCESS) {
                if (value_data != NULL){
                    HeapFree(GetProcessHeap(), 0, value_data);
                }
                return FALSE;
            }
            s = wcsstr(value_data, value);
            if (s == NULL) {
                vlen = wcslen(value);
                bufsize = size + (vlen +1) * sizeof(WCHAR);
                buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufsize);

                memcpy(buf, value, vlen * sizeof(WCHAR));
                buf[vlen++] = L'\0';
                memcpy((buf + vlen), value_data, size * sizeof(WCHAR));
                result = RegSetValueExW(hkey, name, 0, type, (LPBYTE)buf, (DWORD)bufsize);
                HeapFree(GetProcessHeap(), 0, buf);
                if (result != ERROR_SUCCESS) {
                    if (value_data != NULL)
                        HeapFree(GetProcessHeap(), 0, value_data);
                    return FALSE;
                }
            }
            if (value_data != NULL){
                HeapFree(GetProcessHeap(), 0, value_data);
            }
        }
    }
    return TRUE;
}

static BOOL linux_has_font (LPCWSTR font)
{
    DWORD c = 0;
    LSTATUS lResult;
    WCHAR name[MAX_PATH];
    DWORD name_size;
    DWORD i, size, type;
    LPBYTE value = NULL;
    LPWSTR s;
    BOOL found = FALSE;

    HKEY key = open_reg_key(HKEY_LOCAL_MACHINE,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Fonts");
    lResult = RegQueryInfoKeyW(key,    // key handle
                               NULL,    // buffer for class name
                               NULL,    // size of class string
                               NULL,    // reserved
                               NULL,    // number of subkeys
                               NULL,    // longest subkey size
                               NULL,    // longest class string
                               &c,      // number of values for this key
                               NULL,    // longest value name
                               NULL,    // longest value data
                               NULL,    // security descriptor
                               NULL);   // last write time
    if (lResult != ERROR_SUCCESS) {
        if (key != NULL)
            RegCloseKey(key);
        return FALSE;
    }

    for(i = 0; i < c; i++) {
        name_size = _countof(name);
        lResult = RegEnumValueW(key, i, name, &name_size, NULL, &type, NULL, &size);
        if (lResult == ERROR_SUCCESS)
        {
            value = malloc(size);
            lResult = RegQueryValueExW(key, name, NULL, &type, value, &size);
            if (lResult != ERROR_SUCCESS) {
                free(value);
                continue;
            }
            s = wcsstr((LPCWSTR)value, font);
            if (s) {
                if (PathFileExistsW((LPCWSTR)value)) {
                    found = TRUE;
                    free(value);
                    break;
                }
            }
            free(value);
        }
    }

    if (value != NULL)
        free(value);

    if (key != NULL)
        RegCloseKey(key);
    return found;
}

static void wine_systemlink_font(LPCWSTR font_file)
{
    int i = 0;
    HKEY key;

    LPCWSTR link_font_names [] = {
        L"Arial",
        L"Courier New",
        L"Lucida Sans Unicode",
        L"MS Sans Serif",
        L"Microsoft Sans Serif",
        L"NSimSun",
        L"SimSun",
        L"Tahoma",
        L"Times New Roman",
        NULL
    };

    key = open_reg_key(HKEY_LOCAL_MACHINE,
                       L"Software\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink");
    for (i = 0; link_font_names[i]; i++) {
        if (write_reg_key(key, link_font_names[i], font_file)) {
            console_printf(L"字体 \"%ls\" 链接为 \"%s\" 文件\n", link_font_names[i], font_file);
        }
    }
    RegCloseKey(key);
}

static void usage(void)
{
    console_printf(L"用法: wine-cn-fonts.exe [选项]...\n");
    console_printf(L"为 wine 配置中文字体\n\n");
    console_printf(L"-h             显示帮助\n");
    console_printf(L"-l <font>      配置<font>为链接字体\n");
    console_printf(L"-r <font>      配置<font>为替换字体\n");
}

int wmain(int argc, wchar_t **argv)
{
    int i;

    SetConsoleOutputCP(65001);

    if (argc == 1){
        for (int i = 0; chinese_linux_fonts[i]; ++i) {
            if (linux_has_font(chinese_linux_fonts[i])) {
                wine_systemlink_font(chinese_linux_fonts[i]);
                break;
            }
        }
        return 0;
    }

    for (i = 1; i < argc; ++i)
    {
        if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"/?") == 0) {
            usage();
            return 0;
        }

        if (wcslen(argv[i]) == 2 && (argv[i][0] == L'-' || argv[i][0] == L'/'))
        {
            switch (towupper(argv[i][1]))
            {
                case L'L':
                    if (++i < argc) {
                        wine_systemlink_font(argv[i]);
                    } else {
                        console_printf(L"require argument for -l option\n");
                        usage();
                        return 1;
                    }
                    break;
                case L'R':
                    if (++i < argc) {
                        console_printf(L"replate font name: %ls\n", argv[i]);
                    } else {
                        console_printf(L"require argument for -r option\n");
                        usage();
                        return 1;
                    }
                    break;
                default:
                    return 1;
            }
        }
    }

    return 0;
}
