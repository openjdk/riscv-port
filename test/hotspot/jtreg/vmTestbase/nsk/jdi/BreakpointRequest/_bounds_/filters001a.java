/*
 * Copyright (c) 2002, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package nsk.jdi.BreakpointRequest._bounds_;

import nsk.share.*;
import nsk.share.jpda.*;
import nsk.share.jdi.*;

//    THIS TEST IS LINE NUMBER SENSITIVE

/**
 *  <code>filters001a</code> is deugee's part of the test.
 */
public class filters001a {

    static String classToCheck = "filters001b";
    static String objName = "testedObjNULL";
    static filters001b testedObjNULL = null;

    static final String brkptMethodName = "run";
    static final int brkptLineNumber = 75;

    public static void main (String argv[]) {

        // create temporary object to load <filters001b> class
        filters001b tmp = new filters001b("tmp");

        ArgumentHandler argHandler = new ArgumentHandler(argv);
        Log log = new Log(System.err, argHandler);
        IOPipe pipe = argHandler.createDebugeeIOPipe(log);

        pipe.println("ready");

        String instr = pipe.readln();

        if (instr.equals("quit")) {
            log.display("DEBUGEE> completed succesfully.");
            System.exit(Consts.TEST_PASSED + Consts.JCK_STATUS_BASE);
        }

        log.complain("DEBUGEE> unexpected signal of debugger.");
        System.exit(Consts.TEST_FAILED + Consts.JCK_STATUS_BASE);
    }
}

class filters001b extends NamedTask {

    static Log log;
    filters001b(String nameObj) {
        super(nameObj);
    }

    public void run() {
        synchronized  (log) { // brkptLineNumber
            log.display(getName() + "::Breakpoint is reached");
        }
    }

}
