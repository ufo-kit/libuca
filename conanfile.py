from conans import ConanFile, CMake, tools
from conan.tools.microsoft.visual import is_msvc

class UcaConan(ConanFile):
    name = "libuca"
    version = "2.3.0"
    license = "LGPL-2.1"
    author = "Marius Elvert marius.elvert@softwareschneiderei.de"
    url = "https://github.com/ufo-kit/libuca"
    description = "GLib-based C library for unified camera access ."
    topics = ("utilities",)
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = {"shared":True}
    generators = "cmake"
    exports_sources = "src/*", "include/*", "test/*", "bin/*", "plugins/*", "CMakeLists.txt", "package.sh.in"
    requires = "glib/2.75.0", "libtiff/4.4.0",

    def _configured_cmake(self):
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        return cmake        

    def build(self):
        self._configured_cmake().build()

    def package(self):
        self._configured_cmake().install()

    def package_info(self):
        self.cpp_info.libs = ["uca"]
        if is_msvc(self):
            self.cpp_info.defines = ["UCA_API_MSVC_IMPORT"]

    def imports(self):
        self.copy("*.dll", "bin", "bin")
        self.copy("*.dylib", "lib", "lib")
        
    def deploy(self):
        self.copy("*.exe")
        self.copy("*.dll")
        self.copy_deps("*.dll")
