# Style and quality checks.
#
# See here for help with configuration:
# - https://github.com/cachix/pre-commit-hooks.nix
# - https://github.com/cachix/pre-commit-hooks.nix/blob/master/modules/pre-commit.nix
# - https://github.com/cachix/pre-commit-hooks.nix/blob/master/modules/hooks.nix

{ pkgs }:

let
  lib = pkgs.lib;
  sources = import ./sources.nix;
  pre-commit-hooks = import sources."pre-commit-hooks.nix";
in
pre-commit-hooks.run {
  src = pkgs.nix-gitignore.gitignoreSource [ ] ../.;

  # Set the pkgs to get the tools for the hooks from.
  tools = pkgs;

  hooks = {
    clang-format = {
      enable = true;
      # Without mkForce this is appended and not replaced.
      types_or = lib.mkForce [ "c" "c++" ];
      excludes = [
        "lib/libc-tiny"
        "lib/libcxx"
      ];
    };

    cmake-format = {
      enable = true;
    };

    deadnix = {
      enable = true;
      excludes = [
        "nix/sources.nix"
      ];
    };

    nixpkgs-fmt = {
      enable = true;
      excludes = [
        "nix/sources.nix"
      ];
    };

    typos = {
      enable = true;
    };
  };

  settings = {
    typos = {
      configPath = ".typos.toml";
    };
  };
}