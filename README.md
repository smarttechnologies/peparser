# PE Parser

This tool started out as a solution of sorts to the question we once had: how to tell if binaries produced by different builds (either clean full builds or rebuilds) were created with the same source files and are effectively equivalent as far as functionality goes. Microsoft compilers don't produce byte for byte compatible code, but the differences can often be ignored. Below is the list of superficial differences this tool can ignore.

- PE timestamp and checksum
- Digital signature directory entry
- Export table timestamp
- Debugger section timestamp
- PDB signature, age and file path
- Resources timestamp
- All file/product versions in VS_VERSION_INFO resource
- Digital signature section
- __FILE__, __DATE__ and __TIME__ macros when they are used as literal strings (can be wide or narrow char)

Known differences not currently ignored:

- MIDL vanity stub for embedded type libraries (contains a timestamp string).
- Occasionally, compiler would change certain offsets or PE section sizes (fill them with more or less zeroes essentially) and would generate consistent offsets in the code section (.text).

See also:
http://stackoverflow.com/questions/1180852/deterministic-builds-under-windows

Comparing binaries in such a way has limited usefulness outside of a few special cases, but it is left here for completeness.

Other functionality includes editing VS_VERSION_INFO structure, exporting resources and PE sections, signing/timestamping and stripping signatures, unattended signing using Extended Validation certificates located on SafeNet USB tokens and checking load-time dependencies. See Usage for more info.

# License

This code is made available under a permissive MIT license. Please refer to the LICENSE file for details.

# Build

Visual Studio 2015

NuGet package manager dependencies:
    boost
    boost_filesystem-vc140
    boost_program_options-vc140
    boost_system-vc140

Open peparser.sln and build.

# Filing bugs

Please use GitHub issue tracker. Search for an existing bug or create a new one and add reproduction steps and a description of what goes wrong. Or fix it and create a pull request.

# Usage

