# QOP - The “Quite OK Package Format” for bare bones file packages

Single-file MIT licensed library for C/C++

See [qop.h](https://github.com/phoboslab/qop/blob/master/qop.h) for
the documentation and format specification.

⚠️ This is just a draft. The format is subject to change and the library and
conversion tool is missing error checks.

QOP is a like a minimal version of TAR-Archives. It only stores hashes to 
file paths and the actual file data. Meta information (file attributes) or 
the file names/paths are not stored. So it can not be unpacked directly.

Not storing the file name/path is fine, when all you want to do is load files by
name. E.g. `qop_find("some/file.png")` hashes the complete path and looks up the
file by this hash.

The header for QOP is stored at the end of the file. This makes it possible to
concatenate a QOP archive to any other file and still being load it without
scanning for a signature.

E.g. to attach a QOP archive to an executable:

`cat game_binary game_assets.qop > game_with_assets`

See [example.c](https://github.com/phoboslab/qop/blob/master/example.c) for how
to open the archive attached to the executable, find a file and print its 
contents. The example can be compiled with and tested with:

`make example && ./example_with_archive`
