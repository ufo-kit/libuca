from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain

class UcaConan(ConanFile):
    name = "libuca"
    version = "2.4.0"
    license = "LGPL-2.1"
    author = "Marius Elvert <marius.elvert@softwareschneiderei.de>"
    url = "https://github.com/ufo-kit/libuca"
    description = "GLib-based C library for unified camera access."
    topics = ("camera", "glib", "image-processing")
    
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    exports_sources = "src/*", "include/*", "test/*", "bin/*", "plugins/*", "CMakeLists.txt", "package.sh.in"
    
    requires = (
        "glib/2.75.0",
        "libtiff/4.4.0"
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if not self.options.shared:
            self.options["glib"].shared = False
            self.options["libtiff"].shared = False

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        if self.settings.os == "Windows":
            tc.variables["CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["uca"]

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib", dst="lib", src="lib")

    def deploy(self):
        self.copy("*.exe", dst="bin", src="bin")
        self.copy("*.dll", dst="bin", src="bin")