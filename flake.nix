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
        #default = with import nixpkgs { inherit system; };
        #stdenv.mkDerivation rec {
        #  pname = "zoe_demux";
        #  inherit version;

        #  src = ./.;

        #  nativeBuildInputs = with pkgs; [
        #    pkg-config
        #  ];

        #  buildPhase = ''
        #    gcc zoe_demux.c -o zoe_demux
        #  '';
        #  installPhase = ''
        #    install -m 755 -D -t $out/bin/ zoe_demux
        #  '';

        #  meta = with lib; {
        #    homepage = "https://github.com/Gigahawk/zoe_demux";
        #    description = "Zone of the Enders PSS demuxer";
        #    # TODO: Ask for license?
        #    #license = licenses.gpl3;
        #    platforms = platforms.all;
        #  };
        #};
      };
      devShell = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          pkg-config
        ];
      };
    });
}