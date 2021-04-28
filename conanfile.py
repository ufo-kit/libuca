from conans import ConanFile, CMake, tools


class UcaConan(ConanFile):
    name = "libuca"
    version = "2.3.0"
    license = "MIT"
    author = "Marius Elvert marius.elvert@softwareschneiderei.de"
    url = "https://github.com/ufo-kit/libuca"
    description = "GLib-based C library for unified camera access ."
    topics = ("utilities",)
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    generators = "cmake"
    exports_sources = "src/*", "include/*", "test/*", "bin/*", "CMakeLists.txt", "package.sh.in"
    requires = "glib/2.68.1",

    def _configured_cmake(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        return cmake

    def build(self):
        self._configured_cmake().build()

    def package(self):
        self._configured_cmake().install()

    def package_info(self):
        self.cpp_info.libs = ["libuca"]

