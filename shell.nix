with import <nixpkgs> {};
stdenv.mkDerivation {
 name = "qk1";
 buildInputs = with pkgs; [
  SDL2
  pkg-config
 ];
}
