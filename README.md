# QOP - The “Quite OK Package Format” for bare bones file packages

Single-file MIT licensed library for C/C++

See [qop.h](https://github.com/phoboslab/qop/blob/master/qop.h) for
the documentation and format specification.

⚠️ This is just a draft. The format is subject to change and the library and
conversion tool is missing error checks.

QOP is a like a minimal version of TAR-Archives. It only stores the actual file
data, file path and hashes (of file paths) for quick lookup.

The header for QOP is stored at the end of the file. This makes it possible to
concatenate a QOP archive to any other file and still being load it without
scanning for a signature.

E.g. to attach a QOP archive to an executable:

`cat game_binary game_assets.qop > game_with_assets`

See [example.c](https://github.com/phoboslab/qop/blob/master/example.c) for how
to open the archive attached to the executable, find a file and print its 
contents. The example can be compiled with and tested with:

`make example && ./example_with_archive`
