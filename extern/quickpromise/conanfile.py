from conans import ConanFile, tools
import os
import json
import platform
import subprocess

class QuickPromiseConan(ConanFile):
    name = "quickpromise"
    version = "1.0.8"
    license = "Apache 2.0"
    url = "https://github.com/benlau/quickpromise"
    description = "Promise library for QML"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False]}
    default_options = "shared=False"
    exports = "qconanextra.json"
    exports_sources = "*.pro", "*.pri", "*.js", "*.h" , "*.cpp", "*.qml", "!tests/*", "*/qmldir", "*.qrc"

    def package_id(self):
        version_info = subprocess.check_output(['qmake', '--version'])
        self.info.settings.compiler.qmake_version = version_info

    def make(self, args=[]):
        if platform.system() == "Windows":
            self.run("nmake %s" % " ".join(args))
        else:
            self.run("make %s" % " ".join(args))

    def qmake(self, args=[]):
        cmd = "qmake %s" % (" ".join(args))
        self.run(cmd)

    def build(self):
        args = ["%s/qml/qml.pro" % self.source_folder,
                "INSTALL_ROOT=%s" % self.package_folder]

        if self.options.shared:
            args.append("SHARED=true")
            
        self.qmake(args)
        self.make()

    def package(self):
        self.make(["install"])
        self.copy("*", src="qml/QuickPromise", dst="qml/QuickPromise", keep_path=True)

        qconanextra_json = {}
        if not self.options.shared:
            qconanextra_json["resource"] = "quickpromise"
        qconanextra_json["qml_import_path"] = "qml"

        with open(os.path.join(self.package_folder, "qconanextra.json"), "w") as file:
            file.write(json.dumps(qconanextra_json))
            file.close()

    def package_info(self):
        if not self.options.shared:
            self.cpp_info.libs = ["quickpromise"]
            self.cpp_info.libdirs = [""]
