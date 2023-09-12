final: prev:

let
  testNames = [
    "cpuid"
    "cxx"
    "debugport"
    "emulator"
    "emulator-syscall"
    "exceptions"
    "fpu"
    "lapic-modes"
    "lapic-priority"
    "lapic-timer"
    "msr"
    "pagefaults"
    "sgx"
    "sgx-launch-control"
    "tsc"
    "vmx"
  ];

  # All tests from the CMake build.
  allTests = final.cyberus.guest-tests.tests;
  createIsoMultiboot = final.cyberus.cbspkgs.lib.images.createIsoMultiboot;

  # Generates a bootable ISO for the provided test and makes sure the
  # derivation has the same output structure as the other binary variants.
  isoToCmakeStyleOutputFile = name:
    let
      isoSymlink = final.cyberus.cbspkgs.lib.images.createIsoMultiboot {
        name = "guest-test-${name}-iso-link";
        kernel = "${toString allTests}/${name}.elf64";
        kernelCmdline = "--serial 3f8";
      };
    in
    final.runCommand "${name}.iso" { } ''
      mkdir -p $out
      cp ${toString isoSymlink} $out/${name}.iso
    '';

  # Attribute set that maps the name of each test to a derivation that contains
  # all binary variants of that test. Each inner attribute provides the
  # individual binary variants as passthru attributes.
  testsByName = builtins.foldl'
    (acc: name: acc // {
      "${name}" = extractTestAllVariants name;
    })
    { }
    testNames;

  # Extracts all binary variants of a test from the CMake build of all tests.
  extractTestAllVariants =
    name:

    let
      testByVariant' = testByVariant name;
    in
    final.runCommand "guest-test-${name}-all"
      {
        passthru = testByVariant';
      } ''
      mkdir -p $out
      cp ${toString allTests}/${name}.elf32 $out
      cp ${toString allTests}/${name}.elf64 $out
      cp ${toString testByVariant'.iso}/${name}.iso $out
    '';

  # Creates an attribute set that maps the binary variants of a test to a
  # derivation that only exports that single variant.
  testByVariant = name: {
    elf32 = extractTestVariant ".elf32" name;
    elf64 = extractTestVariant ".elf64" name;
    iso = isoToCmakeStyleOutputFile name;
  };

  # Extracts a single binary variant of a test from the CMake build of all tests.
  extractTestVariant = suffix: name: final.runCommand "guest-test-${name}-${suffix}" { } ''
    mkdir -p $out
    cp ${allTests}/${name}${suffix} $out
  '';

in
{
  tests = ((final.callPackage ./build.nix { }).overrideAttrs { passthru = testsByName; });
}
