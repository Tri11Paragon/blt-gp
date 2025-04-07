{ pkgs ? (import <nixpkgs> {
    config.allowUnfree = true;
    config.segger-jlink.acceptLicense = true;
}), ... }:
pkgs.mkShell
{
	buildInputs = with pkgs; [
		cmake
		gcc
		clang
		emscripten
		ninja
		renderdoc
		valgrind
		opentelemetry-cpp    
        opentelemetry-cpp.dev
	];
	nativeBuildInputs = with pkgs; [
	    pkg-config
	    abseil-cpp
	    opentelemetry-cpp
        opentelemetry-cpp.dev
	];
	propagatedBuildInputs = with pkgs; [
        gtest
        gtest.dev
        grpc
        protobuf
		curl
		abseil-cpp
		opentelemetry-cpp
		opentelemetry-cpp.dev
		opentelemetry-collector
		protobufc
		protobufc.dev
	];
	LD_LIBRARY_PATH="/run/opengl-driver/lib:/run/opengl-driver-32/lib";
}
