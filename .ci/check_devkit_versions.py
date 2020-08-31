#
# ----------------------------------------------------------------------------------------------------
#
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#
# ----------------------------------------------------------------------------------------------------

"""
Checks that each devkit mentioned in ci.jsonnet corresponds to a devkit mentioned in make/conf/jib-profiles.js
This is based on pattern matching to avoid the need for a jsonnet and JavaScript parser.
It exists to ensure ci.jsonnet is updated when a relevant devit is updated in jib-profiles.js.
"""

import re
from os.path import dirname, join

repo_dir = dirname(dirname(__file__))
ci_jsonnet_path = join(repo_dir, 'ci.jsonnet')
jib_profiles_path = join(repo_dir, 'make', 'conf', 'jib-profiles.js')

def load_jib_devkits():
    """
    Gets the devkits referred to in jib-profiles.js. For example:

        var devkit_platform_revisions = {
            linux_x64: "gcc10.3.0-OL6.4+1.0",
            macosx: "Xcode12.4+1.0",
            windows_x64: "VS2019-16.9.3+1.0",
            linux_aarch64: "gcc10.3.0-OL7.6+1.0",
            linux_arm: "gcc8.2.0-Fedora27+1.0",
            linux_ppc64le: "gcc8.2.0-Fedora27+1.0",
            linux_s390x: "gcc8.2.0-Fedora27+1.0"
        };

    """
    devkits = set()
    devkit_platform_revisions_re = re.compile(r'var devkit_platform_revisions *= *{([^}]+)}')
    with open(jib_profiles_path) as fp:
        jib_profiles = fp.read()
    devkit_platform_revisions = re.search(r'var devkit_platform_revisions *= *{([^}]+)}', jib_profiles).group(1)
    for entry in devkit_platform_revisions.split(','):
        devkit = entry.strip().split(':')[1]
        devkit = devkit.strip()[1:-1] # strip surrounding ""
        devkits.add(devkit)
    return devkits

def load_ci_devkits():
    """
    Gets the devkits referred to in ci.jsonnet. For example:

        "devkit:gcc10.3.0-OL7.6+1" : "==0"

    """

    devkits = set()
    devkit_re = re.compile(r'"devkit:([^"]+)" *: *"==([\d]+)"')
    with open(ci_jsonnet_path) as fp:
        ci_jsonnet = fp.read()
    for match in devkit_re.finditer(ci_jsonnet):
        devkits.add('.'.join(match.group(1, 2)))
    return devkits

jib_devkits = load_jib_devkits()
ci_devkits = load_ci_devkits()
undefined_devkits = ci_devkits - jib_devkits
if undefined_devkits:
    msg = 'Devkits found in {} that are not defined in {}: {}'.format(ci_jsonnet_path, jib_profiles_path, ', '.join(undefined_devkits))
    raise SystemExit(msg)
