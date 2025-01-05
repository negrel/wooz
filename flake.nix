{
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, flake-utils, ... }:
    let
      outputsWithoutSystem = { };
      outputsWithSystem = flake-utils.lib.eachDefaultSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
          lib = pkgs.lib;
          buildInputs = with pkgs; [
            meson
            pkg-config
            ninja
            wayland
            wayland-protocols
            wayland-scanner
            scdoc
          ];
        in {
          packages = {
            default = pkgs.stdenv.mkDerivation {
              name = "wooz";
              version = "0.1.0";

              src = ./.;

              buildInputs = buildInputs;
            };
          };
          devShells = {
            default = pkgs.mkShell {
              buildInputs = buildInputs;
              LD_LIBRARY_PATH = "${lib.makeLibraryPath buildInputs}";
            };
          };
        });
    in outputsWithSystem // outputsWithoutSystem;
}
