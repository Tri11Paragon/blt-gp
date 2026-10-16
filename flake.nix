{
  description = "C++17 / Python development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs {
        inherit system;
        config = {
          allowUnfree = true;
          segger-jlink.acceptLicense = true;
        };
      };

      python = pkgs.python314.withPackages (python-pkgs: [
        python-pkgs.pybox2d
        python-pkgs.numpy
        python-pkgs.matplotlib
        python-pkgs.seaborn
        python-pkgs.tqdm
        python-pkgs.opencv4
        python-pkgs.torchWithRocm
      ]);
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = [
          python
        ];

        buildInputs = with pkgs; [
          cmake
          gcc
          clang
          emscripten
          ninja
          renderdoc
          valgrind
          gtest
          mold
        ];

        propagatedBuildInputs = with pkgs; [
            libx11
            libx11.dev
            libxcursor
            libxcursor.dev
            libxext
            libxext.dev
            libxinerama
            libxinerama.dev
            libxrandr
            libxrandr.dev
            libxrender
            libxrender.dev
            libxcb
            libxcb.dev
            libxi
            libxi.dev

            harfbuzz
            harfbuzz.dev
            zlib
            zlib.dev
            bzip2
            bzip2.dev
            pngpp
            brotli
            brotli.dev
            pulseaudio.dev

            git
            libGL
            libGL.dev
            glfw
            openmpi

            abseil-cpp
            protobuf
            nlohmann_json
            curl
        ];

        LD_LIBRARY_PATH = "/run/opengl-driver/lib:/run/opengl-driver-32/lib";

        shellHook = ''
          mkdir -p /opt/compilers/current/

          ln -sfn ${pkgs.gcc}/bin/gcc /opt/compilers/current/gcc
          ln -sfn ${pkgs.gcc}/bin/g++ /opt/compilers/current/g++
          ln -sfn ${pkgs.clang}/bin/clang /opt/compilers/current/clang
          ln -sfn ${pkgs.clang}/bin/clang++ /opt/compilers/current/clang++
          ln -sfn ${pkgs.mold}/bin/mold /opt/compilers/current/mold
          ln -sfn ${pkgs.cmake}/bin/cmake /opt/compilers/current/cmake
          ln -sfn ${pkgs.ninja}/bin/ninja /opt/compilers/current/ninja

          echo "GCC: $(${pkgs.gcc}/bin/gcc --version | head -n 1)"
          echo "Clang: $(${pkgs.clang}/bin/clang --version | head -n 1)"
          echo "CMake: $(${pkgs.cmake}/bin/cmake --version | head -n 1)"
          echo "Python: $(${python}/bin/python3 --version)"
        '';
      };
    };
}