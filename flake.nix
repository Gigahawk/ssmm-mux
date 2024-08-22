{
  description = "Devshell and package definition";

  inputs = {
    nixpkgs.url = "nixpkgs/nixos-unstable";
    flake-utils = {
      url = "github:numtide/flake-utils";
    };
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
      version = builtins.concatStringsSep "." [ "1.1" self.lastModifiedDate ];
    in {
      packages = {
        default = with import nixpkgs { inherit system; };
        stdenv.mkDerivation rec {
          pname = "ssmm-demux";
          inherit version;

          src = ./.;

          nativeBuildInputs = with pkgs; [
            pkg-config
          ];

          buildPhase = ''
            gcc ssmm-demux.c -o ssmm-demux
          '';
          installPhase = ''
            install -m 755 -D -t $out/bin/ ssmm-demux
          '';

          meta = with lib; {
            homepage = "https://github.com/Gigahawk/ssmm-demux";
            description = "Sarugetchu Million Monkeys PSS demuxer";
            license = licenses.gpl3;
            platforms = platforms.all;
          };
        };
      };
      devShell = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          pkg-config
        ];
      };
    });
}