### Common
Applies to most other modes.
```
      --help                This message.
      --silent              Suppress standard output.
      --verbose             Print dynamically ignored ranges and other info.
      --input arg           Input files.
      --output arg          Output file path, if omitted uses standard out.
```
### Info
Works on files provided as input (can handle multiple files).
```
      --info                Print full file information. Returns 0 if all files are
                            valid PE binaries.
      --pdb                 Print pdb path and guid. Returns 0 if all files have
                            debug information.
      --imports             Print a list of imported dlls.
      --signature           Check if binary has a digital signature section (does
                            not validate signature). Returns 0 if all files have a
                            DS section.
      --version-info        Print version.
      --dump-section arg    Dump contents of a named PE section. Takes a single
                            input file.
      --dump-resource arg   Extract a resource by path. See contents of .rsrc
                            section in output of --info for available entries.
```
### Compare
```
      --compare             Compare 2 files disregarding linker timestamp, debug
                            info, digital signature, version info section in
                            resources, __FILE__, __DATE__ and __TIME__ macros when
                            they are used as literal strings.
                              Turn off 'link time code generation' option when
                            building binaries to compare and keep full build path
                            length stable between builds. If done right, rebuilds
                            with the same source will be flagged as 'functionally
                            equivalent'.
                              Returns 0 if files are functionally equivalent.

      --r arg               List of ranges to ignore when comparing:
                            {comment1:offset1:size1,comment2:offset2:size2,...}.
      --r1 arg              List of ranges to ignore when comparing (first binary).
      --r2 arg              List of ranges to ignore when comparing (second
                            binary).
      --fast                Use faster comparison. Only static diffs are ignored,
                            no difference percentage.
      --identical           Return 0 only if files are byte-for-byte identical.
      --no-heuristics       Do not try to interpret differences at unknown offsets.
```
### Edit
```
      --delete-resource arg      Delete resource by path.
      --delete-signature         Delete signature.

      --edit-vsversion           Modify VS_VERSIONINFO. Binary must already contain
                                 VS_VERSIONINFO resource.
      --set-version arg          Set new version (both file and product), file is
                                 modified in-place.
      --set-file-version arg     Set new file version.
      --set-product-version arg  Set new product version.
      --set-file-description arg Set file description field.
      --set-internal-name arg    Set internal name field.
      --set-copyright arg        Set copyright field.
      --set-original-name arg    Set original name field.
      --set-product-name arg     Set product name field.
      --no-resource-rebuild      Avoid rebuilding resources, only works with
                                 set-version, set-file-version and
                                 set-product-version and only if there is enough
                                 space in string table to fit new version string.
```
### Sign
```
      --sign                Sign file.

      --cert-store arg      Certificate store. Default value is 'MY'.
      --cert-hash arg       Certificate thumbprint (copy from Details/Thumbprint).
      --timestamp arg       URL to a timestamp server. Repeat for multiple URLs (to
                            be tried if previous URL failed). For example
                            http://timestamp.verisign.com/scripts/timstamp.dll
      --etoken-password arg SafeNet etoken password. Set to avoid GUI password
                            prompt if chosen certificate is on a token.
```
### Dependency check
```
      --check-dependencies  Checks dependencies of a PE binary and everything it
                            links to. Use --verbose for full dependency tree,
                            otherwise prints binaries with missing dependencies
                            only. Returns 2 if a dependency is missing, 1 on any
                            other error and 0 on success. Architecture of this
                            executable (x86/x64) must match architectures of
                            checked binaries.
      --json                Output in json.
      --batch-dlls          Check dependency on all non executables in folders.
                            Executables can't be batched and must be checked one by
                            one in order to set up default activation context. The
                            tool loads dlls in the process, so use matching
                            architecture.
      --reports-dir         directory to dump dependency reports to, creates
                            missing.txt, report.txt (when --verbose is specified),
                            and report.json (when --json is specified).
      --pe-extensions arg   A semi-colon separated list of file extension to check
                            when batching dlls. For example 'dll;cpl;sys'. Omit to
                            test all files except executables.
      --use-system-path     Load system PATH instead of using PATH from current
                            environment.
```
# Examples

### Print general information for a binary
```
peparser.exe --info  peparser.exe
```
```
peparser.exe:

Arch    : 32 bit
Size    : 3327.5 Kb
Version :
PDB     : D:\source\internal-peparser\build\Win32\Debug\peparser.pdb
PDB GUID: {ECD36A2A-342A-435A-AA3C-F0B0EBCD4622}
Signed  : false

Ignored offsets:
        offset: 118        size: 4        PE timestamp
        offset: 168        size: 4        PE checksum
        offset: 1a8        size: 8        Digital signature directory entry
        offset: 2f2d84     size: 4        Debugger timestamp
        offset: 2f2da0     size: 4        Debugger timestamp
        offset: 2fa3e8     size: 53       PDB section
        offset: 2fa3ec     size: 10       PDB 7.00 guid
        offset: 2fa3fc     size: 4        PDB 7.00 age
        offset: 2fa400     size: 3a       PDB 7.00 file path


Imports:
  KERNEL32.dll
  ADVAPI32.dll
  ole32.dll
  CRYPT32.dll
  imagehlp.dll

Delayed imports:

File layout:
offset: 0          size: 33fe00   Whole file
offset: 0          size: 110            DOS Stub
offset: 0          size: 0                      Section: .textbss
offset: 110        size: f4             PE header
offset: 118        size: 4                      PE timestamp
offset: 168        size: 4                      PE checksum
offset: 1a8        size: 8                      Digital signature directory entry
offset: 208        size: 190            Sections directory
offset: 400        size: 2b0400         Section: .text
offset: 2b0800     size: 6f200          Section: .rdata
offset: 2f2d84     size: 4                      Debugger timestamp
offset: 2f2da0     size: 4                      Debugger timestamp
offset: 2fa3e8     size: 53                     PDB section
offset: 2fa3ec     size: 10                             PDB 7.00 guid
offset: 2fa3fc     size: 4                              PDB 7.00 age
offset: 2fa400     size: 3a                             PDB 7.00 file path
offset: 31fa00     size: 6600           Section: .data
offset: 326000     size: 1600           Section: .idata
offset: 327600     size: c00            Section: .gfids
offset: 328200     size: 400            Section: .tls
offset: 328600     size: 200            Section: .00cfg
offset: 328800     size: 600            Section: .rsrc
offset: 328970     size: 17d                    Resource: 24/1/1033
offset: 328e00     size: 17000          Section: .reloc
```

