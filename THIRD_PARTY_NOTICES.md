# Third-party notices

Glide Aholic is authored by Karliky / BugCraft. Its original code is licensed
under GPL-3.0-or-later. This does not change the licenses or ownership of the
components below or of any Guild Wars 2 files supplied by users.

## MinHook

Copyright (C) 2009-2017 Tsuda Kageyu. The vendored implementation matches
MinHook v1.3.4, commit `c3fcafdc10146beb5919319d0683e44e3c30d537`.
Distributed under its BSD-style license.
The complete notice and disclaimer are retained in
`vendor/minhook/LICENSE.txt`; source files retain their original notices,
including those of the bundled disassembler. The full upstream notice includes
Hacker Disassembler Engine32/64, Copyright (c) 2008-2009 Vyacheslav Patkov.
The bundled source headers also preserve their original attribution.

## Archive format reference

The archive decoder in `common/src/inflate.cpp` is implemented for Glide Aholic
under the project license. Its fixed Huffman dictionary consists of format
constants documented by Antoine Hom's
[gw2_reverse research](https://github.com/ahom/gw2_reverse).
This reference does not import that project's decoder or assert a license grant
for its code.

## Microsoft runtime

Windows Release binaries use the static Visual C++ runtime from the v141 toolset.
Microsoft's runtime remains subject to its own terms, rather than this project's
GPL grant. The packages exclude the compiler, SDK and debug runtime files.
See Microsoft's [redistribution documentation](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files)
and [Visual Studio 2017 redistribution list](https://learn.microsoft.com/en-us/visualstudio/releases/2017/2017-redistribution-vs).

## Original game and middleware

Guild Wars 2 executables, archives, extracted assets and proprietary middleware
are not covered by this project's GPL license and are not included in the source
distribution. Users must supply the matching original files and any documented
runtime dependencies separately.

Game artwork shown in documentation screenshots belongs to its respective owners
and is not covered by the project's GPL license.
