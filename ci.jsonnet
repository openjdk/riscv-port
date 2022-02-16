local defs = import "defs.jsonnet";

# https://github.com/graalvm/labs-openjdk-17/blob/master/doc/testing.md
local run_test_spec = "test/hotspot/jtreg/compiler/jvmci";

local labsjdk_builder_version = "0ae6a84d4d7c9a103f696bffbb2ac807575ab28c";

{
    overlay: "de70fec80b4947d9ef25c39e059b01ef38dfc387",
    specVersion: "2",

    OSBase:: {
        path(unixpath):: unixpath,
        exe(unixpath):: unixpath,
        jdk_home(java_home):: self.path(java_home),
        java_home(jdk_home):: self.path(jdk_home),
        copydir(src, dst):: ["cp", "-r", src, dst],
        environment+: {
            JIB_PATH: "${PATH}",
            MAKE : "make",
            ZLIB_BUNDLING: "system",
        },
    },

    Windows:: self.OSBase + {
        path(unixpath):: std.strReplace(unixpath, "/", "\\"),
        exe(unixpath):: self.path(unixpath) + ".exe",
        # https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/xcopy
        copydir(src, dst):: ["xcopy", self.path(src), self.path(dst), "/e", "/i", "/q"],

        downloads+: {
            CYGWIN: {name: "cygwin", version: "3.0.7", platformspecific: true},
        },
        packages : {
            # devkit_platform_revisions in make/conf/jib-profiles.js
            "devkit:VS2019-16.9.3+1" : "==0"
        },
        capabilities+: ["windows"],
        name+: "-windows-cygwin",
        os:: "windows",
        environment+: {
            JIB_PATH: "$CYGWIN\\bin;$PATH",
            ZLIB_BUNDLING: "bundled"
        },
    },
    Linux:: self.OSBase + {
        capabilities+: ["linux"],
        name+: "-linux",
        os:: "linux",
    },
    LinuxDockerAMD64Musl:: self.Linux {
        docker: {
            image: defs.linux_docker_image_amd64_musl
        },
    },

    LinuxAMD64(for_jdk_build):: self.Linux + self.AMD64 {
        docker: {
            image: defs.linux_docker_image_amd64,
            mount_modules: !for_jdk_build # needed for installing the devtoolset package below
        },
        packages : if for_jdk_build then {
            # devkit_platform_revisions in make/conf/jib-profiles.js
            "devkit:gcc10.3.0-OL6.4+1" : "==0"
        } else {
            # When building/testing GraalVM, do not use a devkit as it is known not to
            # work well when dynamically linking libstdc++.
            devtoolset: "==7"
        },
    },
    LinuxAArch64(for_jdk_build):: self.Linux + self.AArch64 {
        packages : if for_jdk_build then {
            # devkit_platform_revisions in make/conf/jib-profiles.js
            "devkit:gcc10.3.0-OL7.6+1" : "==0"
        } else {
            # See GR-26071 as well as comment in self.LinuxAMD64
            devtoolset: "==7"
        },
    },
    Darwin:: self.OSBase + {
        jdk_home(java_home):: java_home + "/../..",
        java_home(jdk_home):: jdk_home + "/Contents/Home",
        packages+: {
            # No need to specify a "make" package as Mac OS X has make 3.81
            # available once Xcode has been installed.
        },
        os:: "darwin",
        name+: "-darwin",
        # JDK 17 switched to Xcode 12.4 which requires 10.15.4
        # at a minimum (GR-32439)
        capabilities+: ["darwin_catalina_7"]
    },

    AMD64:: {
        capabilities+: ["amd64"],
        name+: "-amd64",
    },

    AMD64Musl:: self.AMD64 + {
        name+: "-musl",
    },

    AArch64:: {
        capabilities+: ["aarch64"],
        name+: "-aarch64",
    },

    Eclipse:: {
        downloads+: {
            ECLIPSE: {
                name: "eclipse",
                version: "4.14.0",
                platformspecific: true
            }
        },
        environment+: {
            ECLIPSE_EXE: "$ECLIPSE/eclipse"
        },
    },

    JDT:: {
        downloads+: {
            JDT: {
                name: "ecj",
                version: "4.14.0",
                platformspecific: false
            }
        }
    },

    BootJDK:: {
        downloads+: {
            BOOT_JDK: {
                name : "oraclejdk",
                version : "16+32",
                platformspecific: true
            }
        }
    },

    MuslBootJDK:: {
        downloads+: {
            BOOT_JDK: {
                name: "openjdk",
                version: "16.0.1+9-musl",
                platformspecific: true
            }
        }
    },

    JTReg:: {
        downloads+: {
            JT_HOME: {
                name : "jtreg",
                version : "4.2"
            }
        }
    },

    local setupJDKSources(conf) = {
        run+: [
            # To reduce load, the CI system does not fetch all tags so it must
            # be done explicitly as `build_labsjdk.py` relies on it.
            ["git", "fetch", "--tags"],
        ] + (if conf.os == "windows" then [
            # Need to fix line endings on Windows to satisfy cygwin
            # https://stackoverflow.com/a/26408129
            ["set-export", "JDK_SRC_DIR", "${PWD}\\..\\jdk"],
            ["git", "clone", "--quiet", "-c", "core.autocrlf=input", "-c", "gc.auto=0", ".", "${JDK_SRC_DIR}"],
        ] else [
            ["set-export", "JDK_SRC_DIR", "${PWD}"],
        ]) + [
            ["set-export", "JDK_SUITE_DIR", "${JDK_SRC_DIR}"]
        ],
    },

    Build(conf, is_musl_build):: conf + setupJDKSources(conf) + (if is_musl_build then self.MuslBootJDK else self.BootJDK) + {
        packages+: if !is_musl_build then {
            # GR-19828
            "00:pip:logilab-common ": "==1.4.4",
            "01:pip:astroid" : "==1.1.0",
            "pip:pylint" : "==1.1.0",
        } else {},
        name: "build-jdk" + conf.name,
        timelimit: "1:50:00",
        diskspace_required: "10G",
        logs: ["*.log"],
        targets: ["gate"],

        local build_labsjdk(jdk_debug_level, java_home_env_var) = [
            ["set-export", java_home_env_var, conf.path("${PWD}/../%s-java-home" % jdk_debug_level)],
            ["python3", "-u", conf.path("${LABSJDK_BUILDER_DIR}/build_labsjdk.py"),
                "--boot-jdk=${BOOT_JDK}",
                "--clean-after-build",
                "--bundles=" + (if is_musl_build then "only-static-libs" else "jdk"),
                "--jdk-debug-level=" + jdk_debug_level,
                "--test=" + run_test_spec,
                "--java-home-link-target=${%s}" % java_home_env_var,
                "${JDK_SRC_DIR}"
            ],
            (if !is_musl_build then [conf.exe("${%s}/bin/java" % java_home_env_var), "-version"] else ["echo"])
        ],

        run+: (if !is_musl_build then [
             # Checks that each devkit mentioned in this file corresponds to a devkit in make/conf/jib-profiles.js
            ["python3", "-u", conf.path("${PWD}/.ci/check_devkit_versions.py")],

            # Run some basic mx based sanity checks. This is mostly to ensure
            # IDE support does not regress.
            ["set-export", "JAVA_HOME", "${BOOT_JDK}"],
            ["mx", "-p", "${JDK_SUITE_DIR}", "checkstyle"],
            ["mx", "-p", "${JDK_SUITE_DIR}", "eclipseinit"],
            ["mx", "-p", "${JDK_SUITE_DIR}", "canonicalizeprojects"],
        ] else []) + [
            ["set-export", "LABSJDK_BUILDER_DIR", conf.path("${PWD}/../labsjdk-builder")],
            ["git", "clone", "-c", "core.autocrlf=input", "-c", "gc.auto=0", defs.labsjdk_builder_url, "${LABSJDK_BUILDER_DIR}"],
            ["git", "-C", "${LABSJDK_BUILDER_DIR}", "checkout", labsjdk_builder_version],

            # This restricts cygwin to be on the PATH only while using jib.
            # It must not be on the PATH when building Graal.
            ["set-export", "OLD_PATH", "${PATH}"],
            ["set-export", "PATH", "${JIB_PATH}"],
            ["set-export", "JIB_DATA_DIR", conf.path("${PWD}/../jib")],
            ["set-export", "JIB_SERVER", defs.jib_server],
            ["set-export", "JIB_SERVER_MIRRORS", defs.jib_server_mirrors],
        ] +
        build_labsjdk("release", "JAVA_HOME") +
        build_labsjdk("fastdebug", "JAVA_HOME_FASTDEBUG") +
        [
            ["set-export", "PATH", "${OLD_PATH}"],

            # Prepare for publishing
            ["set-export", "JDK_HOME", conf.path("${PWD}/jdk_home")],
            ["cd", "${JAVA_HOME}"],
            conf.copydir(conf.jdk_home("."), "${JDK_HOME}")
        ],

        publishArtifacts+: if !is_musl_build then [
            {
                name: "labsjdk" + conf.name,
                dir: ".",
                patterns: ["jdk_home"]
            }
        ] else [
            # In contrast to the labsjdk-builder repo, the gate in this repo
            # does not bundle the musl static library into the main JDK. That is
            # why the musl static library builder does not publish anything.
            # The musl-based builder in this repo exists solely to ensure
            # the musl build does not regress.
        ],
    },

    # Downstream Graal branch to test against.
    local downstream_branch = "me/GR-36838_intrinsics_update",

    local clone_graal = {
        run+: [
            ["git", "clone", ["mx", "urlrewrite", "https://github.com/graalvm/graal.git"]],
            # Use branch recorded by previous builder or record it now for subsequent builder(s)
            ["test", "-f", "graal.commit", "||", "echo", downstream_branch, ">graal.commit"],
            ["git", "-C", "graal", "checkout", ["cat", "graal.commit"], "||", "true"],
            ["git", "-C", "graal", "rev-list", "-n", "1", "HEAD", ">graal.commit"],            
        ]
    },

    local requireLabsJDK(conf) = {
        requireArtifacts+: [
            {
                name: "labsjdk" + conf.name,
                dir: "."
            }
        ],
        run+: [
            ["set-export", "JAVA_HOME", conf.java_home("${PWD}/jdk_home")]
        ]
    },

    CompilerTests(conf):: conf + clone_graal + requireLabsJDK(conf) + {
        name: "test-compiler" + conf.name,
        timelimit: "1:00:00",
        logs: ["*.log"],
        targets: ["gate"],
        run+: [
            ["mx", "-p", "graal/compiler", "gate", "--tags", "build,test,bootstraplite"]
        ]
    },

    # Build and test JavaScript on GraalVM
    JavaScriptTests(conf):: conf + clone_graal + requireLabsJDK(conf) + {
        local jsvm = ["mx", "-p", "graal/vm",
            "--dynamicimports", "/graal-js,/substratevm",
            "--components=Graal.js,Native Image",
            "--native-images=js"],

        name: "test-js" + conf.name,
        timelimit: "1:00:00",
        logs: ["*.log"],
        targets: ["gate"],
        run+: [
            # Build and test JavaScript on GraalVM
            jsvm + ["build"],
            ["set-export", "GRAALVM_HOME", jsvm + ["graalvm-home"]],
            ["${GRAALVM_HOME}/bin/js", "test/nashorn/opt/add.js"],
        ] +
        if conf.os != "windows" then [
            # Native launchers do not yet support --jvm mode on Windows
            ["${GRAALVM_HOME}/bin/js", "--jvm", "test/nashorn/opt/add.js"]
            ] else []
    },

    # Build LibGraal
    BuildLibGraal(conf):: conf + clone_graal + requireLabsJDK(conf) + {
        name: "build-libgraal" + conf.name,
        timelimit: "1:00:00",
        logs: ["*.log"],
        targets: ["gate"],
        publishArtifacts: [
            {
                name: "libgraal" + conf.name + ".graal.commit",
                dir: ".",
                patterns: ["graal.commit"]
            },
            {
                name: "libgraal" + conf.name,
                dir: ".",
                patterns: ["graal/*/mxbuild"]
            }
        ],
        run+: [
            ["mx", "-p", "graal/vm", "--env", "libgraal",
                "--extra-image-builder-argument=-J-esa",
                "--extra-image-builder-argument=-H:+ReportExceptionStackTraces", "build"],
        ]
    },

    local requireLibGraal(conf) = {
        requireArtifacts+: [
            {
                name: "libgraal" + conf.name + ".graal.commit",
                dir: ".",
                autoExtract: true
            },
            {
                name: "libgraal" + conf.name,
                dir: ".",
                autoExtract: false
            }
        ],
    },

    # Test LibGraal
    TestLibGraal(conf):: conf + clone_graal + requireLabsJDK(conf) + requireLibGraal(conf) {
        name: "test-libgraal" + conf.name,
        timelimit: "1:00:00",
        logs: ["*.log"],
        targets: ["gate"],
        run+: [
            ["unpack-artifact", "libgraal" + conf.name],
            ["mx", "-p", "graal/vm",
                "--env", "libgraal",
                "gate", "--task", "LibGraal"],
        ]
    },

    # Run LabsJDK
    RunJDK(conf):: conf + requireLabsJDK(conf) {
        name: "run-jdk" + conf.name,
        logs: ["*.log"],
        targets: ["gate"],
        run+: [
            [conf.exe("${JAVA_HOME}/bin/java"), "-version"],
        ]
    },

    local build_confs = [
        self.LinuxAMD64(true),
        self.LinuxAArch64(true),
        self.Darwin + self.AMD64,
        self.Windows + self.AMD64
    ],

    local graal_confs = [
        self.LinuxAMD64(false),
        self.LinuxAArch64(false),
        self.Darwin + self.AMD64,
    ],

    local amd64_musl_confs = [
        self.LinuxDockerAMD64Musl + self.AMD64Musl,
    ],

    builds: [ self.Build(conf, is_musl_build=false) for conf in build_confs ] +
            [ self.CompilerTests(conf) for conf in graal_confs ] +

            # GR-25354
            #[ self.JavaScriptTests(conf) for conf in graal_confs ] +

            [ self.BuildLibGraal(conf) for conf in graal_confs ] +
            [ self.TestLibGraal(conf) for conf in graal_confs ] +

            [ self.Build(conf, is_musl_build=true) for conf in amd64_musl_confs ] +

            # GR-20001 prevents reliable Graal testing on Windows
            # but we want to "require" the JDK artifact so that it
            # is uploaded.
            [ self.RunJDK(self.Windows + self.AMD64) ]
}