### Print full dependency tree
```
peparser.exe --check-dependencies peparser.exe --verbose
```

Legend for plain text output:
- ```[!]``` -- dependency resolution failed
- ```[D]``` -- this is a delay-load dependency
- ```[M]``` -- loaded manifest for that binary, if binary has an SxS manifest and it is not marked as loaded its dependencies will likely be incorrect
- ```name -> path``` -- dependency name in perent's import table -> full path on the file system

```
[ ][ ][ ] peparser.exe
    [ ][ ][ ] KERNEL32.dll -> C:\Windows\SYSTEM32\KERNEL32.DLL
        [ ][ ][ ] api-ms-win-core-rtlsupport-l1-2-0.dll -> C:\Windows\SYSTEM32\ntdll.dll
        [ ][ ][ ] ntdll.dll -> C:\Windows\SYSTEM32\ntdll.dll
        [ ][ ][ ] KERNELBASE.dll -> C:\Windows\SYSTEM32\KERNELBASE.dll
            [ ][ ][ ] ntdll.dll -> C:\Windows\SYSTEM32\ntdll.dll
            [ ][D][ ] ext-ms-win-advapi32-registry-l1-1-0.dll -> C:\Windows\SYSTEM32\ADVAPI32.dll
[remaining 450Kb of text are skipped]
```

### Print full dependency tree in JSON

```
peparser.exe --check-dependencies peparser.exe --verbose --json > dependencies.json
```

Outputs JSON object with the following structure:

```
{
      "type": "singlefile"
    , "resolved": Boolean
    , "id": binary path as specified on cmd line
    , "binaries" :
        [
            {
                  "id": full resolved binary path
                , "resolved": Boolean
                , "manifest": Boolean, true if binary has a SxS manifest and it was loaded successfully
                , "imports":
                    [
                        {
                            "delayed": Boolean, true if import is delay loaded
                            "name": import name as specified in parent's import table
                            "id": full resolved binary path if found, same as name otherwise. Resolved import will have its own entry in "binaries" collection
                        }, ...
                    ]
            }, ...
        ]
}
```

When batch-dlls option is used, directory is searched for non-executable PE binaries and the root object has type "cachedump" with all checked dlls and their dependencies listed in "binaries" collection.

### Sign and timestamp with a backup timestamp server

```
peparser.exe --sign --cert-hash "<actual thumbprint of the certificate here>" --timestamp "http://timestamp.verisign.com/scripts/timstamp.dll" --timestamp "http://timestamp.comodoca.com/authenticode"
```

Cert-hash takes certificate thumbprint, currently in the exact format you can see in Windows certificate manager. For example "01 32 45 67 78 90 ab cd ef 01 32 45 67 78 90 ab cd ef 01 32".
Timestamps can be specified multiple times and next server will be tried if the previous one fails.

### Comparing binaries made form the same source between clean rebuilds

```
peparser.exe --compare "peparser - Copy.exe" peparser.exe --verbose
```

Output consists of general info for both files and an equivalency conclusion. In case differences are found they will be listed in the file tree below. Vildly different files will take a long time to process on verbose level and will print a lot of output.

In this example older copy of peparser.exe was compared a new version of itself ater a clean build.

