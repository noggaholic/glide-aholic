# MinHook

The 13 vendored implementation files match upstream MinHook v1.3.4, commit
`c3fcafdc10146beb5919319d0683e44e3c30d537`.
`PROVENANCE.json` records their SHA-256 hashes and the compared upstream revision.
The project builds its own static library from these sources.
Copyright notices are retained in all source files, with the main license reproduced in `LICENSE.txt`.

The v141 project suppresses warnings for constructs in the unmodified C dependency: anonymous
unions/structures, bit-field types, integer conversions, empty architecture-specific translation units,
assignment in conditions, the allocator's unused x86 parameter and potential uninitialized values.
These suppressions apply only to this dependency; application code retains `/W4 /WX`.
