{
  description = "A simple flake that provides a development environment for Shawnee's ETEC 1811 course.";

  inputs = {
    utils.url = "github:numtide/flake-utils";
  };
  outputs = { self, nixpkgs, utils }: utils.lib.eachDefaultSystem (system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShell = pkgs.mkShell {
        buildInputs = with pkgs; [
        ];

        packages = with pkgs; [
          # libclang # i give up on clang for c and clangd, i'll just rough it out with no lsp, not like it oughta matter that much anyway
          gcc
          gdb
          gnumake
        ];
      };
    }
  );
}