```
peparser - Copy.exe:

Arch    : 32 bit
Size    : 3327.5 Kb
Version :
PDB     : D:\source\internal-peparser\build\Win32\Debug\peparser.pdb
PDB GUID: {ECD36A2A-342A-435A-AA3C-F0B0EBCD4622}
Signed  : false

Ignored offsets:
        offset: 118        size: 4        PE timestamp
        offset: 168        size: 4        PE checksum
        offset: 1a8        size: 8        Digital signature directory entry
        offset: 2f2d84     size: 4        Debugger timestamp
        offset: 2f2da0     size: 4        Debugger timestamp
        offset: 2fa3e8     size: 53       PDB section
        offset: 2fa3ec     size: 10       PDB 7.00 guid
        offset: 2fa3fc     size: 4        PDB 7.00 age
        offset: 2fa400     size: 3a       PDB 7.00 file path


Imports:
  KERNEL32.dll
  ADVAPI32.dll
  ole32.dll
  CRYPT32.dll
  imagehlp.dll

Delayed imports:

peparser.exe:

Arch    : 32 bit
Size    : 3327.5 Kb
Version :
PDB     : D:\source\internal-peparser\build\Win32\Debug\peparser.pdb
PDB GUID: {2C524743-9D20-42D6-97A5-0D3463844DCF}
Signed  : false

Ignored offsets:
        offset: 118        size: 4        PE timestamp
        offset: 168        size: 4        PE checksum
        offset: 1a8        size: 8        Digital signature directory entry
        offset: 2f2d84     size: 4        Debugger timestamp
        offset: 2f2da0     size: 4        Debugger timestamp
        offset: 2fa3e8     size: 53       PDB section
        offset: 2fa3ec     size: 10       PDB 7.00 guid
        offset: 2fa3fc     size: 4        PDB 7.00 age
        offset: 2fa400     size: 3a       PDB 7.00 file path


Imports:
  KERNEL32.dll
  ADVAPI32.dll
  ole32.dll
  CRYPT32.dll
  imagehlp.dll

Delayed imports:


Functionally equivalent.

Difference: 0.00% (0 bytes)

offset: 0          size: 33fe00   File 1
offset: 0          size: 110            DOS Stub
offset: 0          size: 0                      Section: .textbss
offset: 110        size: f4             PE header
offset: 118        size: 4                      PE timestamp
offset: 168        size: 4                      PE checksum
offset: 1a8        size: 8                      Digital signature directory entry
offset: 208        size: 190            Sections directory
offset: 400        size: 2b0400         Section: .text
offset: 2b0800     size: 6f200          Section: .rdata
offset: 2f2d84     size: 4                      Debugger timestamp
offset: 2f2da0     size: 4                      Debugger timestamp
offset: 2fa3e8     size: 53                     PDB section
offset: 2fa3ec     size: 10                             PDB 7.00 guid
offset: 2fa3fc     size: 4                              PDB 7.00 age
offset: 2fa400     size: 3a                             PDB 7.00 file path
offset: 31fa00     size: 6600           Section: .data
offset: 326000     size: 1600           Section: .idata
offset: 327600     size: c00            Section: .gfids
offset: 328200     size: 400            Section: .tls
offset: 328600     size: 200            Section: .00cfg
offset: 328800     size: 600            Section: .rsrc
offset: 328970     size: 17d                    Resource: 24/1/1033
offset: 328e00     size: 17000          Section: .reloc
```

### Dumping resources and PE sections

To extract executable manifest:
```
peparser.exe --dump-resource 24/1 peparser.exe > manifest.xml
```

To dump whole resource section:
```
peparser.exe --dump-section .rsrc peparser.exe > rsrc.dat
```

Use --info command (or any PE editor) to see available resource paths and sections.

### Editing version information
```
peparser.exe --edit-vsversion --set-file-version 1.2.3.4 --set-product-version 1.2.3.5 --set-product-name "PE Parser" "peparser - Copy.exe"
```
