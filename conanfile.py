from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMake
from conan.tools.microsoft.visual import is_msvc
from conan.errors import ConanInvalidConfiguration

class UcaConan(ConanFile):
    name = "libuca"
    version = "2.4.0"
    license = "LGPL-2.1-or-later"
    author = "Marius Elvert marius.elvert@softwareschneiderei.de"
    url = "https://github.com/ufo-kit/libuca"
    description = "GLib-based C library for unified camera access ."
    topics = ("utilities",)
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "with_tiff": [True, False],
    }
    default_options = {
        "shared": True,
        "with_tiff": True,
    }
    generators = "CMakeDeps"
    exports_sources = "src/*", "test/*", "bin/*", "plugins/*", "cmake/*", "CMakeLists.txt", "package.sh.in", "COPYING"
    def configure(self):
        # These are not applicable for C libraries
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        # GLib does not work when linked statically
        self.options["glib"].shared = True

    def requirements(self):
        if self.options.with_tiff:
            self.requires("libtiff/4.7.2")
        self.requires("glib/2.86.5", transitive_headers=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["WITH_PYTHON_MULTITHREADING"] = False
        tc.variables["WITH_GIR"] = False
        tc.variables["WITH_GUI"] = False
        tc.variables["WITH_TOOLS"] = True
        tc.variables["WITH_TIFF"] = bool(self.options.with_tiff)
        tc.variables["USE_FIND_PACKAGE_FOR_GLIB"] = True
        tc.generate()

    def package_info(self):
        self.cpp_info.libs = ["uca"]
        if is_msvc(self):
            self.cpp_info.defines = ["UCA_API_MSVC_IMPORT"]
            
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def layout(self):
        cmake_layout(self)

    def validate(self):
        if self.dependencies["glib"].options.get_safe("shared", None) == False:
            raise ConanInvalidConfiguration("Static GLib is not supported")
