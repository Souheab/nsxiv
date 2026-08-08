{
  description = "nsxiv - New Suckless X Image Viewer";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      pkgsFor = system: import nixpkgs { inherit system; };
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
          imlib2WithJp2 = pkgs.imlib2Full.overrideAttrs (old: {
            buildInputs = (old.buildInputs or [ ]) ++ [ pkgs.openjpeg ];
            configureFlags = (old.configureFlags or [ ]) ++ [ "--with-j2k" ];
          });
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "nsxiv";
            version = "34";

            src = pkgs.lib.cleanSource ./.;

            buildInputs =
              with pkgs;
              [
                giflib
                libxft
                libexif
                libwebp
              ]
              ++ [ imlib2WithJp2 ]
              ++ pkgs.lib.optional pkgs.stdenv.hostPlatform.isDarwin pkgs.libinotify-kqueue;

            env.NIX_LDFLAGS = pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin "-linotify";

            makeFlags = [ "CC:=$(CC)" ];
            installFlags = [ "PREFIX=$(out)" ];
            installTargets = [ "install-all" ];

            meta = {
              description = "New Suckless X Image Viewer";
              homepage = "https://nsxiv.codeberg.page/";
              license = pkgs.lib.licenses.gpl2Plus;
              mainProgram = "nsxiv";
              platforms = pkgs.lib.platforms.unix;
            };
          };
        }
      );

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${nixpkgs.lib.getExe self.packages.${system}.default}";
          meta.description = "Run nsxiv";
        };
      });

      devShells = forAllSystems (
        system:
        let
          pkgs = pkgsFor system;
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = with pkgs; [
              gnumake
              pkg-config
            ];
          };
        }
      );

      formatter = forAllSystems (system: (pkgsFor system).nixfmt-tree);
    };
}
