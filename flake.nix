{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSupportedSystem = f: nixpkgs.lib.genAttrs supportedSystems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      devShells = forEachSupportedSystem({ pkgs }: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            gcc
            gdb
            inotify-tools
            man-pages
            pkg-config
            shaderc
            shader-slang
            valgrind
            vulkan-tools
            vulkan-headers
            vulkan-tools-lunarg
            vulkan-validation-layers
            vulkan-extension-layer
          ];
          shellHook = ''
            export VK_LAYER_PATH="${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d"
            export MANPATH="${pkgs.man-pages}/share/man:''${MANPATH:-}"
          '';
        };
      });
    };
}
