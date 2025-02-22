# Modules for implant.js

- `whoami`: Just outputs the current user info
- `recon`: Performs some basic system enumeration
- `rtport_lpe` (Windows only): LPE exploit leveraging the FFI interfaces

[`types.d.ts`](types.d.ts) provides Intellisense/LSP context to your editor about the different globals exposed from the implant.js runtime to a module, and documents the different functions available.

## Limitations

- A library file can't import other files
- All library imports must be at the beginning of a module

## FFI notes

- String-type values are assumed to be a normal ASCII string with a null terminator. If it some weird magic that has a length passed back through a pointer param (or a wide/UTF-16 string), you must define it as a pointer and manually read/write the memory appropriately.