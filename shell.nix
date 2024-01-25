with import <nixpkgs> {};
stdenvNoCC.mkDerivation {
 name = "qk1";
 buildInputs = with pkgs; [
  SDL2
  openal
  pkg-config
  meson
  ninja
  gcc13
 ];
}
