from conan.tools.scm import Git
from conan.tools.files import rm, rmdir


# get recipes from a git repo
class GitRecipes:

    def __init__(self, cf, url, source, target, remotename):
        self.url = url
        self.source = source
        self.target = target
        self.cf = cf
        self.remotename = remotename

        # use git @hey: make this a custom command or something
        rm(self, "*", ".recipes", recursive=True)
        rmdir(self, ".recipes")
        self.git = Git(cf)
        self.git.run(
            cmd="clone --filter=tree:0 --no-checkout --depth 1 --sparse {} {}".format(
                self.url, self.target
            )
        )

        # folder to put recipes in
        self.git.folder = self.target

    def add_checkout(self, recipe):
        self.git.run(
            cmd="sparse-checkout add {}/{}".format(self.source, recipe))

    def checkout(self):
        self.git.run(cmd="checkout")

        # clean up some markdown files
        rm(self, "*", self.target)
        self.cf.run(
            "conan remote add {} {} --force --index=0".format(
                self.remotename, self.target
            )
        )

        # add as remote put this in conanfile
        # self.run("conan remote add conan-ex .recipes --force --index=0")

        # old/alt method
        # add specific recipes
        # self.run(
        #    "conan config install ",
        #    recipe_repo,
        #    " -sf=recipes/corrade -tf=kolbbond-conan-recipes/recipes/corrade",
        # )
        # self.run(
        #    "conan config install https://github.com/kolbbond/conan-center-index.git -sf=recipes/magnum -tf=kolbbond-conan-recipes/recipes/magnum"
        # )

        # add this recipe repo as a remote force
        # self.run(
        #    "conan remote add kolbbond-conan-recipes ~/.conan2/kolbbond-conan-recipes --force --index=0"
        # )

