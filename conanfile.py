from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain
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
    generators = "CMakeDeps"
    exports_sources = "src/*", "include/*", "test/*", "bin/*", "plugins/*", "CMakeLists.txt", "package.sh.in"
    requires = "glib/2.81.0", "libtiff/4.4.0", 
    tool_requires = "glib/2.81.0", 

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["WITH_PYTHON_MULTITHREADING"] = False
        tc.variables["WITH_GIR"] = False
        tc.variables["WITH_GUI"] = False
        tc.variables["WITH_TOOLS"] = True
        tc.variables["WITH_TIFF"] = True
        tc.variables["USE_FIND_PACKAGE_FOR_GLIB"] = True
        tc.generate()

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

    def layout(self):
        cmake_layout(self)
