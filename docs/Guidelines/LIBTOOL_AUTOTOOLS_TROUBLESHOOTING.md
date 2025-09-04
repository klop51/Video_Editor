**Symptoms & fixes**
- `libtool: command not found` → add `libtool-bin`.
- `LT_SYS_SYMBOL_USCORE` undefined (libxcrypt) → add `libtool libtool-bin libltdl-dev gperf` and run verify step.
- Always ensure `autoconf automake m4 autoconf-archive` present.

**Verify**
```
autoconf --version && automake --version && libtool --version && libtoolize --version && gperf --version