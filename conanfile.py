# try setting up conan build
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan import tools
from scripts.gitrecipes import GitRecipes

class smgRecipe(ConanFile):
    name = "mypkg"
    version = "0.1"
    package_type = "library"

    # Optional metadata
    license = "<Put the package license here>"
    author = "<Put your name here> <And your email here>"
    url = "<Package recipe repository url here, for issues about the package>"
    description = "<Description of mypkg package here>"
    topics = ("<Put some tag here>", "<here>", "<and here>")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    tools.build.compiler_executables={"cpp" : "/usr/bin/g++-13", "c" : "/usr/bin/gcc-13","fortran":"/usr/bin/gfortran"}

    # add recipe to remote
    def source(self):
        # load git recipes?  
        # setup repo and recipe directory (source), target directory, and remote name
        gr = pkg.GitRecipes(  
            cf=self,  
            url="https://github.com/kolbbond/conan-center-index.git",  
            source="recipes",  
            target=".recipes",  
            remotename="conan_example"
        )  
        gr.add_checkout("corrade")  
        gr.add_checkout("magnum")  

        gr.checkout()  


    def requirements(self):
        self.requires("corrade/[2025.01]")
        self.requires("magnum/[2025.01]")
        self.requires("armadillo/[>0]")

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)
    
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["mypkg"]

